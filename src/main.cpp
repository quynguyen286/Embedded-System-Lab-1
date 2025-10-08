#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include <DHT.h>

// ================== CONFIG ==================
#define DHTPIN 8
#define DHTTYPE DHT11
#define LED_PIN 48
constexpr char WIFI_SSID[] = "Nước mắm có ga";
constexpr char WIFI_PASSWORD[] = "11111111";
constexpr char TOKEN[] = "185bccplog39k6errzrf";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883;

constexpr uint32_t SERIAL_BAUD = 9600;
constexpr uint32_t SEND_INTERVAL = 500; // ms

// ================== OBJECTS ==================
WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient);
DHT dht(DHTPIN, DHTTYPE);

uint32_t lastSend = 0;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr char LED_MODE_ATTR[] = "ledMode";
constexpr char LED_STATE_ATTR[] = "ledState";

volatile bool attributesChanged = false;
volatile int ledMode = 0;
volatile bool ledState = false;

constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;

uint32_t previousStateChange;

constexpr int16_t telemetrySendInterval = 10000U;
uint32_t previousDataSend;

constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
  LED_STATE_ATTR,
  BLINKING_INTERVAL_ATTR
};




RPC_Response setLedSwitchState(const RPC_Data &data) {
    Serial.println("Received Switch state");
    bool newState = data;
    Serial.print("Switch state change: ");
    Serial.println(newState);
    digitalWrite(LED_PIN, newState);
    attributesChanged = true;
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 1U> callbacks = {
  RPC_Callback{ "setLedSwitchValue", setLedSwitchState }
};

void processSharedAttributes(const Shared_Attribute_Data &data) {
  for (auto it = data.begin(); it != data.end(); ++it) {
    if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0) {
      const uint16_t new_interval = it->value().as<uint16_t>();
      if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX) {
        blinkingInterval = new_interval;
        Serial.print("Blinking interval is set to: ");
        Serial.println(new_interval);
      }
    } else if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0) {
      ledState = it->value().as<bool>();
      digitalWrite(LED_PIN, ledState);
      Serial.print("LED state is set to: ");
      Serial.println(ledState);
    }
  }
  attributesChanged = true;
}

const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());

// ================== WIFI ==================
void connectWiFi()
{
  Serial.printf("\n Kết nối WiFi : %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\n Connected! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ================== THINGSBOARD ==================
bool ensureConnected()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectWiFi();
  }

  if (!tb.connected())
  {
    Serial.printf("Đang kết nối với CoreIOT (%s)...\n", THINGSBOARD_SERVER);
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT))
    {
      Serial.println("Kết nối thất bại với CoreIOT!");
      delay(2000);
      return false;
    }
    Serial.println("Kết nối thành công với CoreIOT!");
  }
  return true;
}
// ================== SETUP ==================
void setup()
{
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_PIN, OUTPUT);
  delay(1000);

  Serial.println("Khởi động...");
  connectWiFi();
  dht.begin();
}

// ================== LOOP ==================
void loop()
{
  if (!ensureConnected())
    return;
  if (attributesChanged) {
    attributesChanged = false;
    tb.sendAttributeData(LED_STATE_ATTR, digitalRead(LED_PIN));
  }
  if (millis() - lastSend >= SEND_INTERVAL)
  {
    lastSend = millis();

    float temp = dht.readTemperature();
    float humi = dht.readHumidity();

    if (isnan(temp) || isnan(humi))
    {
      Serial.println("Đọc dữ liệu từ cảm biến DHT11 thất bại!");
    }
    else
    {
      Serial.printf("Nhiệt độ : %.2f °C | Độ ẩm : %.2f %%\n", temp, humi);
      tb.sendTelemetryData("temperature", temp);
      tb.sendTelemetryData("humidity", humi);
    }

    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
    tb.sendAttributeData("ip", WiFi.localIP().toString().c_str());
  }

  tb.loop();
}
