#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>

#define NODE_ID 1

#define RGB_PIN 5
#define DHT_PIN 4
#define DHTTYPE DHT11

uint8_t gatewayAddress[] = {0x84, 0xF7, 0x03, 0x12, 0xA8, 0xCC};

typedef struct sensor_data {
  int nodeID;
  float temp;
  float hum;
} sensor_data;

typedef struct led_command {
  bool turnOn;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} led_command;

sensor_data sensorData;
led_command ledCmd;

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, RGB_PIN, NEO_GRB + NEO_KHZ800);
DHT dht(DHT_PIN, DHTTYPE);
esp_now_peer_info_t peerInfo;

float temp = 0.0;
float hum = 0.0;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  Serial.println("========================================");
  Serial.print("RECEIVED DATA - Length: ");
  Serial.println(len);
  
  if (len != sizeof(led_command)) {
    Serial.print("ERROR: Wrong data size! Expected: ");
    Serial.print(sizeof(led_command));
    Serial.print(" Got: ");
    Serial.println(len);
    return;
  }
  
  memcpy(&ledCmd, data, sizeof(ledCmd));
  
  Serial.println("LED Command Received from Gateway");
  Serial.print("Turn On: ");
  Serial.println(ledCmd.turnOn);
  Serial.print("R: ");
  Serial.println(ledCmd.r);
  Serial.print("G: ");
  Serial.println(ledCmd.g);
  Serial.print("B: ");
  Serial.println(ledCmd.b);
  
  if (ledCmd.turnOn) {
    Serial.println("Setting LED color...");
    pixel.setPixelColor(0, ledCmd.r, ledCmd.g, ledCmd.b);
    pixel.show();
    Serial.println("LED updated!");
  } else {
    Serial.println("Turning LED off...");
    pixel.setPixelColor(0, 0, 0, 0);
    pixel.show();
  }
  
  Serial.println("========================================");
}

void sendSensorData() {
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read DHT sensor");
    return;
  }
  
  sensorData.nodeID = NODE_ID;
  sensorData.temp = temp;
  sensorData.hum = hum;
  
  Serial.println("========================================");
  Serial.print("Node ");
  Serial.print(NODE_ID);
  Serial.println(" - Sending to Gateway");
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" C");
  Serial.print("Humidity: ");
  Serial.print(hum);
  Serial.println(" %");
  Serial.println("========================================");
  
  esp_err_t result = esp_now_send(gatewayAddress, (uint8_t*)&sensorData, sizeof(sensorData));
  
  if (result != ESP_OK) {
    Serial.print("Error sending data: ");
    Serial.println(result);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("========================================");
  Serial.print("ESP32 NODE ");
  Serial.print(NODE_ID);
  Serial.println(" STARTING");
  Serial.println("========================================");
  
  dht.begin();
  
  Serial.println("Initializing Neopixel...");
  pixel.begin();
  pixel.setBrightness(50);
  
  Serial.println("Testing LED - Blue");
  pixel.setPixelColor(0, 0, 0, 255);
  pixel.show();
  delay(1000);
  
  Serial.println("Testing LED - Red");
  pixel.setPixelColor(0, 255, 0, 0);
  pixel.show();
  delay(1000);
  
  Serial.println("Testing LED - Green");
  pixel.setPixelColor(0, 0, 255, 0);
  pixel.show();
  delay(1000);
  
  Serial.println("LED OFF");
  pixel.setPixelColor(0, 0, 0, 0);
  pixel.show();
  
  WiFi.mode(WIFI_STA);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    ESP.restart();
  }
  Serial.println("ESP-NOW Initialized");
  
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
  
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, gatewayAddress, 6);
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add Gateway peer");
    ESP.restart();
  }
  Serial.println("Gateway registered as peer");
  
  Serial.println("========================================");
  Serial.println("NODE READY - MONITORING");
  Serial.println("Waiting for LED commands...");
  Serial.println("========================================");
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSendTime >= sendInterval) {
    sendSensorData();
    lastSendTime = currentTime;
  }
  
  delay(100);
}
