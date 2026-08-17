/*
 * Befaco - PonyPlay
*/

#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include "pins_arduino.h"
#include <SPI.h>
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "pico/platform.h"
#include <SdFat.h>
#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include <SevSeg.h>
#include <I2S.h>
#include "pico/multicore.h"
#include <array>
#include <vector>
#include <string>
#include <atomic>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "hardware/adc.h"
#include "pico/time.h"

// Debug logging
#ifndef DEBUG
#  define DEBUG 0
#endif

#if DEBUG
#  define LOG(fmt, ...) Serial2.printf(fmt, ##__VA_ARGS__)
#else
#  define LOG(fmt, ...) do {} while (0)
#endif

// Timing constants
constexpr uint32_t AUDIO_SAMPLE_RATE_HZ = 48000;
constexpr size_t RING_BUFFER_SIZE = 32768; // MUST be a power of 2; ~170 ms at 48 kHz stereo 16-bit
constexpr int DECODE_BUFFER_FRAMES = 4096;
constexpr int MAX_BANKS = 16;
constexpr uint32_t LOOP_TOGGLE_HOLD_MS = 3000;
constexpr uint32_t ENCODER_LONG_PRESS_MS = 1500;
constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 2000;
constexpr int ADC_HYSTERESIS = 20;
constexpr int PITCH_POT_CENTER_SNAP_COUNTS = 80;
constexpr uint32_t kMinLengthOutputFrames = 1200u; // ~25 ms at 48 kHz — minimum perceived length
constexpr uint32_t kResamplerUnity = 65536u; // 16.16 fixed-point resampler phase accumulator.
constexpr uint32_t kOneShotFadeFrames = 480u; // ~10 ms at 48 kHz — ramp to silence at one-shot
constexpr uint32_t kGateDecayFrames = 2400u; // ~50 ms at 48 kHz — decay envelope

// Ring-buffer fill passes per loop()
constexpr int kFillIterationsPerLoop = 32;

// CV trigger polarity: false = active-LOW
// Set true if +5V trigger
constexpr bool kCvTriggerOnRise = false;

// ADC constants
constexpr int ADC_MAX_VALUE = 4095;
constexpr int ADC_MIDPOINT = 2048;
constexpr int ADC_RANGE = 4096;
constexpr int LENGTH_POT_FULL_THRESHOLD = 4090;
constexpr int LENGTH_POT_MIN_THRESHOLD = 5;

// Ring buffer fill/read thresholds
constexpr size_t RING_BUFFER_MIN_FREE_BYTES = 256;
constexpr size_t RING_BUFFER_MIN_AVAILABLE_BYTES = 128;
constexpr int FILL_FRAMES_PER_CALL = 64;

// UI timing (ms)
constexpr uint32_t TRIGGER_FLASH_DURATION_MS = 120;
constexpr uint32_t LOOP_TOGGLE_BLINK_DURATION_MS = 800;
constexpr uint32_t LOOP_TOGGLE_BLINK_PERIOD_MS = 60;
constexpr uint32_t BREATH_LUT_STEP_PERIOD_MS = 20;
constexpr uint32_t ENCODER_CLICK_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_DEBOUNCE_INTERVAL_MS = 2;
constexpr uint32_t WATCHDOG_TIMEOUT_MS = 500;

// Encoder switch ISR debounce (microseconds)
constexpr uint32_t ENCODER_SWITCH_DEBOUNCE_US = 15000;

// Quadrature pulses per detent
constexpr int ENCODER_PULSES_PER_DETENT = 4;

// ADC hardware timer interval (microseconds, negative = repeating)
constexpr int32_t ADC_TIMER_INTERVAL_US = -200;

// NeoPixel brightness
constexpr uint8_t NEOPIXEL_BRIGHTNESS = 80;

// I2S buffer configuration
constexpr int I2S_BUFFER_COUNT = 2;
constexpr int I2S_BUFFER_SIZE = 64;

// diagnostics (Core0 only unless noted)
#if DEBUG
static volatile uint32_t g_sdReadCount = 0;
static volatile uint32_t g_sdReadTotalUs = 0;
static volatile uint32_t g_sdReadMaxUs = 0;
static volatile uint32_t g_ringUnderrunCount = 0;
static volatile uint32_t g_fillSkipCount = 0;
static volatile uint32_t g_loopMaxUs = 0;
static volatile uint32_t g_loopCount = 0;
#endif

// Compile-time invariants
static_assert((RING_BUFFER_SIZE & (RING_BUFFER_SIZE - 1)) == 0, "RING_BUFFER_SIZE must be a power of 2 for the ring-mask to work");

// Channel hardware pin configuration
struct ChannelHardwareConfig {
    uint8_t pitchAdcPin;
    uint8_t lengthAdcPin;
    uint8_t buttonPin;
    uint8_t ledPin;
};

constexpr std::array<ChannelHardwareConfig, 2> kChannelHw = {{
    { A4, A7, PIN_BTN_A, PIN_LED_A }, // Channel A
    { A3, A6, PIN_BTN_B, PIN_LED_B }, // Channel B
}};

// 7-segment display (SevSeg library)
SevSeg sevseg;

// A–H then I J L N O P U Y  (letters that render clearly on a single 7-seg digit)
constexpr std::array<uint8_t, 16> kSegmentBankLetterPatterns = {
    0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71, 0x7D, 0x76,  // A B C D E F G H
    0x06, 0x1E, 0x38, 0x54, 0x3F, 0x73, 0x3E, 0x6E   // I J L N O P U Y
};
constexpr uint8_t kSegmentDecimalPointBit = 0x80;

void writeSegmentDisplay(uint8_t segmentBitmask) {
    sevseg.setSegments(&segmentBitmask);
    sevseg.refreshDisplay();
}

// NeoPixel breathing
constexpr std::array<uint8_t, 64> kBreathLut = {
     60,  65,  71,  76,  81,  86,  91,  95,
     99, 103, 106, 109, 111, 113, 114, 115,
    115, 115, 114, 113, 111, 109, 106, 103,
     99,  95,  91,  86,  81,  76,  71,  65,
     60,  55,  49,  44,  39,  34,  29,  25,
     21,  17,  14,  11,   9,   7,   6,   5,
      5,   5,   6,   7,   9,  11,  14,  17,
     21,  25,  29,  34,  39,  44,  49,  55
};

// Audio ring buffer
uint8_t audioRingBuffer[RING_BUFFER_SIZE] __attribute__((aligned(4)));
std::atomic<size_t> ringBufferWriteHead{0};
std::atomic<size_t> ringBufferReadTail{0};

// Trigger ring buffer flush
static std::atomic<bool> g_ringFlushPending{false};
static std::atomic<size_t> g_ringFlushTarget{0};

[[nodiscard]] static inline size_t ringBufferFreeSpace(size_t wHead, size_t rTail) {
    return (rTail - wHead - 1) & (RING_BUFFER_SIZE - 1);
}

[[nodiscard]] static inline size_t ringBufferAvailableBytes(size_t wHead, size_t rTail) {
    return (wHead - rTail) & (RING_BUFFER_SIZE - 1);
}

static inline void writeSampleToRingBuffer(size_t& pos, int16_t sample) {
    audioRingBuffer[pos] = static_cast<uint8_t>(sample & 0xFF);
    pos = (pos + 1) & (RING_BUFFER_SIZE - 1);
    audioRingBuffer[pos] = static_cast<uint8_t>((sample >> 8) & 0xFF);
    pos = (pos + 1) & (RING_BUFFER_SIZE - 1);
}

