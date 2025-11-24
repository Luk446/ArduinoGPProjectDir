#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>
#include <DHT.h>

// define based on selection
// swaroop = 1
// kuro = 2
// ignacio = 3
// mur = 4
#define NODE_ID 2

#define RGB_PIN 5
#define DHT_PIN 4
#define DHTTYPE DHT11
#define NEOPIXEL_TYPE (NEO_GRB + NEO_KHZ800)

uint8_t gatewayAddress[] = {0xF0, 0xF5, 0xBD, 0xFB, 0x26, 0xB4};

typedef struct sensor_data {
  int nodeID;
  float temp;
  float hum;
} __attribute__((packed)) sensor_data;

typedef struct led_command {
  uint8_t turnOn;  // Use uint8_t instead of bool for consistent struct packing
  uint8_t r;
  uint8_t g;
  uint8_t b;
} __attribute__((packed)) led_command;

sensor_data sensorData;
led_command ledCmd;

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, RGB_PIN, NEOPIXEL_TYPE);
DHT dht(DHT_PIN, DHTTYPE);
esp_now_peer_info_t peerInfo;

float temp = 0.0;
float hum = 0.0;
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial0.print("Send Status: ");
  Serial0.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  // Debug: Print sender MAC and data length
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  
  Serial0.println("========================================");
  Serial0.print("Data Received from MAC: ");
  Serial0.println(macStr);
  Serial0.print("Data Length: ");
  Serial0.print(len);
  Serial0.print(" bytes (expected ");
  Serial0.print(sizeof(ledCmd));
  Serial0.println(" bytes)");
  
  // Check if it's an LED command (correct size)
  if (len == sizeof(ledCmd)) {
    memcpy(&ledCmd, data, sizeof(ledCmd));
    
    Serial0.println("LED Command Received from Gateway");
    Serial0.print("Turn On: ");
    Serial0.println(ledCmd.turnOn ? "YES" : "NO");
    Serial0.print("Color (R,G,B): (");
    Serial0.print(ledCmd.r);
    Serial0.print(", ");
    Serial0.print(ledCmd.g);
    Serial0.print(", ");
    Serial0.print(ledCmd.b);
    Serial0.println(")");
    
    // Always update LED color, never turn it off
    pixel.setPixelColor(0, ledCmd.r, ledCmd.g, ledCmd.b);
    pixel.show();
    Serial0.println("LED Updated!");
  } else {
    Serial0.println("WARNING: Received data size mismatch - ignoring");
  }
  Serial0.println("========================================");
}

void sendSensorData() {
  temp = dht.readTemperature();
  hum = dht.readHumidity();
  
  if (isnan(temp) || isnan(hum)) {
    Serial0.println("Failed to read DHT sensor");
    return;
  }
  
  sensorData.nodeID = NODE_ID;
  sensorData.temp = temp;
  sensorData.hum = hum;
  
  Serial0.println("========================================");
  Serial0.print("Node ");
  Serial0.print(NODE_ID);
  Serial0.println(" - Sending to Gateway");
  Serial0.print("Temperature: ");
  Serial0.print(temp);
  Serial0.println(" C");
  Serial0.print("Humidity: ");
  Serial0.print(hum);
  Serial0.println(" %");
  Serial0.println("========================================");
  
  esp_err_t result = esp_now_send(gatewayAddress, (uint8_t*)&sensorData, sizeof(sensorData));
  
  if (result != ESP_OK) {
    Serial0.println("Error sending data");
  }
}

void setup() {
  Serial0.begin(115200);
  delay(1000);
  Serial0.println("========================================");
  Serial0.print("ESP32 NODE ");
  Serial0.print(NODE_ID);
  Serial0.println(" STARTING");
  Serial0.println("========================================");
  
  dht.begin();
  pixel.begin();
  pixel.setPixelColor(0, 0, 0, 255);
  pixel.show();
  
  WiFi.mode(WIFI_STA);
  Serial0.print("MAC Address: ");
  Serial0.println(WiFi.macAddress());
  
  // Set WiFi channel to 10 for ESP-NOW communication (must match Gateway)
  esp_wifi_set_channel(10, WIFI_SECOND_CHAN_NONE);
  Serial0.println("Set WiFi channel to 10 for ESP-NOW");
  
  if (esp_now_init() != ESP_OK) {
    Serial0.println("ESP-NOW Init Failed");
    ESP.restart();
  }
  Serial0.println("ESP-NOW Initialized");
  
  Serial0.print("Struct Sizes - sensor_data: ");
  Serial0.print(sizeof(sensor_data));
  Serial0.print(" bytes, led_command: ");
  Serial0.print(sizeof(led_command));
  Serial0.println(" bytes");
  
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  esp_now_register_recv_cb(OnDataRecv);
  
  // Use channel 10 to match Gateway
  peerInfo.channel = 10;
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, gatewayAddress, 6);
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial0.println("Failed to add Gateway peer");
    ESP.restart();
  }
  Serial0.println("Gateway registered as peer");
  
  // Initialize LED to green (normal state)
  pixel.setPixelColor(0, 0, 255, 0);
  pixel.show();
  delay(1000);
  
  Serial0.println("========================================");
  Serial0.println("NODE READY - MONITORING");
  Serial0.println("LED initialized to GREEN");
  Serial0.println("========================================");
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSendTime >= sendInterval) {
    sendSensorData();
    lastSendTime = currentTime;
  }
  
  delay(100);
}