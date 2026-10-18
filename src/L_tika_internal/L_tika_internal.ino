// ESP32 内蔵LEDのLチカ
constexpr uint8_t LED_PIN = 2;

void setup()
{
  // GPIO 2を出力モードに設定
  pinMode(LED_PIN, OUTPUT);
}

void loop()
{
  // LEDを点灯
  digitalWrite(LED_PIN, HIGH);
  delay(1000);

  // LEDを消灯
  digitalWrite(LED_PIN, LOW);
  delay(1000);
}