// WAV file metadata
struct WavFileMetadata {
    uint32_t audioDataByteOffset = 0;
    uint32_t totalFrameCount = 0;
    uint16_t channelCount = 1;
    bool isValid = false;
};

// Sample bank
struct SampleBank {
    char directoryPath[64] = {};
    std::vector<std::string> fileNames;
    std::vector<WavFileMetadata> fileMetadata;

    [[nodiscard]] int fileCount() const { 
        return static_cast<int>(fileNames.size()); 
    }

    void clear() {
        fileNames.clear();
        fileMetadata.clear();
    }

    void addFile(const char* name, const WavFileMetadata& meta) {
        fileNames.emplace_back(name);
        fileMetadata.push_back(meta);
    }
};

std::array<SampleBank, MAX_BANKS> sampleBanks;
int totalBankCount = 0;

// Enums
enum class EncoderEditTarget : uint8_t { Sample, Bank };
enum class PlayMode         : uint8_t { OneShot, Loop, Gate };
enum class ChannelPixelState : uint8_t {
    TriggerFlash,
    LoopToggleBlink,
    LoopPlayingPulse,
    OneShotPlayingPulse,
    GatePlayingPulse,
    StereoPlayingPulse,
    ActiveIdle,
    InactiveIdle
};

// Playback state
struct AudioChannel {
    int currentBankIndex = 0;
    int currentFileIndex = 0;
    FsFile wavFile;
    uint32_t wavDataByteOffset = 0;
    uint32_t totalSourceFrames = 0;
    uint16_t sourceChannelCount = 1;

    uint32_t playbackEndFrame = 0;
    uint32_t currentSourceFrame = 0;
    bool isPlaying = false;
    PlayMode playMode = PlayMode::Loop;
    bool isGateActive = false;
    bool isGateDecaying = false;
    uint32_t gateDecayFramesRemaining = 0;

    uint32_t resamplerPhaseAccumulator = 0;
    uint32_t resamplerPhaseIncrement = kResamplerUnity;
    int16_t decodedFrameBuffer[DECODE_BUFFER_FRAMES];
    uint8_t rawReadBuffer[DECODE_BUFFER_FRAMES * 4];
    int decodedFrameCount = 0;
    int decodedFrameReadIndex = 0;
    int16_t previousSample = 0;
    int16_t nextSample = 0;
    int16_t lastDecodedRightSample = 0;
    int16_t previousRightSample = 0;
    int16_t nextRightSample = 0;
    int16_t lastRenderedRightSample = 0;

    // Hot-swap
    struct PendingSwap {
        FsFile wavFile;
        uint32_t wavDataByteOffset = 0;
        uint32_t totalFrameCount = 0;
        uint16_t channelCount = 1;
        int bankIndex = 0;
        int fileIndex = 0;
    };
    PendingSwap pendingSwap;
    std::atomic<bool> pendingSwapReady{false};
    FsFile deferredClose;
    int pitchPotAdcValue = -1;
    int lengthPotAdcValue = -1;
    int sampleCvAdcValue = -1;
    uint32_t triggerFlashExpiry = 0;
    uint32_t loopToggleFlashExpiry = 0;
    uint32_t buttonPressStartMs = 0;
    bool isButtonHeld = false;
    bool longPressHandled = false;
    bool stereoOutputEnabled = false;
};

static_assert(sizeof(AudioChannel) <= 32768, "AudioChannel exceeds expected per-channel SRAM budget");
AudioChannel channelA;
AudioChannel channelB;
std::array<AudioChannel*, 2> channels = { &channelA, &channelB };

// Global player state
int activeChannelIndex = 0;
EncoderEditTarget encoderEditTarget = EncoderEditTarget::Sample;
uint32_t settingsSaveScheduledAt = 0;
static bool g_stereoSplitToggleHandled = false; // prevents repeated fires while both buttons held

