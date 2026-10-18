#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <cstring>

// =====================================================
// LED設定
// =====================================================
constexpr uint8_t LED_PIN = 32;

// 通信途絶判定時間
constexpr unsigned long HEARTBEAT_TIMEOUT_MS = 200;

// =====================================================
// 送受信共通の構造体
// =====================================================
struct HeartbeatMessage
{
    char message[16];
    uint32_t sequence;
};

// =====================================================
// 状態変数
// =====================================================
volatile unsigned long lastHeartbeatTime = 0;
volatile bool heartbeatReceived = false;
volatile bool newHeartbeatReceived = false;
volatile uint32_t lastSequence = 0;

bool previousCommunicationState = false;

// =====================================================
// MACアドレス表示
// =====================================================
void printMacAddress(const uint8_t *mac)
{
    Serial.printf(
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
}

// =====================================================
// ESP-NOW受信コールバック
// ESP32 Arduino Core 3.x対応
// =====================================================
void onDataReceive(
    const esp_now_recv_info_t *recvInfo,
    const uint8_t *incomingData,
    int dataLength)
{
    if (recvInfo == nullptr || incomingData == nullptr)
    {
        return;
    }

    if (dataLength != sizeof(HeartbeatMessage))
    {
        return;
    }

    HeartbeatMessage receivedMessage = {};

    memcpy(
        &receivedMessage,
        incomingData,
        sizeof(receivedMessage)
    );

    receivedMessage.message[
        sizeof(receivedMessage.message) - 1
    ] = '\0';

    if (strcmp(receivedMessage.message, "HEARTBEAT") == 0)
    {
        lastHeartbeatTime = millis();
        heartbeatReceived = true;
        newHeartbeatReceived = true;
        lastSequence = receivedMessage.sequence;
    }
}

// =====================================================
// ESP-NOW初期化
// =====================================================
bool initializeEspNow()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    delay(100);

    uint8_t receiverMac[6];

    if (esp_read_mac(receiverMac, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        Serial.println("Failed to read MAC address.");
        return false;
    }

    Serial.print("Receiver MAC address: ");
    printMacAddress(receiverMac);
    Serial.println();

    const esp_err_t initResult = esp_now_init();

    if (initResult != ESP_OK)
    {
        Serial.print("ESP-NOW initialization failed. Error: ");
        Serial.println(initResult);
        return false;
    }

    const esp_err_t callbackResult =
        esp_now_register_recv_cb(onDataReceive);

    if (callbackResult != ESP_OK)
    {
        Serial.print(
            "Receive callback registration failed. Error: "
        );
        Serial.println(callbackResult);
        return false;
    }

    Serial.println("ESP-NOW initialized successfully.");

    return true;
}

// =====================================================
// 通信状態判定
// =====================================================
bool isCommunicationAlive(
    const unsigned long currentTime)
{
    if (!heartbeatReceived)
    {
        return false;
    }

    return (
        currentTime - lastHeartbeatTime
        <= HEARTBEAT_TIMEOUT_MS
    );
}

// =====================================================
// LED制御
// 通信中：点灯
// 通信途絶：消灯
// =====================================================
void updateLed(const bool communicationAlive)
{
    digitalWrite(
        LED_PIN,
        communicationAlive ? HIGH : LOW
    );
}

// =====================================================
// シリアル表示
// =====================================================
void updateSerialOutput(
    const bool communicationAlive)
{
    if (newHeartbeatReceived)
    {
        noInterrupts();

        const uint32_t sequence = lastSequence;
        newHeartbeatReceived = false;

        interrupts();

        Serial.print("HEARTBEAT received. Sequence: ");
        Serial.println(sequence);
    }

    if (
        communicationAlive
        != previousCommunicationState
    )
    {
        previousCommunicationState =
            communicationAlive;

        if (communicationAlive)
        {
            Serial.println(
                "Communication started. LED ON."
            );
        }
        else
        {
            Serial.println(
                "Communication timeout. LED OFF."
            );
        }
    }
}

// =====================================================
// setup
// =====================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_PIN, OUTPUT);

    // 起動直後は通信未確立なので消灯
    digitalWrite(LED_PIN, LOW);

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP-NOW HEARTBEAT RECEIVER");
    Serial.println("========================================");

    if (!initializeEspNow())
    {
        Serial.println("Receiver initialization failed.");
        Serial.println("System stopped with LED OFF.");

        digitalWrite(LED_PIN, LOW);

        while (true)
        {
            delay(1000);
        }
    }

    Serial.println("Waiting for HEARTBEAT messages...");

    Serial.print("Heartbeat timeout: ");
    Serial.print(HEARTBEAT_TIMEOUT_MS);
    Serial.println(" ms");
}

// =====================================================
// loop
// =====================================================
void loop()
{
    const unsigned long currentTime = millis();

    const bool communicationAlive =
        isCommunicationAlive(currentTime);

    updateLed(communicationAlive);

    updateSerialOutput(communicationAlive);

    delay(1);
}