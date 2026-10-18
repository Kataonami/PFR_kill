#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <cstring>

// =====================================================
// 出力ピン設定
// =====================================================
constexpr uint8_t LED_PIN   = 32;
constexpr uint8_t RELAY_PIN = 33;

// リレーモジュールの入力方式
// HIGH入力でリレーONの場合：true
// LOW入力でリレーONの場合 ：false
constexpr bool RELAY_ACTIVE_HIGH = true;

// 通信途絶判定時間
constexpr unsigned long HEARTBEAT_TIMEOUT_MS = 100;

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
// リレー制御
// =====================================================
void setRelay(const bool relayOn)
{
    if (RELAY_ACTIVE_HIGH)
    {
        digitalWrite(
            RELAY_PIN,
            relayOn ? HIGH : LOW
        );
    }
    else
    {
        digitalWrite(
            RELAY_PIN,
            relayOn ? LOW : HIGH
        );
    }
}

// =====================================================
// リレーを安全状態にする
// =====================================================
void setSafeState()
{
    setRelay(false);
}

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

    if (
        esp_read_mac(
            receiverMac,
            ESP_MAC_WIFI_STA
        ) != ESP_OK
    )
    {
        Serial.println(
            "Failed to read MAC address."
        );

        return false;
    }

    Serial.print("Receiver MAC address: ");
    printMacAddress(receiverMac);
    Serial.println();

    const esp_err_t initResult = esp_now_init();

    if (initResult != ESP_OK)
    {
        Serial.print(
            "ESP-NOW initialization failed. Error: "
        );
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

    Serial.println(
        "ESP-NOW initialized successfully."
    );

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
// リレー出力制御
// =====================================================
void updateRelay(
    const bool communicationAlive)
{
    if (communicationAlive)
    {
        // 通信中：リレーON
        setRelay(true);
    }
    else
    {
        // 通信途絶：リレーOFF
        setRelay(false);
    }
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

        Serial.print(
            "HEARTBEAT received. Sequence: "
        );
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
                "Communication started."
            );
            Serial.println(
                "Relay ON."
            );
        }
        else
        {
            Serial.println(
                "Communication timeout."
            );
            Serial.println(
                "Relay OFF."
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

    // GPIO32は初期化時にLOWに設定し、
    // 以降はスイッチングしない
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pinMode(RELAY_PIN, OUTPUT);

    // 起動直後はリレーOFF
    setSafeState();

    Serial.println();
    Serial.println(
        "========================================"
    );
    Serial.println(
        "ESP-NOW HEARTBEAT RELAY RECEIVER"
    );
    Serial.println(
        "========================================"
    );

    Serial.print("GPIO32 fixed level: ");
    Serial.println("LOW");

    Serial.print("Relay GPIO: ");
    Serial.println(RELAY_PIN);

    Serial.print("Relay active level: ");

    if (RELAY_ACTIVE_HIGH)
    {
        Serial.println("HIGH");
    }
    else
    {
        Serial.println("LOW");
    }

    if (!initializeEspNow())
    {
        Serial.println(
            "Receiver initialization failed."
        );
        Serial.println(
            "System stopped with Relay OFF."
        );

        setSafeState();

        while (true)
        {
            setSafeState();
            delay(1000);
        }
    }

    Serial.println(
        "Waiting for HEARTBEAT messages..."
    );

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

    updateRelay(communicationAlive);

    updateSerialOutput(communicationAlive);

    delay(1);
}