// SD, Enc, But, Neo
SdFs sd;
I2S i2s(OUTPUT);
Adafruit_NeoPixel neoPixels(2, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
Bounce debouncerBtnA, debouncerBtnB, debouncerEncoderSwitch;

// debouncer pointers
std::array<Bounce*, 2> channelDebouncers = { &debouncerBtnA, &debouncerBtnB };

// Rotary encoder
static volatile int8_t g_encoderLastQuadState = 0;
static volatile int g_encoderPulseAccumulator = 0;

static constexpr int8_t kQuadLut[16] = { 0, -1,  1,  0, 1,  0,  0, -1,  -1,  0,  0,  1,  0,  1, -1,  0 };

static void encoderQuadISR() {
    const int8_t cur = static_cast<int8_t>((gpio_get(PIN_ENC_A) << 1) | gpio_get(PIN_ENC_B));
    g_encoderPulseAccumulator += kQuadLut[((g_encoderLastQuadState << 2) | cur) & 0x0F];
    g_encoderLastQuadState = cur;
}

static volatile bool     g_encSwFell          = false;
static volatile bool     g_encSwRose          = false;
static volatile uint32_t g_encSwLastChangeUs  = 0;

static void encoderSwitchISR() {
    const uint32_t now = time_us_32();
    if (now - g_encSwLastChangeUs < ENCODER_SWITCH_DEBOUNCE_US) return;
    g_encSwLastChangeUs = now;
    if (gpio_get(PIN_ENC_SW)) g_encSwRose = true;
    else                       g_encSwFell = true;
}

void initEncoder() {
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    g_encoderLastQuadState = static_cast<int8_t>((gpio_get(PIN_ENC_A) << 1) | gpio_get(PIN_ENC_B));
    attachInterrupt(PIN_ENC_A, encoderQuadISR, CHANGE);
    attachInterrupt(PIN_ENC_B, encoderQuadISR, CHANGE);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    attachInterrupt(PIN_ENC_SW, encoderSwitchISR, CHANGE);
}

// Drain detents
int pollEncoderDelta() {
    noInterrupts();
    const int acc = g_encoderPulseAccumulator;
    int detentDelta = 0;
    if (acc >= 2) {
        detentDelta = (acc + 1) / ENCODER_PULSES_PER_DETENT;
        if (!detentDelta) detentDelta = 1;
    } else if (acc <= -2) {
        detentDelta = (acc - 1) / ENCODER_PULSES_PER_DETENT;
        if (!detentDelta) detentDelta = -1;
    }
    if (detentDelta) g_encoderPulseAccumulator = acc - detentDelta * ENCODER_PULSES_PER_DETENT;
    interrupts();
    return detentDelta;
}

// Hardware ADC
static constexpr uint8_t kAdcGpioPins[5] = { 40, 43, 44, 46, 47 };
static std::atomic<uint16_t> g_adcResults[8] = {};
static repeating_timer_t g_adcRepeatTimer;

static bool adcSampleTimerCallback(repeating_timer_t*) {
    static uint8_t idx = 0;
    const uint8_t ch = static_cast<uint8_t>(kAdcGpioPins[idx] - 40u);
    adc_select_input(ch);
    g_adcResults[ch].store(adc_read(), std::memory_order_relaxed);
    if (++idx >= 5u) idx = 0;
    return true;
}

void initAdcHardware() {
    adc_init();
    for (uint8_t pin : kAdcGpioPins) adc_gpio_init(pin);
    for (uint8_t pin : kAdcGpioPins) {
        const uint8_t ch = static_cast<uint8_t>(pin - 40u);
        adc_select_input(ch);
        g_adcResults[ch].store(adc_read(), std::memory_order_relaxed);
    }
    add_repeating_timer_us(ADC_TIMER_INTERVAL_US, adcSampleTimerCallback, nullptr, &g_adcRepeatTimer);
}

// helper readAnalogInputs
static inline int adcRead(uint8_t gpio) {
    return static_cast<int>(g_adcResults[static_cast<uint8_t>(gpio - 40u)].load(std::memory_order_relaxed));
}

// Declarations
static int16_t readNextSourceSample(AudioChannel& channel);
static void primeResamplerBuffer(AudioChannel& channel);
static bool ensureFileOpen(AudioChannel& channel);
void openChannelFile(AudioChannel& channel, int bankIndex, int fileIndex,
bool isInitialOpen = false);
void startChannelPlayback(AudioChannel& channel);
void triggerChannelPlayback(int channelIndex);
void recalculatePlaybackEndFrame(AudioChannel& channel);
void saveChannelSettings();
void loadChannelSettings();
void fillAudioRingBuffer();

inline void pumpRingBuffer() {
    for (int i = 0; i < kFillIterationsPerLoop; i++) fillAudioRingBuffer();
}

// Pitch pot resampler
[[nodiscard]] uint32_t pitchAdcToPhaseIncrement(int adcValue) {
    const float exponent = (adcValue - static_cast<float>(ADC_MIDPOINT)) / static_cast<float>(ADC_MIDPOINT) * 2.0f;
    const float playbackSpeed = std::clamp(powf(2.0f, exponent), 0.25f, 4.0f);
    return static_cast<uint32_t>(static_cast<float>(kResamplerUnity) * playbackSpeed);
}

// Playback length pot 
void recalculatePlaybackEndFrame(AudioChannel& channel) {
    if (!channel.totalSourceFrames) {
        channel.playbackEndFrame = 0;
        return;
    }

    const int lengthAdcValue = (channel.lengthPotAdcValue < 0) ? ADC_MAX_VALUE : channel.lengthPotAdcValue;
    const float pitchRatio = static_cast<float>(channel.resamplerPhaseIncrement) / static_cast<float>(kResamplerUnity);
    const uint32_t minEndFrame = std::max(static_cast<uint32_t>(1), static_cast<uint32_t>(kMinLengthOutputFrames * pitchRatio));
    uint32_t endFrame;
    if (lengthAdcValue >= LENGTH_POT_FULL_THRESHOLD)
        endFrame = channel.totalSourceFrames;
    else {
        const float normalized = static_cast<float>(lengthAdcValue) / static_cast<float>(ADC_MAX_VALUE);
        endFrame = static_cast<uint32_t>(static_cast<float>(channel.totalSourceFrames) * normalized * normalized * pitchRatio);
    }
    channel.playbackEndFrame = std::clamp(endFrame, minEndFrame, channel.totalSourceFrames);
}

// Settings persistence
static constexpr const char* kSettingsFilePath = "/ponyplay.cfg";

void saveChannelSettings() {
    FsFile file = sd.open(kSettingsFilePath, O_WRONLY | O_CREAT | O_TRUNC);
    if (!file) return;
    // Format: bankA fileA modeA bankB fileB modeB stereoA activeChannel
    file.printf("%d %d %d %d %d %d %d %d\n",
        channelA.currentBankIndex, channelA.currentFileIndex, static_cast<int>(channelA.playMode),
        channelB.currentBankIndex, channelB.currentFileIndex, static_cast<int>(channelB.playMode),
        static_cast<int>(channelA.stereoOutputEnabled), activeChannelIndex);
    file.close();
}

void loadChannelSettings() {
    FsFile file = sd.open(kSettingsFilePath, O_RDONLY);
    if (!file) return;

    char buf[48];
    const int bytesRead = file.read(buf, static_cast<int>(sizeof(buf)) - 1);
    buf[bytesRead] = '\0';
    file.close();

    int bankA = 0, fileA = 0, modeA = 1, bankB = 0, fileB = 0, modeB = 1;
    int stereoA = 0, activeCh = 0;
    sscanf(buf, "%d %d %d %d %d %d %d %d", &bankA, &fileA, &modeA, &bankB, &fileB, &modeB, &stereoA, &activeCh);

    bankA = std::clamp(bankA, 0, std::max(0, totalBankCount - 1));
    bankB = std::clamp(bankB, 0, std::max(0, totalBankCount - 1));
    fileA = std::clamp(fileA, 0, std::max(0, sampleBanks[bankA].fileCount() - 1));
    fileB = std::clamp(fileB, 0, std::max(0, sampleBanks[bankB].fileCount() - 1));

    channelA.currentBankIndex = bankA;
    channelA.currentFileIndex = fileA;
    channelA.playMode = static_cast<PlayMode>(std::clamp(modeA, 0, 2));
    channelA.stereoOutputEnabled = stereoA != 0;
    channelB.currentBankIndex = bankB;
    channelB.currentFileIndex = fileB;
    channelB.playMode = static_cast<PlayMode>(std::clamp(modeB, 0, 2));
    activeChannelIndex = std::clamp(activeCh, 0, 1);
}

// WAV header parser
[[nodiscard]] bool parseWavHeader(FsFile& file,
    uint16_t& outChannelCount,
    uint32_t& outDataByteSize) {
    uint8_t riffHeader[12];
    if (file.read(riffHeader, 12) != 12
        || memcmp(riffHeader, "RIFF", 4)
        || memcmp(riffHeader + 8, "WAVE", 4))
        return false;

    bool foundFmtChunk = false;
    bool foundDataChunk = false;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;

    while (file.available() >= 8 && !foundDataChunk) {
        uint8_t chunkHeader[8];
        if (file.read(chunkHeader, 8) != 8) break;

        const uint32_t chunkSize =
            static_cast<uint32_t>(chunkHeader[4])
            | (static_cast<uint32_t>(chunkHeader[5]) << 8)
            | (static_cast<uint32_t>(chunkHeader[6]) << 16)
            | (static_cast<uint32_t>(chunkHeader[7]) << 24);

        if (!memcmp(chunkHeader, "fmt ", 4)) {
            if (chunkSize < 16) return false;

            uint8_t fmtData[16];
            if (file.read(fmtData, 16) != 16) return false;
            if ((fmtData[0] | (fmtData[1] << 8)) != 1) return false; // PCM only

            outChannelCount = fmtData[2] | (fmtData[3] << 8);
            sampleRate = static_cast<uint32_t>(fmtData[4])
                | (static_cast<uint32_t>(fmtData[5]) << 8)
                | (static_cast<uint32_t>(fmtData[6]) << 16)
                | (static_cast<uint32_t>(fmtData[7]) << 24);
            bitsPerSample = fmtData[14] | (fmtData[15] << 8);

            if (chunkSize > 16) file.seek(file.position() + chunkSize - 16);
            foundFmtChunk = true;

        } else if (!memcmp(chunkHeader, "data", 4)) {
            outDataByteSize = chunkSize;
            foundDataChunk = true;

        } else {
            file.seek(file.position() + chunkSize + (chunkSize & 1));
        }
    }

    return foundFmtChunk && foundDataChunk && bitsPerSample == 16 && sampleRate == 48000;
}

// SD card scan
void scanSdCardForBanks() {
    for (int i = 0; i < totalBankCount; i++) {
        sampleBanks[i].clear();
    }
    totalBankCount = 0;

    auto collectWavFiles = [](SampleBank& bank) {
        FsFile bankDir = sd.open(bank.directoryPath);
        FsFile dirEntry;
        while ((dirEntry = bankDir.openNextFile())) {
            if (!dirEntry.isDirectory()) {
                char entryName[64];
                dirEntry.getName(entryName, sizeof(entryName));
                const int nameLength = strlen(entryName);
                if (entryName[0] != '.' && nameLength > 4 && !strcasecmp(entryName + nameLength - 4, ".wav")) {
                    bank.addFile(entryName, WavFileMetadata{});
                }
            }
            dirEntry.close();
        }
        bankDir.close();
    };

    FsFile rootDir = sd.open("/");
    FsFile dirEntry;
    while ((dirEntry = rootDir.openNextFile()) && totalBankCount < MAX_BANKS) {
        if (dirEntry.isDirectory()) {
            char subdirName[32];
            dirEntry.getName(subdirName, sizeof(subdirName));
            if (subdirName[0] != '.') {
                sampleBanks[totalBankCount].clear();
                snprintf(sampleBanks[totalBankCount].directoryPath, sizeof(sampleBanks[totalBankCount].directoryPath) - 1, "/%s", subdirName);
                collectWavFiles(sampleBanks[totalBankCount]);
                if (sampleBanks[totalBankCount].fileCount()) totalBankCount++;
            }
        }
        dirEntry.close();
    }
    rootDir.close();

    // Sort banks alphabetically so folder names match display letters (A=0, B=1, ...)
    std::sort(sampleBanks.begin(), sampleBanks.begin() + totalBankCount,
              [](const SampleBank& a, const SampleBank& b) {
                  return strcasecmp(a.directoryPath, b.directoryPath) < 0;
              });

    if (!totalBankCount) {
        strcpy(sampleBanks[0].directoryPath, "/");
        collectWavFiles(sampleBanks[0]);
        if (sampleBanks[0].fileCount()) totalBankCount = 1;
    }
}

// Channel file open
void openChannelFile(AudioChannel& channel, int bankIndex, int fileIndex, bool isInitialOpen) {
    if (bankIndex < 0 || bankIndex >= totalBankCount) return;
    if (fileIndex < 0 || fileIndex >= sampleBanks[bankIndex].fileCount()) return;

    WavFileMetadata& wavMeta = sampleBanks[bankIndex].fileMetadata[fileIndex];
    if (!wavMeta.isValid) {
        if (!isInitialOpen) pumpRingBuffer();
        FsFile bankDir = sd.open(sampleBanks[bankIndex].directoryPath);
        FsFile tempFile;
        tempFile.open(&bankDir, sampleBanks[bankIndex].fileNames[fileIndex].c_str(), O_RDONLY);
        bankDir.close();
        if (!tempFile) return;
        if (!isInitialOpen) pumpRingBuffer();
        uint16_t channelCount = 0;
        uint32_t dataByteSize = 0;
        if (parseWavHeader(tempFile, channelCount, dataByteSize)) {
            wavMeta.audioDataByteOffset = tempFile.position();
            wavMeta.totalFrameCount = dataByteSize / (channelCount * 2);
            wavMeta.channelCount = channelCount;
            wavMeta.isValid = true;
        }
        tempFile.close();
        if (!wavMeta.isValid) return;
    }

    if (isInitialOpen) {
        channel.wavFile.close();
        FsFile bankDir = sd.open(sampleBanks[bankIndex].directoryPath);
        channel.wavFile.open(&bankDir, sampleBanks[bankIndex].fileNames[fileIndex].c_str(), O_RDONLY);
        bankDir.close();
        if (!channel.wavFile) return;
        channel.currentBankIndex = bankIndex;
        channel.currentFileIndex = fileIndex;
        channel.wavDataByteOffset = wavMeta.audioDataByteOffset;
        channel.sourceChannelCount = wavMeta.channelCount;
        channel.totalSourceFrames = wavMeta.totalFrameCount;
        channel.currentSourceFrame = 0;
        channel.resamplerPhaseAccumulator = 0;
        channel.decodedFrameCount = 0;
        channel.decodedFrameReadIndex = 0;
        channel.previousSample = 0;
        channel.nextSample = 0;
        recalculatePlaybackEndFrame(channel);
    } else {
        if (channel.pendingSwapReady.load(std::memory_order_relaxed)) {
            channel.pendingSwap.wavFile.close();
            channel.pendingSwapReady.store(false, std::memory_order_relaxed);
        }
        pumpRingBuffer();
        FsFile bankDir = sd.open(sampleBanks[bankIndex].directoryPath);
        channel.pendingSwap.wavFile.open(&bankDir, sampleBanks[bankIndex].fileNames[fileIndex].c_str(), O_RDONLY);
        bankDir.close();
        if (!channel.pendingSwap.wavFile) return;
        channel.pendingSwap.wavDataByteOffset = wavMeta.audioDataByteOffset;
        channel.pendingSwap.totalFrameCount = wavMeta.totalFrameCount;
        channel.pendingSwap.channelCount = wavMeta.channelCount;
        channel.pendingSwap.bankIndex = bankIndex;
        channel.pendingSwap.fileIndex = fileIndex;
        channel.pendingSwapReady.store(true, std::memory_order_release);
    }

}

// Resampler no recursion on the audio thread.
static int16_t __not_in_flash_func(readNextSourceSample)(AudioChannel& channel) {
    const bool stereo2ch = channel.stereoOutputEnabled && channel.sourceChannelCount == 2;
    while (true) {
        if (stereo2ch) {
            if (channel.decodedFrameReadIndex + 1 < channel.decodedFrameCount) {
                int16_t L = channel.decodedFrameBuffer[channel.decodedFrameReadIndex++];
                channel.lastDecodedRightSample = channel.decodedFrameBuffer[channel.decodedFrameReadIndex++];
                return L;
            }
        } else {
            if (channel.decodedFrameReadIndex < channel.decodedFrameCount) {
                int16_t sample = channel.decodedFrameBuffer[channel.decodedFrameReadIndex++];
                channel.lastDecodedRightSample = sample;
                return sample;
            }
        }

        if (channel.currentSourceFrame >= channel.playbackEndFrame) {
            const bool shouldLoop = (channel.playMode == PlayMode::Loop)
                || (channel.playMode == PlayMode::Gate && (channel.isGateActive || channel.isGateDecaying));
            if (shouldLoop) {
                channel.currentSourceFrame = 0;
                channel.decodedFrameCount = 0;
                channel.decodedFrameReadIndex = 0;
                channel.wavFile.seek(channel.wavDataByteOffset);
                continue;
            }
            channel.isPlaying = false;
            channel.lastDecodedRightSample = 0;
            return 0;
        }

        const uint32_t framesUntilEnd = channel.playbackEndFrame - channel.currentSourceFrame;
        const int maxFrames = stereo2ch ? DECODE_BUFFER_FRAMES / 2 : DECODE_BUFFER_FRAMES;
        const int framesToRead = static_cast<int>(std::min(static_cast<uint32_t>(maxFrames), framesUntilEnd));
        if (!framesToRead) {
            channel.isPlaying = false;
            channel.lastDecodedRightSample = 0;
            return 0;
        }

        const int bytesPerFrame = channel.sourceChannelCount * 2;
#if DEBUG
        const uint32_t sdT0 = time_us_32();
#endif
        const int bytesRead = channel.wavFile.read(channel.rawReadBuffer, framesToRead * bytesPerFrame);
#if DEBUG
        const uint32_t sdElapsed = time_us_32() - sdT0;
        g_sdReadCount++;
        g_sdReadTotalUs += sdElapsed;
        if (sdElapsed > g_sdReadMaxUs) g_sdReadMaxUs = sdElapsed;
#endif
        const int framesRead = bytesRead / bytesPerFrame;

        if (!framesRead) {
            const bool shouldLoop = (channel.playMode == PlayMode::Loop)
                || (channel.playMode == PlayMode::Gate && (channel.isGateActive || channel.isGateDecaying));
            if (shouldLoop) {
                channel.currentSourceFrame = 0;
                channel.decodedFrameCount = 0;
                channel.decodedFrameReadIndex = 0;
                channel.wavFile.seek(channel.wavDataByteOffset);
                continue;
            }
            channel.isPlaying = false;
            channel.lastDecodedRightSample = 0;
            return 0;
        }

        if (stereo2ch) {
            for (int i = 0; i < framesRead; i++) {
                memcpy(&channel.decodedFrameBuffer[i * 2], channel.rawReadBuffer + i * 4, 2);
                memcpy(&channel.decodedFrameBuffer[i * 2 + 1], channel.rawReadBuffer + i * 4 + 2, 2);
            }
            channel.decodedFrameCount = framesRead * 2;
        } else {
            for (int i = 0; i < framesRead; i++) {
                memcpy(&channel.decodedFrameBuffer[i], channel.rawReadBuffer + i * bytesPerFrame, 2);
            }
            channel.decodedFrameCount = framesRead;
        }
        channel.decodedFrameReadIndex = 0;
        channel.currentSourceFrame += framesRead;
    }
}

static void primeResamplerBuffer(AudioChannel& channel) {
    channel.decodedFrameCount = 0;
    channel.decodedFrameReadIndex = 0;
    channel.previousSample = 0;
    channel.nextSample = 0;
    channel.lastDecodedRightSample = 0;
    channel.previousRightSample = 0;
    channel.nextRightSample = 0;
    channel.lastRenderedRightSample = 0;
    channel.resamplerPhaseAccumulator = 0;
    channel.nextSample = readNextSourceSample(channel);
    channel.nextRightSample = channel.lastDecodedRightSample;
    channel.previousSample = channel.nextSample;
    channel.previousRightSample = channel.nextRightSample;
    channel.nextSample = readNextSourceSample(channel);
    channel.nextRightSample = channel.lastDecodedRightSample;
}

static void __not_in_flash_func(advanceResamplerPhase)(AudioChannel& channel) {
    channel.resamplerPhaseAccumulator += channel.resamplerPhaseIncrement;
    while (channel.resamplerPhaseAccumulator >= kResamplerUnity && channel.isPlaying) {
        channel.previousSample = channel.nextSample;
        channel.previousRightSample = channel.nextRightSample;
        channel.resamplerPhaseAccumulator -= kResamplerUnity;
        channel.nextSample = readNextSourceSample(channel);
        channel.nextRightSample = channel.lastDecodedRightSample;
    }
}

static int16_t __not_in_flash_func(renderNextOutputSample)(AudioChannel& channel) {
    if (channel.pendingSwapReady.load(std::memory_order_acquire)) {
        channel.deferredClose = std::move(channel.wavFile);
        channel.wavFile = std::move(channel.pendingSwap.wavFile);
        channel.wavDataByteOffset = channel.pendingSwap.wavDataByteOffset;
        channel.totalSourceFrames = channel.pendingSwap.totalFrameCount;
        channel.sourceChannelCount = channel.pendingSwap.channelCount;
        channel.currentBankIndex = channel.pendingSwap.bankIndex;
        channel.currentFileIndex = channel.pendingSwap.fileIndex;
        channel.wavFile.seek(channel.wavDataByteOffset);
        channel.currentSourceFrame = 0;
        channel.resamplerPhaseAccumulator = 0;
        channel.decodedFrameCount = 0;
        channel.decodedFrameReadIndex = 0;
        channel.previousSample = 0;
        channel.nextSample = 0;
        channel.previousRightSample = 0;
        channel.nextRightSample = 0;
        channel.lastDecodedRightSample = 0;
        channel.lastRenderedRightSample = 0;
        recalculatePlaybackEndFrame(channel);
        channel.pendingSwapReady.store(false, std::memory_order_release);
    }

    if (!channel.isPlaying) { channel.lastRenderedRightSample = 0; return 0; }
    const int32_t frac = static_cast<int32_t>(channel.resamplerPhaseAccumulator >> 8);
    int32_t interpolated = static_cast<int32_t>(channel.previousSample) + (((static_cast<int32_t>(channel.nextSample - channel.previousSample)) * frac) >> 8);
    int32_t interpolatedR = static_cast<int32_t>(channel.previousRightSample) + (((static_cast<int32_t>(channel.nextRightSample - channel.previousRightSample)) * frac) >> 8);
    advanceResamplerPhase(channel);

    // One-shot
    if (channel.playMode == PlayMode::OneShot && channel.isPlaying) {
        const uint32_t framesLeft = (channel.currentSourceFrame < channel.playbackEndFrame) ? channel.playbackEndFrame - channel.currentSourceFrame : 0u;
        if (framesLeft < kOneShotFadeFrames) {
            interpolated = interpolated * static_cast<int32_t>(framesLeft) / static_cast<int32_t>(kOneShotFadeFrames);
            interpolatedR = interpolatedR * static_cast<int32_t>(framesLeft) / static_cast<int32_t>(kOneShotFadeFrames);
        }
    }

    // Gate
    if (channel.playMode == PlayMode::Gate && channel.isGateDecaying) {
        if (channel.gateDecayFramesRemaining > 0) {
            interpolated = interpolated * static_cast<int32_t>(channel.gateDecayFramesRemaining) / static_cast<int32_t>(kGateDecayFrames);
            interpolatedR = interpolatedR * static_cast<int32_t>(channel.gateDecayFramesRemaining) / static_cast<int32_t>(kGateDecayFrames);
            --channel.gateDecayFramesRemaining;
        } else {
            channel.isPlaying = false;
            channel.isGateDecaying = false;
            channel.lastRenderedRightSample = 0;
            return 0;
        }
    }

    channel.lastRenderedRightSample = static_cast<int16_t>(std::clamp(interpolatedR, static_cast<int32_t>(-32768), static_cast<int32_t>(32767)));
    return static_cast<int16_t>(std::clamp(interpolated, static_cast<int32_t>(-32768), static_cast<int32_t>(32767)));
}

// Playback helpers
[[nodiscard]] static bool ensureFileOpen(AudioChannel& channel) {
    if (channel.wavFile) return true;
    openChannelFile(channel, channel.currentBankIndex, channel.currentFileIndex);
    return channel.pendingSwapReady.load(std::memory_order_relaxed) || static_cast<bool>(channel.wavFile);
}

void startChannelPlayback(AudioChannel& channel) {
    if (!ensureFileOpen(channel)) return;
    channel.currentSourceFrame = 0;
    channel.wavFile.seek(channel.wavDataByteOffset);
    primeResamplerBuffer(channel);
    channel.isPlaying = true;
    channel.triggerFlashExpiry = millis() + TRIGGER_FLASH_DURATION_MS;
    // Only flush the ring buffer if the other channel is silent
    const bool otherPlaying = (&channel == &channelA) ? channelB.isPlaying : channelA.isPlaying;
    if (!otherPlaying) {
        g_ringFlushTarget.store(
            ringBufferWriteHead.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        g_ringFlushPending.store(true, std::memory_order_release);
        pumpRingBuffer();
    }
}
void triggerChannelPlayback(int channelIndex) {
    startChannelPlayback(*channels[channelIndex]);
}

// Analog input reading
void readAnalogInputs() {
    for (int ch = 0; ch < 2; ch++) {
        AudioChannel& channel = *channels[ch];

        // Pitch pot
        const int rawPitch = ADC_MAX_VALUE - adcRead(kChannelHw[ch].pitchAdcPin);
        const bool centreSnapped = (abs(rawPitch - ADC_MIDPOINT) <= PITCH_POT_CENTER_SNAP_COUNTS);
        const int snappedPitch = centreSnapped ? ADC_MIDPOINT : rawPitch;
        if (abs(snappedPitch - channel.pitchPotAdcValue) > ADC_HYSTERESIS) {
            channel.pitchPotAdcValue = snappedPitch;
            channel.resamplerPhaseIncrement = centreSnapped ? kResamplerUnity : pitchAdcToPhaseIncrement(snappedPitch);
            recalculatePlaybackEndFrame(channel);
        }

        // Length pot
        const int rawLen = ADC_MAX_VALUE - adcRead(kChannelHw[ch].lengthAdcPin);
        if (abs(rawLen - channel.lengthPotAdcValue) > ADC_HYSTERESIS) {
            channel.lengthPotAdcValue = rawLen;
            recalculatePlaybackEndFrame(channel);
        }
    }

    // Sample CV active channel
    AudioChannel& activeChannel = *channels[activeChannelIndex];
    const int sampleCvAdcReading = adcRead(40); // A0 = GPIO40
    if (abs(sampleCvAdcReading - activeChannel.sampleCvAdcValue) > ADC_HYSTERESIS * 4) {
        activeChannel.sampleCvAdcValue = sampleCvAdcReading;
        if (totalBankCount > 0 && sampleBanks[activeChannel.currentBankIndex].fileCount() > 0) {
            const int filesInBank = sampleBanks[activeChannel.currentBankIndex].fileCount();
            const int newFileIndex = std::clamp(sampleCvAdcReading * filesInBank / ADC_RANGE, 0, filesInBank - 1);
            if (newFileIndex != activeChannel.currentFileIndex) {
                const bool wasPlaying = activeChannel.isPlaying;
                openChannelFile(activeChannel, activeChannel.currentBankIndex, newFileIndex);
                if (wasPlaying) startChannelPlayback(activeChannel);
            }
        }
    }
}

// Button handling
void handleButtonInputs() {
    const uint32_t now = millis();

    for (int ch = 0; ch < 2; ch++) {
        AudioChannel& channel = *channels[ch];
        Bounce& debouncer = *channelDebouncers[ch];
        debouncer.update();
        // kCvTriggerOnRise = true: (+5 V = trigger).
        // kCvTriggerOnRise = active-LOW buttons / inverted CV.
        const bool triggered = kCvTriggerOnRise ? debouncer.rose() : debouncer.fell();
        const bool released  = kCvTriggerOnRise ? debouncer.fell() : debouncer.rose();

        if (triggered) {
            if (channel.playMode == PlayMode::Gate) {
                channel.isGateActive = true;
                channel.isGateDecaying = false;
                channel.gateDecayFramesRemaining = 0;
                startChannelPlayback(channel);
            } else {
                triggerChannelPlayback(ch);
            }
            channel.buttonPressStartMs = now;
            channel.isButtonHeld       = true;
            channel.longPressHandled   = false;
        }
        if (released) {
            channel.isButtonHeld = false;
            if (channel.playMode == PlayMode::Gate && channel.isGateActive) {
                channel.isGateActive = false;
                channel.isGateDecaying = true;
                channel.gateDecayFramesRemaining = kGateDecayFrames;
            }
        }
        if (channel.isButtonHeld && !channel.longPressHandled
            && (now - channel.buttonPressStartMs) >= LOOP_TOGGLE_HOLD_MS) {
            channel.longPressHandled = true;
            // Cycle: OneShot → Loop → Gate → OneShot
            switch (channel.playMode) {
                case PlayMode::OneShot: channel.playMode = PlayMode::Loop; break;
                case PlayMode::Loop: channel.playMode = PlayMode::Gate; break;
                case PlayMode::Gate:
                    channel.playMode = PlayMode::OneShot;
                    channel.isGateActive = false;
                    channel.isGateDecaying = false;
                    channel.gateDecayFramesRemaining = 0;
                    break;
            }
            channel.loopToggleFlashExpiry = now + LOOP_TOGGLE_BLINK_DURATION_MS;
            settingsSaveScheduledAt = now + SETTINGS_SAVE_DELAY_MS;
        }
    }

    // simultaneous hold toggle
    if (channels[0]->isButtonHeld && channels[1]->isButtonHeld) {
        const uint32_t bothHeldSince = std::max(channels[0]->buttonPressStartMs,channels[1]->buttonPressStartMs);
        if ((now - bothHeldSince) >= LOOP_TOGGLE_HOLD_MS && !g_stereoSplitToggleHandled) {
            g_stereoSplitToggleHandled = true;
            channelA.stereoOutputEnabled = !channelA.stereoOutputEnabled;
            channelA.pitchPotAdcValue = -1;
            settingsSaveScheduledAt = now + SETTINGS_SAVE_DELAY_MS;
        }
    } else {
        g_stereoSplitToggleHandled = false;
    }
}

// Encoder switch + rotation handling
static uint32_t encoderSwitchPressStartMs = 0;
static bool isEncoderSwitchHeld = false;
static bool encoderSwitchLongFired = false;
static bool encoderSwitchWasClicked = false;
static uint32_t encoderSwitchLastReleaseMs = 0;

void handleEncoderInput() {
    const uint32_t now = millis();
    // Drain ISR flags
    noInterrupts();
    const bool fell = g_encSwFell;
    const bool rose = g_encSwRose;
    g_encSwFell = false;
    g_encSwRose = false;
    interrupts();

    if (fell) {
        encoderSwitchPressStartMs = now;
        isEncoderSwitchHeld = true;
        encoderSwitchLongFired = false;
    }
    if (rose) {
        isEncoderSwitchHeld = false;
        if (!encoderSwitchLongFired) {
            encoderSwitchWasClicked = true;
            encoderSwitchLastReleaseMs = now;
        }
    }

    if (isEncoderSwitchHeld && !encoderSwitchLongFired && (now - encoderSwitchPressStartMs) >= ENCODER_LONG_PRESS_MS) {
        encoderSwitchLongFired = true;
        encoderEditTarget = (encoderEditTarget == EncoderEditTarget::Sample) ? EncoderEditTarget::Bank : EncoderEditTarget::Sample;
    }

    if (encoderSwitchWasClicked && (now - encoderSwitchLastReleaseMs) > ENCODER_CLICK_DEBOUNCE_MS) {
        encoderSwitchWasClicked = false;
        activeChannelIndex = 1 - activeChannelIndex;
    }

    const int encoderDelta = pollEncoderDelta();
    if (!encoderDelta || !totalBankCount) return;

    AudioChannel& activeChannel = *channels[activeChannelIndex];

    switch (encoderEditTarget) {
        case EncoderEditTarget::Bank: {
            const int newBank = (activeChannel.currentBankIndex + encoderDelta + totalBankCount) % totalBankCount;
            const bool wasPlaying = activeChannel.isPlaying;
            activeChannel.currentBankIndex = newBank;
            activeChannel.currentFileIndex = 0;
            openChannelFile(activeChannel, newBank, 0);
            if (wasPlaying) startChannelPlayback(activeChannel);
            break;
        }
        case EncoderEditTarget::Sample: {
            const int filesInBank = sampleBanks[activeChannel.currentBankIndex].fileCount();
            if (!filesInBank) return;
            const int newFile = (activeChannel.currentFileIndex + encoderDelta + filesInBank) % filesInBank;
            const bool wasPlaying = activeChannel.isPlaying;
            openChannelFile(activeChannel, activeChannel.currentBankIndex, newFile);
            if (wasPlaying) startChannelPlayback(activeChannel);
            break;
        }
    }

    settingsSaveScheduledAt = now + SETTINGS_SAVE_DELAY_MS;
}

// Display & LED updates
void updateSegmentDisplay() {
    const AudioChannel& activeChannel = *channels[activeChannelIndex];
    switch (encoderEditTarget) {
        case EncoderEditTarget::Bank: {
            const int idx = std::clamp(activeChannel.currentBankIndex, 0, static_cast<int>(kSegmentBankLetterPatterns.size()) - 1);
            writeSegmentDisplay(kSegmentBankLetterPatterns[idx]);
            break;
        }
        case EncoderEditTarget::Sample: {
            const int fileNumber = activeChannel.currentFileIndex + 1;
            sevseg.setNumber(fileNumber % 10);
            if (fileNumber > 9) {
                uint8_t seg;
                sevseg.getSegments(&seg);
                writeSegmentDisplay(seg | kSegmentDecimalPointBit);
            } else {
                sevseg.refreshDisplay();
            }
            break;
        }
    }
}

void updateChannelLeds() {
    for (int ch = 0; ch < 2; ch++) {
        gpio_put(kChannelHw[ch].ledPin, activeChannelIndex != ch ? 1 : 0);
    }
}

static ChannelPixelState resolveChannelPixelState(const AudioChannel& ch, int channelIndex, uint32_t now) {
    if (channelA.stereoOutputEnabled) {
        if (now < channelA.triggerFlashExpiry) return ChannelPixelState::TriggerFlash;
        if (now < channelA.loopToggleFlashExpiry) return ChannelPixelState::LoopToggleBlink;
        if (channelA.isPlaying) return ChannelPixelState::StereoPlayingPulse;
        return ChannelPixelState::StereoPlayingPulse;
    }
    if (now < ch.triggerFlashExpiry) return ChannelPixelState::TriggerFlash;
    if (now < ch.loopToggleFlashExpiry) return ChannelPixelState::LoopToggleBlink;
    if (ch.isPlaying) {
        switch (ch.playMode) {
            case PlayMode::Loop: return ChannelPixelState::LoopPlayingPulse;
            case PlayMode::Gate: return ChannelPixelState::GatePlayingPulse;
            case PlayMode::OneShot:
            default: return ChannelPixelState::OneShotPlayingPulse;
        }
    }
    if (channelIndex == activeChannelIndex) return ChannelPixelState::ActiveIdle;
    return ChannelPixelState::InactiveIdle;
}

void updateNeoPixels() {
    const uint32_t now = millis();
    for (int i = 0; i < 2; i++) {
        const AudioChannel& channel = *channels[i];
        const ChannelPixelState state = resolveChannelPixelState(channel, i, now);
        uint32_t pixelColor = 0;

        switch (state) {
            case ChannelPixelState::TriggerFlash:
                pixelColor = neoPixels.Color(255, 0, 0);
                break;

            case ChannelPixelState::LoopToggleBlink:
                pixelColor = (now / LOOP_TOGGLE_BLINK_PERIOD_MS & 1) ? neoPixels.Color(0, 0, 220) : 0u;
                break;

            case ChannelPixelState::LoopPlayingPulse: {
                const uint8_t v = kBreathLut[(now / BREATH_LUT_STEP_PERIOD_MS) & 63];
                pixelColor = neoPixels.Color(0, 0, v);
                break;
            }

            case ChannelPixelState::OneShotPlayingPulse: {
                const uint8_t v = kBreathLut[(now / BREATH_LUT_STEP_PERIOD_MS) & 63];
                pixelColor = neoPixels.Color(0, v, 0);
                break;
            }

            case ChannelPixelState::GatePlayingPulse:
                pixelColor = neoPixels.Color(0, 140, 100);
                break;

            case ChannelPixelState::StereoPlayingPulse: {
                const uint8_t v = kBreathLut[(now / BREATH_LUT_STEP_PERIOD_MS) & 63];
                pixelColor = neoPixels.Color(v, 0, v);
                break;
            }

            case ChannelPixelState::ActiveIdle:
                pixelColor = neoPixels.Color(30, 30, 30);
                break;

            case ChannelPixelState::InactiveIdle:
                pixelColor = neoPixels.Color(0, 10, 15);
                break;
        }
        neoPixels.setPixelColor(1 - i, pixelColor);
    }
    neoPixels.show();
}

// Audio ring buffer
void __not_in_flash_func(fillAudioRingBuffer)() {
    const size_t wHead = ringBufferWriteHead.load(std::memory_order_relaxed);
    const size_t rTail = ringBufferReadTail.load(std::memory_order_acquire);
    if (ringBufferFreeSpace(wHead, rTail) < RING_BUFFER_MIN_FREE_BYTES) {
#if DEBUG
        g_fillSkipCount++;
#endif
        return;
    }

    size_t writePos = wHead;
    for (int i = 0; i < FILL_FRAMES_PER_CALL; i++) {
        int16_t leftSample, rightSample;
        if (channelA.stereoOutputEnabled) {
            leftSample  = renderNextOutputSample(channelA);
            rightSample = channelA.lastRenderedRightSample;
        } else {
            leftSample  = renderNextOutputSample(channelB);
            rightSample = renderNextOutputSample(channelA);
        }
        writeSampleToRingBuffer(writePos, leftSample);
        writeSampleToRingBuffer(writePos, rightSample);
    }
    ringBufferWriteHead.store(writePos, std::memory_order_release);
}

// Core1: I2S audio output task
void __not_in_flash_func(core1AudioOutputTask)() {
    while (true) {
        // Flush check
        if (g_ringFlushPending.load(std::memory_order_acquire)) {
            ringBufferReadTail.store( g_ringFlushTarget.load(std::memory_order_relaxed), std::memory_order_release);
            g_ringFlushPending.store(false, std::memory_order_release);
        }

        const size_t wHead = ringBufferWriteHead.load(std::memory_order_acquire);
        const size_t rTail = ringBufferReadTail.load(std::memory_order_relaxed);
        const size_t available = ringBufferAvailableBytes(wHead, rTail);

        if (available >= RING_BUFFER_MIN_AVAILABLE_BYTES) {
            const size_t chunkSize = std::min(RING_BUFFER_MIN_AVAILABLE_BYTES, RING_BUFFER_SIZE - rTail);
            const size_t bytesWritten = i2s.write(&audioRingBuffer[rTail], chunkSize);
            ringBufferReadTail.store( (rTail + bytesWritten) & (RING_BUFFER_SIZE - 1), std::memory_order_release);
        } else {
#if DEBUG
            g_ringUnderrunCount++;
#endif
            tight_loop_contents();
        }
    }
}

// Setup
void setup() {

    #if DEBUG
        Serial2.begin(115200);
    #endif
  
    neoPixels.begin();
    neoPixels.setBrightness(NEOPIXEL_BRIGHTNESS);

    // SevSeg
    byte digitPins[] = { PIN_SEG_DIG };
    byte segmentPins[] = { PIN_SEG_A, PIN_SEG_B, PIN_SEG_C, PIN_SEG_D, PIN_SEG_E, PIN_SEG_F, PIN_SEG_G, PIN_SEG_DP_PIN };
    sevseg.begin(COMMON_ANODE, 1, digitPins, segmentPins, true);

    initEncoder();
    debouncerBtnA.attach(PIN_BTN_A, INPUT_PULLUP);
    debouncerBtnA.interval(BUTTON_DEBOUNCE_INTERVAL_MS);
    debouncerBtnB.attach(PIN_BTN_B, INPUT_PULLUP);
    debouncerBtnB.interval(BUTTON_DEBOUNCE_INTERVAL_MS);

    for (int ch = 0; ch < 2; ch++) {
        gpio_init(kChannelHw[ch].ledPin);
        gpio_set_dir(kChannelHw[ch].ledPin, GPIO_OUT);
        gpio_put(kChannelHw[ch].ledPin, ch == 1 ? 1 : 0);
    }

    SPI.setRX(PIN_SD_MISO);
    SPI.setTX(PIN_SD_MOSI);
    SPI.setSCK(PIN_SD_SCK);
    SPI.begin();

    if (!sd.begin(SdSpiConfig(PIN_SD_CS, DEDICATED_SPI, SD_SCK_MHZ(80), &SPI))) {
        neoPixels.setPixelColor(0, neoPixels.Color(255, 0, 0));
        neoPixels.setPixelColor(1, neoPixels.Color(255, 0, 0));
        neoPixels.show();
        writeSegmentDisplay(0x79); // 'E'
        while (true) tight_loop_contents();
    }

    scanSdCardForBanks();
    if (!totalBankCount) {
        neoPixels.setPixelColor(0, neoPixels.Color(255, 50, 0));
        neoPixels.setPixelColor(1, neoPixels.Color(255, 50, 0));
        neoPixels.show();
        writeSegmentDisplay(0x54); // 'n'
        while (true) tight_loop_contents();
    }

    loadChannelSettings();

    openChannelFile(channelA, channelA.currentBankIndex, channelA.currentFileIndex, true);
    openChannelFile(channelB, channelB.currentBankIndex, channelB.currentFileIndex, true);

    i2s.setBCLK(PIN_I2S_BCLK);
    i2s.setDATA(PIN_I2S_DOUT);
    i2s.setBitsPerSample(16);
    i2s.setBuffers(I2S_BUFFER_COUNT, I2S_BUFFER_SIZE);
    i2s.begin(AUDIO_SAMPLE_RATE_HZ);

    // Hardware ADC timer
    initAdcHardware();

    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
    debouncerBtnA.update();
    debouncerBtnB.update();

    updateSegmentDisplay();
    updateChannelLeds();
    updateNeoPixels();

    multicore_launch_core1(core1AudioOutputTask);
}

#if DEBUG
static uint32_t lastDebugLogMs = 0;
#endif

void loop() {
    watchdog_update();
#if DEBUG
    const uint32_t loopT0 = time_us_32();
#endif
    pumpRingBuffer();
    readAnalogInputs();
    handleButtonInputs();
    pumpRingBuffer();
    if (channelA.deferredClose) channelA.deferredClose.close();
    if (channelB.deferredClose) channelB.deferredClose.close();
    pumpRingBuffer();
    handleEncoderInput();
    pumpRingBuffer();

    const uint32_t now = millis();
    if (settingsSaveScheduledAt && now >= settingsSaveScheduledAt) {
        settingsSaveScheduledAt = 0;
        saveChannelSettings();
        pumpRingBuffer();
    }

    updateSegmentDisplay();
    updateChannelLeds();
    pumpRingBuffer();
    updateNeoPixels();

#if DEBUG
    const uint32_t loopElapsed = time_us_32() - loopT0;
    g_loopCount++;
    if (loopElapsed > g_loopMaxUs) g_loopMaxUs = loopElapsed;

    if (now - lastDebugLogMs >= 500) {
        lastDebugLogMs = now;

        const size_t wH = ringBufferWriteHead.load(std::memory_order_relaxed);
        const size_t rT = ringBufferReadTail.load(std::memory_order_relaxed);
        const size_t ringUsed = ringBufferAvailableBytes(wH, rT);
        const size_t ringFree = ringBufferFreeSpace(wH, rT);

        const uint32_t sdAvgUs = g_sdReadCount ? (g_sdReadTotalUs / g_sdReadCount) : 0;

        LOG("──── JITTER DIAG ────\n");
        LOG("SD: reads=%lu avgUs=%lu maxUs=%lu\n",
            (unsigned long)g_sdReadCount, (unsigned long)sdAvgUs, (unsigned long)g_sdReadMaxUs);
        LOG("RING: used=%u free=%u underruns=%lu fillSkips=%lu\n",
            (unsigned)ringUsed, (unsigned)ringFree,
            (unsigned long)g_ringUnderrunCount, (unsigned long)g_fillSkipCount);
        LOG("LOOP: count=%lu maxUs=%lu\n",
            (unsigned long)g_loopCount, (unsigned long)g_loopMaxUs);

        g_sdReadCount = 0;
        g_sdReadTotalUs = 0;
        g_sdReadMaxUs = 0;
        g_ringUnderrunCount = 0;
        g_fillSkipCount = 0;
        g_loopMaxUs = 0;
        g_loopCount = 0;

        static constexpr const char* kChannelLabels[] = { "A", "B" };
        for (int ch = 0; ch < 2; ch++) {
            const AudioChannel& channel = *channels[ch];
            const float speed = static_cast<float>(channel.resamplerPhaseIncrement) / static_cast<float>(kResamplerUnity);
            LOG("Ch%s: %.3fx file=%d/%d %c %c\n",
                kChannelLabels[ch], speed,
                channel.currentFileIndex + 1,
                sampleBanks[channel.currentBankIndex].fileCount(),
                channel.playMode == PlayMode::Loop ? 'L' : (channel.playMode == PlayMode::Gate ? 'G' : 'O'),
                channel.isPlaying ? 'P' : '-');
        }
        LOG("─────────────────────\n\n");
    }
#endif
}