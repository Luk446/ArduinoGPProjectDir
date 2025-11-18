#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// lab wifi
const char* ssid = "B31IOT-MQTT";
const char* pass = "QWERTY1234";

float threshval = 25.0;

#define MQTT_HOST "broker.mqtt.cool"
#define MQTT_PORT 1883
#define MQTT_USER ""
#define MQTT_TOKEN ""
#define MQTT_DEVICEID "lukeESP4482"
#define MQTT_PUB_TOPIC "ThreshCheck4482"

// MQTT objects
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, wifiClient);

// Print reasons if WiFi fails
void printWiFiStatus(){
  wl_status_t st = WiFi.status();
  Serial0.print("WiFi status: ");
  Serial0.println((int)st);
}

void setup() {
  Serial0.begin(115200);
  delay(100);
  
  // Connect to WiFi
  Serial0.println("Connecting to WiFi...");
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
  } else {
    Serial0.println("\nWiFi connection failed!");
    return;
  }
  
  // Set MQTT server
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  
  // Connect to MQTT broker
  Serial0.print("Connecting to MQTT broker at ");
  Serial0.print(MQTT_HOST);
  Serial0.print(":");
  Serial0.println(MQTT_PORT);
  
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial0.println("MQTT Connected");
  } else {
    Serial0.print("MQTT Failed to connect! State: ");
    Serial0.println(mqtt.state());
    Serial0.println("Error codes: -4=timeout, -3=connection lost, -2=connect failed, -1=disconnected, 0=connected");
  }
}

void loop() {
  // connect to mqtt
  while (!mqtt.connected()) {
    Serial0.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial0.println("MQTT Connected");
    } else {
      Serial0.print("MQTT Failed to connect! State: ");
      Serial0.println(mqtt.state());
      delay(5000);
    }
  }
  
  // Maintain MQTT connection
  mqtt.loop();

  // publish the threshold value
  StaticJsonDocument<100> jsonDoc;
  JsonObject payload = jsonDoc.to<JsonObject>();
  payload["thresh"] = threshval;
  char msg[100];
  serializeJson(jsonDoc, msg, sizeof(msg));
  Serial0.print("Publishing to ");
  Serial0.print(MQTT_PUB_TOPIC);
  Serial0.print(": ");
  Serial0.println(msg);
  mqtt.publish(MQTT_PUB_TOPIC, msg);
  delay(10000);
}
