// ---------------------------------------------------------
// LIBRARY IMPORTS
// Required libraries for WiFi, ESP-NOW, MQTT, and JSON
// ---------------------------------------------------------
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// ---------------------------------------------------------
// MQTT CONFIGURATION
// MQTT broker settings and credentials
// ---------------------------------------------------------
#define MQTT_HOST "broker.mqtt.cool"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "esp_gateway"
#define MQTT_USER ""
#define MQTT_TOKEN ""
#define MQTT_TOPIC ""
#define MQTT_SUB_TOPIC "ThreshCheck4482"
#define MQTT_TEST_TOPIC "TestTopic4482"

float testval = 25.0;

// ---------------------------------------------------------
// SYSTEM CONFIGURATION
// Temperature threshold and WiFi timeout settings
// ---------------------------------------------------------
#define TEMP_THRESHOLD 23.0
#define WIFI_TIMEOUT_MS 10000  // 10 seconds timeout for WiFi connection

// ---------------------------------------------------------
// NODE MAC ADDRESSES
// MAC addresses of ESP-NOW peer nodes
// ---------------------------------------------------------
uint8_t node1Address[] = {0x78, 0xE3, 0x6D, 0x07, 0x90, 0x78}; // swr
uint8_t node2Address[] = {0xF0, 0xF5, 0xBD, 0xFB, 0x26, 0xB4}; // luke
uint8_t node3Address[] = {0x08, 0xB6, 0x1F, 0x28, 0x79, 0xF8}; // ignacio
uint8_t node4Address[] = {0x08, 0xB6, 0x1F, 0x28, 0x86, 0xE8}; // mur


// ---------------------------------------------------------
// WIFI CREDENTIALS
// Network SSID and password
// ---------------------------------------------------------
const char* ssid = "B31IOT-MQTT";
const char* pass = "QWERTY1234";

// ---------------------------------------------------------
// DATA STRUCTURES
// Struct definitions for sensor data and LED commands
// ---------------------------------------------------------

// Sensor data structure received from nodes
typedef struct sensor_data {
  int nodeID;
  float temp;
  float hum;
} __attribute__((packed)) sensor_data;

// LED command structure sent to nodes
typedef struct led_command {
  uint8_t turnOn;  // Use uint8_t instead of bool for consistent struct packing
  uint8_t r;
  uint8_t g;
  uint8_t b;
} __attribute__((packed)) led_command;

// ---------------------------------------------------------
// GLOBAL VARIABLES
// Instance variables for data handling and state tracking
// ---------------------------------------------------------

// Incoming sensor data buffer
sensor_data incomingData;
// LED command buffer
led_command ledCmd;

// Node data storage structure with temperature, humidity, and activity tracking
struct NodeData {
  float temp;
  float hum;
  unsigned long lastUpdate;
  bool active;
} nodeData[4];

// WiFi and MQTT client objects
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, wifiClient);

// ESP-NOW peer information
esp_now_peer_info_t peerInfo;

// JSON document for MQTT messages
StaticJsonDocument<300> jsonDoc;
char msg[300];

// Alarm state tracking
bool alarmActive = false;

// MQTT publishing timing variables
unsigned long lastPublishTime = 0;
const unsigned long publishInterval = 5000;

// Connection status flags
bool wifiConnected = false;
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000;

// ---------------------------------------------------------
// ESP-NOW CALLBACK: OnDataSent
// Callback function triggered when ESP-NOW data is sent
// Reports success or failure of LED command transmission
// ---------------------------------------------------------
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial0.print("LED Command Send Status: ");
  Serial0.println(status == ESP_NOW_SEND_SUCCESS ? " Success" : " Fail");
}

// ---------------------------------------------------------
// ESP-NOW CALLBACK: OnDataRecv
// Callback function triggered when sensor data is received
// Processes incoming temperature/humidity data from nodes
// Updates node data arrays and checks temperature thresholds
// Sends initial blue LED command when node first connects
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// MQTT CALLBACK: mqttCallback
// Callback function for incoming MQTT messages
// Prints received messages to serial monitor
// ---------------------------------------------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial0.print("MQTT Message [");
  Serial0.print(topic);
  Serial0.print("]: ");
  
  payload[length] = 0;
  Serial0.println((char*)payload);
}

// ---------------------------------------------------------
// FUNCTION: checkTemperatureThreshold
// Monitors temperature data from all active nodes
// Activates alarm (red LED) if any node exceeds threshold
// Deactivates alarm (green LED) when all temps are normal
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// FUNCTION: broadcastLEDCommand
// Sends LED color commands to all registered nodes via ESP-NOW
// Parameters: turnOn (on/off), r/g/b (RGB color values)
// Reports success/failure for each node transmission
// ---------------------------------------------------------
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
  esp_err_t result4 = esp_now_send(node4Address, (uint8_t*)&ledCmd, sizeof(ledCmd));
  
  
  if (result1 != ESP_OK || result2 != ESP_OK || result3 != ESP_OK || result4 != ESP_OK) {
    Serial0.println("Failed to send LED command to one or more nodes");
    Serial0.print("  Node1: ");
    Serial0.print(result1 == ESP_OK ? "OK" : "FAIL");
    Serial0.print(" Node2: ");
    Serial0.print(result2 == ESP_OK ? "OK" : "FAIL");
    Serial0.print(" Node3: ");
    Serial0.print(result3 == ESP_OK ? "OK" : "FAIL");
    Serial0.print(" Node4: ");
    Serial0.println(result4 == ESP_OK ? "OK" : "FAIL");
  }
}

