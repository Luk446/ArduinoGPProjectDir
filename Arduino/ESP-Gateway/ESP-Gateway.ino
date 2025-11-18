#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MQTT_HOST ""
#define MQTT_PORT 1883
#define MQTT_DEVICEID ""
#define MQTT_USER ""
#define MQTT_TOKEN ""
#define MQTT_TOPIC ""
#define MQTT_TOPIC_DISPLAY ""

#define TEMP_THRESHOLD 23.0
#define WIFI_TIMEOUT_MS 10000  // 10 seconds timeout for WiFi connection

uint8_t node1Address[] = {0x78, 0xE3, 0x6D, 0x07, 0x90, 0x78}; // swr
uint8_t node2Address[] = {0xF0, 0xF5, 0xBD, 0xFB, 0x26, 0xB4}; // luke
uint8_t node3Address[] = {0x08, 0xB6, 0x1F, 0x28, 0x79, 0xF8}; // ignacio

char ssid[] = "";
char pass[] = "";

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

// Connection status flags
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000;

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
    bool wasInactive = !nodeData[incomingData.nodeID - 1].active;
    
    nodeData[incomingData.nodeID - 1].temp = incomingData.temp;
    nodeData[incomingData.nodeID - 1].hum = incomingData.hum;
    nodeData[incomingData.nodeID - 1].lastUpdate = millis();
    nodeData[incomingData.nodeID - 1].active = true;
    
    // Send initial blue LED command when node first connects
    if (wasInactive && !alarmActive) {
      Serial0.print("Node ");
      Serial0.print(incomingData.nodeID);
      Serial0.println(" activated - sending initial BLUE LED");
      
      ledCmd.turnOn = 1;
      ledCmd.r = 0;
      ledCmd.g = 0;
      ledCmd.b = 255;
      
      if (incomingData.nodeID == 1) {
        esp_now_send(node1Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
      } else if (incomingData.nodeID == 2) {
        esp_now_send(node2Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
      } else if (incomingData.nodeID == 3) {
        esp_now_send(node3Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
      }
    }
  }
  
  checkTemperatureThreshold();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial0.print("MQTT Message [");
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
        Serial0.print("WARNING: NODE ");
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
  if (!mqttConnected) {
    Serial0.println("WARNING: Cannot publish - MQTT not connected");
    return;
  }
  
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
    Serial0.println("ERROR: MQTT Publish Failed");
    mqttConnected = false;
  } else {
    Serial0.println("SUCCESS: MQTT Publish Success");
  }
}

void setup() {
  Serial0.begin(115200);
  delay(1000);
  Serial0.println("║    ESP32 GATEWAY STARTING             ║");
  // broadcastLEDCommand(true, 0, 0, 255);
  // Serial0.println("Setting default LED colour");
  for (int i = 0; i < 4; i++) {
    nodeData[i].active = false;
    nodeData[i].temp = 0.0;
    nodeData[i].hum = 0.0;
    nodeData[i].lastUpdate = 0;
  }
  
  WiFi.mode(WIFI_STA);
  Serial0.print(" Connecting to WiFi");
  
  // Check if WiFi credentials are provided
  if (strlen(ssid) == 0) {
    Serial0.println("\n  No WiFi credentials - Running in ESP-NOW only mode");
    wifiConnected = false;
  } else {
    WiFi.begin(ssid, pass);
    unsigned long startAttemptTime = millis();
    
    // Try to connect with timeout
    while (WiFi.status() != WL_CONNECTED && 
           millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
      delay(500);
      Serial0.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial0.println("\n WiFi Connected");
      Serial0.print("   IP Address: ");
      Serial0.println(WiFi.localIP());
      Serial0.print("   WiFi Channel: ");
      Serial0.println(WiFi.channel());
      wifiConnected = true;
    } else {
      Serial0.println("\n  WiFi connection timeout - Running in ESP-NOW only mode");
      wifiConnected = false;
    }
  }
  
  Serial0.print("MAC Address: ");
  Serial0.println(WiFi.macAddress());
  
  // Set WiFi channel to 1 for ESP-NOW communication
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  Serial0.println("Set WiFi channel to 1 for ESP-NOW");
  
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
  
  // Use channel 1 for all ESP-NOW communication
  peerInfo.channel = 1;
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
  
  // Only attempt MQTT if WiFi is connected
  if (wifiConnected) {
    Serial0.println("\nConnecting to MQTT Broker...");
    mqtt.setCallback(mqttCallback);
    
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial0.println("SUCCESS: MQTT Connected");
      Serial0.print("   Subscribing to: ");
      Serial0.println(MQTT_TOPIC_DISPLAY);
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      mqttConnected = true;
    } else {
      Serial0.println("ERROR: MQTT Connection Failed");
      Serial0.print("   MQTT State: ");
      Serial0.println(mqtt.state());
      mqttConnected = false;
    }
  } else {
    Serial0.println("\nWARNING: Skipping MQTT (WiFi not connected)");
    mqttConnected = false;
  }
  
  Serial0.println("\n╔══════════════════════════════════════╗");
  Serial0.println("║    GATEWAY READY - LISTENING          ║");
  Serial0.print("║    Mode: ESP-NOW ");
  if (wifiConnected && mqttConnected) {
    Serial0.println("+ WiFi + MQTT      ║");
  } else if (wifiConnected) {
    Serial0.println("+ WiFi (no MQTT)   ║");
  } else {
    Serial0.println("only               ║");
  }
  Serial0.println("╚══════════════════════════════════════╝");
}

void loop() {
  // Only run MQTT operations if WiFi is connected
  if (wifiConnected) {
    mqtt.loop();
    
    // Check MQTT connection and reconnect if needed
    if (!mqtt.connected()) {
      mqttConnected = false;
      unsigned long currentTime = millis();
      
      // Only attempt reconnection at intervals
      if (currentTime - lastMqttReconnectAttempt >= mqttReconnectInterval) {
        lastMqttReconnectAttempt = currentTime;
        Serial0.println("MQTT Disconnected - Reconnecting...");
        
        if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
          Serial0.println("SUCCESS: MQTT Reconnected");
          mqtt.subscribe(MQTT_TOPIC_DISPLAY);
          mqttConnected = true;
        } else {
          Serial0.print("ERROR: MQTT Reconnect Failed (State: ");
          Serial0.print(mqtt.state());
          Serial0.println(")");
        }
      }
    } else {
      mqttConnected = true;
    }
    
    // Publish data at regular intervals if MQTT is connected
    unsigned long currentTime = millis();
    if (mqttConnected && (currentTime - lastPublishTime >= publishInterval)) {
      publishToNodeRED();
      lastPublishTime = currentTime;
    }
  }
  
  // ESP-NOW continues to work regardless of WiFi/MQTT status
  delay(100);
}