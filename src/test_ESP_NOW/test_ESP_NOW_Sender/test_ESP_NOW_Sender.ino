#include <WiFi.h>
#include <esp_now.h>
#include <esp_mac.h>
#include <cstring>

// =====================================================
// 受信側ESP32のMACアドレス
// =====================================================
uint8_t receiverMacAddress[] = {
    0x20, 0xE7, 0xC8, 0xEC, 0xB1, 0x7C
};

// =====================================================
// 送受信共通の構造体
// =====================================================
struct HeartbeatMessage
{
    char message[16];
    uint32_t sequence;
};

// =====================================================
// 送信設定
// =====================================================
constexpr unsigned long SEND_INTERVAL_MS = 20;

unsigned long lastSendTime = 0;
uint32_t sequenceNumber = 0;

HeartbeatMessage heartbeatMessage;
esp_now_peer_info_t peerInfo;

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
// ESP-NOW送信完了コールバック
// ESP32 Arduino Core 3.x対応
// =====================================================
void onDataSent(
    const wifi_tx_info_t *txInfo,
    esp_now_send_status_t status)
{
    Serial.print("Send callback: ");

    if (status == ESP_NOW_SEND_SUCCESS)
    {
        Serial.println("SUCCESS");
    }
    else
    {
        Serial.println("FAILED");
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

    uint8_t senderMac[6];

    if (esp_read_mac(senderMac, ESP_MAC_WIFI_STA) != ESP_OK)
    {
        Serial.println("Failed to read sender MAC address.");
        return false;
    }

    Serial.print("Sender MAC address: ");
    printMacAddress(senderMac);
    Serial.println();

    Serial.print("Receiver MAC address: ");
    printMacAddress(receiverMacAddress);
    Serial.println();

    const esp_err_t initResult = esp_now_init();

    if (initResult != ESP_OK)
    {
        Serial.print("ESP-NOW initialization failed. Error: ");
        Serial.println(initResult);
        return false;
    }

    const esp_err_t callbackResult =
        esp_now_register_send_cb(onDataSent);

    if (callbackResult != ESP_OK)
    {
        Serial.print(
            "Send callback registration failed. Error: "
        );
        Serial.println(callbackResult);
        return false;
    }

    memset(&peerInfo, 0, sizeof(peerInfo));

    memcpy(
        peerInfo.peer_addr,
        receiverMacAddress,
        6
    );

    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (!esp_now_is_peer_exist(receiverMacAddress))
    {
        const esp_err_t addPeerResult =
            esp_now_add_peer(&peerInfo);

        if (addPeerResult != ESP_OK)
        {
            Serial.print("Failed to add peer. Error: ");
            Serial.println(addPeerResult);
            return false;
        }
    }

    Serial.println("ESP-NOW initialized successfully.");
    Serial.println("Receiver registered as peer.");

    return true;
}

// =====================================================
// ハートビート送信
// =====================================================
void sendHeartbeat()
{
    memset(
        &heartbeatMessage,
        0,
        sizeof(heartbeatMessage)
    );

    strncpy(
        heartbeatMessage.message,
        "HEARTBEAT",
        sizeof(heartbeatMessage.message) - 1
    );

    heartbeatMessage.sequence = sequenceNumber;

    const esp_err_t sendResult =
        esp_now_send(
            receiverMacAddress,
            reinterpret_cast<const uint8_t *>(
                &heartbeatMessage
            ),
            sizeof(heartbeatMessage)
        );

    if (sendResult == ESP_OK)
    {
        Serial.print("HEARTBEAT queued. Sequence: ");
        Serial.println(sequenceNumber);
    }
    else
    {
        Serial.print("Send request failed. Error: ");
        Serial.println(sendResult);
    }

    sequenceNumber++;
}

// =====================================================
// setup
// =====================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("ESP-NOW HEARTBEAT SENDER");
    Serial.println("========================================");

    if (!initializeEspNow())
    {
        Serial.println("Sender initialization failed.");
        Serial.println("Program stopped.");

        while (true)
        {
            delay(1000);
        }
    }

    Serial.print("Send interval: ");
    Serial.print(SEND_INTERVAL_MS);
    Serial.println(" ms");

    Serial.println("Starting HEARTBEAT transmission...");
}

// =====================================================
// loop
// =====================================================
void loop()
{
    const unsigned long currentTime = millis();

    if (
        currentTime - lastSendTime
        >= SEND_INTERVAL_MS
    )
    {
        lastSendTime = currentTime;
        sendHeartbeat();
    }

    delay(1);
}