// ----------------------------------------------------------
// FUNCTION: check MQTT 
// publish a test message to check connection
// publish a test value over the test topic
// ----------------------------------------------------------
void checkMQTT() {

  // publish the test value
  StaticJsonDocument<100> jsonDoc;
  JsonObject payload = jsonDoc.to<JsonObject>();
  payload["testing"] = testval;
  char msg[100];
  serializeJson(jsonDoc, msg, sizeof(msg));
  Serial0.print("Publishing to ");
  Serial0.print(MQTT_TEST_TOPIC);
  Serial0.print(": ");
  Serial0.println(msg);
  mqtt.publish(MQTT_TEST_TOPIC, msg);
  delay(500);
}


// ---------------------------------------------------------
// FUNCTION: publishToNodeRED
// Publishes sensor data from all nodes to MQTT broker
// Creates JSON payload with temperature, humidity, and alarm status
// Only publishes if MQTT connection is active
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// WIFI CONNECTION SETUP
void setupWIFI() {
  Serial0.println("Connecting to WiFi...");
  delay(500);
  WiFi.mode(WIFI_STA);
  delay(500);
  WiFi.disconnect(); // Disconnect from any previous connections
  delay(500);
  WiFi.begin(ssid, pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial0.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial0.println("\nWiFi connected");
    Serial0.print("IP address: ");
    Serial0.println(WiFi.localIP());
    wifiConnected = true; // FIX: ensure flag reflects successful connection
  } else {
    Serial0.println("\nWiFi connection failed!");
    wifiConnected = false; // explicitly mark as not connected
    return;
  }
}

// ---------------------------------------------------------
// FUNCTION: registerNode
// Registers a node as an ESP-NOW peer
// Parameters: nodeAddress (MAC address), nodeName (for logging)
// ---------------------------------------------------------
void registerNode(uint8_t* nodeAddress, const char* nodeName) {
  memcpy(peerInfo.peer_addr, nodeAddress, 6);
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    Serial0.print("    ");
    Serial0.print(nodeName);
    Serial0.println(" registered");
  } else {
    Serial0.print("    ");
    Serial0.print(nodeName);
    Serial0.println(" registration failed");
  }
}

// ---------------------------------------------------------
// SETUP FUNCTION
// Initializes gateway: WiFi, ESP-NOW, MQTT connections
// Registers peer nodes for ESP-NOW communication
// Sets up callbacks and prepares system for operation
// wifi must start before esp-now
// ---------------------------------------------------------
void setup() {
  
  Serial0.begin(115200); // start serial monitor
  delay(1000); // wait for serial to initialize
  Serial0.println("║    ESP32 GATEWAY STARTING       ║");

  // Initialize node data structures
  for (int i = 0; i < 4; i++) {
    nodeData[i].active = false;
    nodeData[i].temp = 0.0;
    nodeData[i].hum = 0.0;
    nodeData[i].lastUpdate = 0;
  }
  
  // Check if WiFi credentials are provided then start wifi
  if (strlen(ssid) == 0) {
    Serial0.println("\n  No WiFi credentials - Running in ESP-NOW only mode");
    wifiConnected = false;
  } else {
    setupWIFI();
  }

  // start esp-now after wifi
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // print sizes of data structures
  Serial0.print("Struct Sizes - sensor_data: ");
  Serial0.print(sizeof(sensor_data));
  Serial0.print(" bytes, led_command: ");
  Serial0.print(sizeof(led_command));
  Serial0.println(" bytes");
  delay(500);

  // Register ESP-NOW callbacks
  esp_now_register_send_cb(esp_now_send_cb_t(OnDataSent));
  esp_now_register_recv_cb(OnDataRecv);

  // make sure peers are on same channels
  esp_now_peer_info_t peer;
  // Use channel 1 for all ESP-NOW communication
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  
  Serial0.println("\n Registering Node Peers:");
  
  registerNode(node1Address, "Node 1 (swr)");
  registerNode(node2Address, "Node 2 (luke)");
  registerNode(node3Address, "Node 3 (ignacio)");
  registerNode(node4Address, "Node 4 (mur)");
  
  // Only attempt MQTT if WiFi is connected
  if (wifiConnected) {
    Serial0.println("\nConnecting to MQTT Broker...");
    mqtt.setCallback(mqttCallback);
    
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial0.println("SUCCESS: MQTT Connected");
      Serial0.print("   Subscribing to: ");
      Serial0.println(MQTT_SUB_TOPIC);
      mqtt.subscribe(MQTT_SUB_TOPIC);
      mqttConnected = true;

      checkMQTT();

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

// ---------------------------------------------------------
// LOOP FUNCTION
// Main program loop that:
// - Maintains MQTT connection and handles reconnection
// - Publishes sensor data at regular intervals
// - Processes ESP-NOW messages (via callbacks)
// ---------------------------------------------------------
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
          mqtt.subscribe(MQTT_SUB_TOPIC);
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