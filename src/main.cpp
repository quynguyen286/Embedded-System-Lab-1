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
bool atribute_Data = false;
RPC_Response setLedSwitchState(const RPC_Data &data){
  Serial.println("Received Switch state");
  bool newstate = data;
  Serial.print("Switch state change: ");
  Serial.println(newstate);
  atribute_Data = true;
  return RPC_Response("setLedSwitchValue", newstate);
}

const std::array<RPC_Callback, 1U> callbacks = {
  RPC_Callback{"setLedSwitchValue", setLedSwitchState}
};

// ================== SETUP ==================
void setup()
{
  Serial.begin(SERIAL_BAUD);
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
  if (atribute_Data){
    atribute_Data = false;
  }
  else atribute_Data = true;
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
