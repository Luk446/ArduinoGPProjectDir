#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MQTT_HOST ""
#define MQTT_PORT 1883
#define MQTT_DEVICEID ""
#define MQTT_USER ""
#define MQTT_TOKEN ""
#define MQTT_TOPIC ""
#define MQTT_TOPIC_DISPLAY ""

#define TEMP_THRESHOLD 30.0

uint8_t node1Address[] = {0x78, 0xE3, 0x6D, 0x07, 0x90, 0x78}; // swr
uint8_t node2Address[] = {0xF0, 0xF5, 0xBD, 0xFB, 0x26, 0xB4}; // luke
uint8_t node3Address[] = {0x08, 0xB6, 0x1F, 0x28, 0x79, 0xF8}; // ignacio

char ssid[] = "";
char pass[] = "";

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

sensor_data incomingData;
led_command ledCmd;

struct NodeData {
  float temp;
  float hum;
  unsigned long lastUpdate;
  bool active;
} nodeData[4];

WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, wifiClient);
esp_now_peer_info_t peerInfo;

StaticJsonDocument<300> jsonDoc;
char msg[300];
bool alarmActive = false;
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 5000;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial0.print("LED Command Send Status: ");
  Serial0.println(status == ESP_NOW_SEND_SUCCESS ? " Success" : " Fail");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  
  memcpy(&incomingData, data, sizeof(incomingData));
  
  Serial0.println("========================================");
  Serial0.print("Data from Node ");
  Serial0.print(incomingData.nodeID);
  Serial0.print(" (MAC: ");
  Serial0.print(macStr);
  Serial0.println(")");
  Serial0.print("Temperature: ");
  Serial0.print(incomingData.temp);
  Serial0.println(" °C");
  Serial0.print("Humidity: ");
  Serial0.print(incomingData.hum);
  Serial0.println(" %");
  Serial0.println("========================================");
  
  if (incomingData.nodeID >= 1 && incomingData.nodeID <= 4) {
    nodeData[incomingData.nodeID - 1].temp = incomingData.temp;
    nodeData[incomingData.nodeID - 1].hum = incomingData.hum;
    nodeData[incomingData.nodeID - 1].lastUpdate = millis();
    nodeData[incomingData.nodeID - 1].active = true;
  }
  
  checkTemperatureThreshold();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial0.print("📨 MQTT Message [");
  Serial0.print(topic);
  Serial0.print("]: ");
  
  payload[length] = 0;
  Serial0.println((char*)payload);
}

void checkTemperatureThreshold() {
  bool thresholdExceeded = false;
  
  for (int i = 0; i < 4; i++) {
    if (nodeData[i].active && (millis() - nodeData[i].lastUpdate < 30000)) {
      if (nodeData[i].temp > TEMP_THRESHOLD) {
        thresholdExceeded = true;
        Serial0.print("⚠️  NODE ");
        Serial0.print(i + 1);
        Serial0.print(" TEMP EXCEEDED! (");
        Serial0.print(nodeData[i].temp);
        Serial0.println(" °C)");
      }
    }
  }
  
  if (thresholdExceeded && !alarmActive) {
    alarmActive = true;
    Serial0.println("ALARM ACTIVATED - SENDING RED LED COMMAND TO ALL NODES");
    broadcastLEDCommand(true, 255, 0, 0);
  } 
  else if (!thresholdExceeded && alarmActive) {
    alarmActive = false;
    Serial0.println("ALARM DEACTIVATED - SENDING GREEN LED COMMAND TO ALL NODES");
    broadcastLEDCommand(true, 0, 255, 0);
  }
}

