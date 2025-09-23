
int pot1 = 0;
int pot2 = 0;
int pot3 = 0;
int pot4 = 0;

void setup() {
}



void loop() {
  pot1 = analogRead(A5);
  Serial.print("Pot 1: ");
  Serial.print(pot1);
  Serial.print("\n");
  pot2 = analogRead(A4);
  Serial.print("Pot 2: ");
  Serial.print(pot2);
  Serial.print("\n");
  pot3 = analogRead(A3);
  Serial.print("Pot 3: ");
  Serial.print(pot3);
  Serial.print("\n");
  pot4 = analogRead(A2);
  Serial.print("Pot 4: ");
  Serial.print(pot4);
  Serial.print("\n");
}