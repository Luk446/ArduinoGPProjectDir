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
  Serial.print("LED Command Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? " Success" : " Fail");
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
           recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
  
  memcpy(&incomingData, data, sizeof(incomingData));
  
  Serial.println("========================================");
  Serial.print("Data from Node ");
  Serial.print(incomingData.nodeID);
  Serial.print(" (MAC: ");
  Serial.print(macStr);
  Serial.println(")");
  Serial.print("Temperature: ");
  Serial.print(incomingData.temp);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(incomingData.hum);
  Serial.println(" %");
  Serial.println("========================================");
  
  if (incomingData.nodeID >= 1 && incomingData.nodeID <= 4) {
    nodeData[incomingData.nodeID - 1].temp = incomingData.temp;
    nodeData[incomingData.nodeID - 1].hum = incomingData.hum;
    nodeData[incomingData.nodeID - 1].lastUpdate = millis();
    nodeData[incomingData.nodeID - 1].active = true;
  }
  
  checkTemperatureThreshold();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("📨 MQTT Message [");
  Serial.print(topic);
  Serial.print("]: ");
  
  payload[length] = 0;
  Serial.println((char*)payload);
}

void checkTemperatureThreshold() {
  bool thresholdExceeded = false;
  
  for (int i = 0; i < 4; i++) {
    if (nodeData[i].active && (millis() - nodeData[i].lastUpdate < 30000)) {
      if (nodeData[i].temp > TEMP_THRESHOLD) {
        thresholdExceeded = true;
        Serial.print("⚠️  NODE ");
        Serial.print(i + 1);
        Serial.print(" TEMP EXCEEDED! (");
        Serial.print(nodeData[i].temp);
        Serial.println(" °C)");
      }
    }
  }
  
  if (thresholdExceeded && !alarmActive) {
    alarmActive = true;
    Serial.println("ALARM ACTIVATED - SENDING RED LED COMMAND TO ALL NODES");
    broadcastLEDCommand(true, 255, 0, 0);
  } 
  else if (!thresholdExceeded && alarmActive) {
    alarmActive = false;
    Serial.println("ALARM DEACTIVATED - SENDING GREEN LED COMMAND TO ALL NODES");
    broadcastLEDCommand(true, 0, 255, 0);
  }
}

void broadcastLEDCommand(bool turnOn, uint8_t r, uint8_t g, uint8_t b) {
  ledCmd.turnOn = turnOn;
  ledCmd.r = r;
  ledCmd.g = g;
  ledCmd.b = b;
  
  Serial.print("Broadcasting LED Command: ");
  Serial.print("R=");
  Serial.print(r);
  Serial.print(" G=");
  Serial.print(g);
  Serial.print(" B=");
  Serial.println(b);
  
  esp_err_t result = esp_now_send(NULL, (uint8_t*)&ledCmd, sizeof(ledCmd));
  
  if (result != ESP_OK) {
    Serial.println("Failed to broadcast LED command");
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
  
  Serial.println("Publishing to Node-RED:");
  Serial.println(msg);
  
  if (!mqtt.publish(MQTT_TOPIC, msg)) {
    Serial.println("MQTT Publish Failed");
  } else {
    Serial.println("MQTT Publish Success");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("║    ESP32 GATEWAY STARTING             ║");
  
  for (int i = 0; i < 4; i++) {
    nodeData[i].active = false;
    nodeData[i].temp = 0.0;
    nodeData[i].hum = 0.0;
    nodeData[i].lastUpdate = 0;
  }
  
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\ WiFi Connected");
  Serial.print("   IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    ESP.restart();
  }
  Serial.println("ESP-NOW Initialized");
  
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  esp_now_register_recv_cb(OnDataRecv);
  
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  Serial.println("\n Registering Node Peers:");
  
  memcpy(peerInfo.peer_addr, node1Address, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("    Node 1 (swr) registered");
  } else {
    Serial.println("    Node 1 registration failed");
  }
  
  memcpy(peerInfo.peer_addr, node2Address, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("    Node 2 (luke) registered");
  } else {
    Serial.println("    Node 2 registration failed");
  }
  
  memcpy(peerInfo.peer_addr, node3Address, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial.println("    Node 3 (ignacio) registered");
  } else {
    Serial.println("    Node 3 registration failed");
  }
  
  Serial.println("\n Connecting to MQTT Broker...");
  mqtt.setCallback(mqttCallback);
  
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial.println(" MQTT Connected");
    Serial.print("   Subscribing to: ");
    Serial.println(MQTT_TOPIC_DISPLAY);
    mqtt.subscribe(MQTT_TOPIC_DISPLAY);
  } else {
    Serial.println(" MQTT Connection Failed");
    Serial.print("   MQTT State: ");
    Serial.println(mqtt.state());
  }
  
  Serial.println("║    GATEWAY READY - LISTENING          ║");
}

void loop() {
  mqtt.loop();
  
  if (!mqtt.connected()) {
    Serial.println("  MQTT Disconnected - Reconnecting...");
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial.println(" MQTT Reconnected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
    } else {
      Serial.print(" MQTT Reconnect Failed (State: ");
      Serial.print(mqtt.state());
      Serial.println(")");
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