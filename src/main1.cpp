#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "services\wserial.h"

static bool networkServicesStarted = false;

void setup() {
  WiFi.begin("InovaIndustria","industria50");
  wserial.begin(115200, 47268UL);
}
void loop() {
  if (WiFi.status() == WL_CONNECTED && !networkServicesStarted) {
    networkServicesStarted = true;
    wserial.println("[IP] is " + String(WiFi.localIP().toString()));
    MDNS.begin("KIT_HOSTNAME");
  }
  wserial.update();
}
