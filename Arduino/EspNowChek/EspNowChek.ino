// libraries

#include "WiFi.h"


void setup() {
  Serial0.begin(115200);
  WiFi.mode(WIFI_STA);
  // add delay for wifi
  delay(100);
  Serial0.println(WiFi.macAddress());

}

void loop() {
}
