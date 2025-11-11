// libraries

#include "WiFi.h"


void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  // add delay for wifi
  delay(100);
  Serial.println(WiFi.macAddress());

}

void loop() {
}
