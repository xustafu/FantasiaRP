#define AIC3204

// Workaround for a pico-audio bug: its DMA buffers are int16_t arrays with only 2-byte
// alignment, but the DMA moves 32-bit words, and misaligned DMA accesses on the RP2350
// are lost (input reads all zeros, output stays silent). Declaring them here first with
// 4-byte alignment makes the compiler apply it to the library's definitions.
extern "C"
{
    extern volatile int16_t tx_audio_buffer_0[] __attribute__((aligned(4)));
    extern volatile int16_t tx_audio_buffer_1[] __attribute__((aligned(4)));
    extern volatile int16_t rx_audio_buffer_0[] __attribute__((aligned(4)));
    extern volatile int16_t rx_audio_buffer_1[] __attribute__((aligned(4)));
}

#include <pico-audio.h>

// Audio pass-through (L -> L, R -> R) with a peak meter on each input channel.
// The LEDs work as a VU meter: LEDs 0-1 show the left input, LEDs 2-3 the right input.

// Audio chain
AudioInputI2S input;
AudioOutputI2S output;
AudioAnalyzePeak peakL, peakR;

// Audio connections
AudioConnection patchCord1(input, 0, output, 0); // Left in to left out
AudioConnection patchCord2(input, 1, output, 1); // Right in to right out
AudioConnection patchCord3(input, 0, peakL, 0);  // Left in to left meter
AudioConnection patchCord4(input, 1, peakR, 0);  // Right in to right meter

// VU meter range in dBFS
#define VU_FLOOR_DB -60.0f // below this the LEDs are off
#define VU_SPLIT_DB -20.0f // above this the second LED of the channel starts to light

float levelL = 0.0f;
float levelR = 0.0f;
unsigned long lastPrint = 0;

void setupAudio()
{
    AudioMemory(20);

    // Start audio I/O
    output.begin(6, 7, 5);
    input.begin(7, 6, 4);

    // Workaround for a pico-audio bug: the input DMA is paced by the PIO TX FIFO
    // instead of the RX FIFO, so it can read the RX FIFO while empty.
    // Re-point its DREQ to the RX FIFO without retriggering the channel.
    dma_channel_hw_t *rx = dma_channel_hw_addr(rx_dma_channel);
    rx->al1_ctrl = (rx->al1_ctrl & ~DMA_CH0_CTRL_TRIG_TREQ_SEL_BITS) |
                   ((DREQ_PIO0_RX0 + sm_aic3204) << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);
    Serial.printf("Audio DMA channels - tx: %u rx: %u\n", tx_dma_channel, rx_dma_channel);

    Serial.println("Audio started - pass-through L -> L, R -> R with VU meter");
}

// maps a dB value inside [lowDb, highDb] to a 0-255 brightness
uint8_t dbToBrightness(float db, float lowDb, float highDb)
{
    if (db <= lowDb)
        return 0;
    if (db >= highDb)
        return 255;
    return (uint8_t)((db - lowDb) / (highDb - lowDb) * 255.0f);
}

// lights two LEDs as a small meter for one channel:
// first LED green (quiet signals), second LED red (loud signals)
void showLevel(float peak, uint8_t firstLed)
{
    float db = peak > 0.0f ? 20.0f * log10f(peak) : -100.0f;

    ledsColors[firstLed].red = 0;
    ledsColors[firstLed].green = dbToBrightness(db, VU_FLOOR_DB, VU_SPLIT_DB);
    ledsColors[firstLed].blue = 0;

    ledsColors[firstLed + 1].red = dbToBrightness(db, VU_SPLIT_DB, 0.0f);
    ledsColors[firstLed + 1].green = 0;
    ledsColors[firstLed + 1].blue = 0;
}

void updateAudio()
{
    if (peakL.available())
        levelL = peakL.read();
    if (peakR.available())
        levelR = peakR.read();

    showLevel(levelL, 0);
    showLevel(levelR, 2);

    // print the levels a few times per second to the serial monitor
    if (millis() - lastPrint > 250)
    {
        lastPrint = millis();
        Serial.printf("Peak L: %.4f  R: %.4f\n", levelL, levelR);
    }
}