void broadcastLEDCommand(bool turnOn, uint8_t r, uint8_t g, uint8_t b) {
  ledCmd.turnOn = turnOn;
  ledCmd.r = r;
  ledCmd.g = g;
  ledCmd.b = b;
  
  Serial0.print("Broadcasting LED Command: ");
  Serial0.print("R=");
  Serial0.print(r);
  Serial0.print(" G=");
  Serial0.print(g);
  Serial0.print(" B=");
  Serial0.println(b);
  
  // Send to each registered node individually
  esp_err_t result1 = esp_now_send(node1Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
  esp_err_t result2 = esp_now_send(node2Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
  esp_err_t result3 = esp_now_send(node3Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
  
  if (result1 != ESP_OK || result2 != ESP_OK || result3 != ESP_OK) {
    Serial0.println("Failed to send LED command to one or more nodes");
    Serial0.print("  Node1: ");
    Serial0.print(result1 == ESP_OK ? "OK" : "FAIL");
    Serial0.print(" Node2: ");
    Serial0.print(result2 == ESP_OK ? "OK" : "FAIL");
    Serial0.print(" Node3: ");
    Serial0.println(result3 == ESP_OK ? "OK" : "FAIL");
  }
}


void publishToNodeRED() {
  jsonDoc.clear();
  JsonObject payload = jsonDoc.createNestedObject("d");
  
  for (int i = 0; i < 4; i++) {
    if (nodeData[i].active && (millis() - nodeData[i].lastUpdate < 30000)) {
      String nodePrefix = "node" + String(i + 1);
      payload[nodePrefix + "_temp"] = nodeData[i].temp;
      payload[nodePrefix + "_hum"] = nodeData[i].hum;
      payload[nodePrefix + "_active"] = true;
    } else {
      String nodePrefix = "node" + String(i + 1);
      payload[nodePrefix + "_active"] = false;
    }
  }
  
  payload["alarm"] = alarmActive;
  payload["threshold"] = TEMP_THRESHOLD;
  
  serializeJson(jsonDoc, msg, sizeof(msg));
  
  Serial0.println("Publishing to Node-RED:");
  Serial0.println(msg);
  
  if (!mqtt.publish(MQTT_TOPIC, msg)) {
    Serial0.println("MQTT Publish Failed");
  } else {
    Serial0.println("MQTT Publish Success");
  }
}

void setup() {
  Serial0.begin(115200);
  delay(1000);
  Serial0.println("║    ESP32 GATEWAY STARTING             ║");
  
  for (int i = 0; i < 4; i++) {
    nodeData[i].active = false;
    nodeData[i].temp = 0.0;
    nodeData[i].hum = 0.0;
    nodeData[i].lastUpdate = 0;
  }
  
  WiFi.mode(WIFI_STA);
  Serial0.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial0.print(".");
  }
  Serial0.println("\ WiFi Connected");
  Serial0.print("   IP Address: ");
  Serial0.println(WiFi.localIP());
  Serial0.print("MAC Address: ");
  Serial0.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial0.println("ESP-NOW Init Failed");
    ESP.restart();
  }
  Serial0.println("ESP-NOW Initialized");
  
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  esp_now_register_recv_cb(OnDataRecv);
  
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  Serial0.println("\n Registering Node Peers:");
  
  memcpy(peerInfo.peer_addr, node1Address, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial0.println("    Node 1 (swr) registered");
  } else {
    Serial0.println("    Node 1 registration failed");
  }
  
  memcpy(peerInfo.peer_addr, node2Address, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial0.println("    Node 2 (luke) registered");
  } else {
    Serial0.println("    Node 2 registration failed");
  }
  
  memcpy(peerInfo.peer_addr, node3Address, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial0.println("    Node 3 (ignacio) registered");
  } else {
    Serial0.println("    Node 3 registration failed");
  }
  
  Serial0.println("\n Connecting to MQTT Broker...");
  mqtt.setCallback(mqttCallback);
  
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial0.println(" MQTT Connected");
    Serial0.print("   Subscribing to: ");
    Serial0.println(MQTT_TOPIC_DISPLAY);
    mqtt.subscribe(MQTT_TOPIC_DISPLAY);
  } else {
    Serial0.println(" MQTT Connection Failed");
    Serial0.print("   MQTT State: ");
    Serial0.println(mqtt.state());
  }
  
  Serial0.println("║    GATEWAY READY - LISTENING          ║");
}

void loop() {
  mqtt.loop();
  
  if (!mqtt.connected()) {
    Serial0.println("  MQTT Disconnected - Reconnecting...");
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial0.println(" MQTT Reconnected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
    } else {
      Serial0.print(" MQTT Reconnect Failed (State: ");
      Serial0.print(mqtt.state());
      Serial0.println(")");
      delay(5000);
    }
  }
  
  unsigned long currentTime = millis();
  if (currentTime - lastPublishTime >= publishInterval) {
    publishToNodeRED();
    lastPublishTime = currentTime;
  }
  
  delay(100);
}