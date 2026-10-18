# define BLUE 32

void setup() {
  pinMode(BLUE, OUTPUT);
}

void loop() {
  digitalWrite(BLUE, HIGH);
  delay(500);
  digitalWrite(BLUE, LOW);
  delay(500);
}
