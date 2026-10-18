constexpr uint8_t RELAY_PIN = 33;

void setup()
{
    Serial.begin(115200);
    pinMode(RELAY_PIN, OUTPUT);
}

void loop()
{
    Serial.println("GPIO HIGH");
    digitalWrite(RELAY_PIN, HIGH);
    delay(3000);

    Serial.println("GPIO LOW");
    digitalWrite(RELAY_PIN, LOW);
    delay(3000);
}