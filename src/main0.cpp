#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "services\wserial.h"


void setup() {
  #if CONFIG_ETH_USE_OPENETH
    startOpenEth();   // QEMU + lab-router
  #else
    WiFi.begin("InovaIndustria","industria50");
  #endif

  while (WiFi.status()!=WL_CONNECTED) delay(100);

    // Tenta listen até conseguir
  wserial.begin(115200, 47268UL);
  wserial.println("[IP] is " + String(WiFi.localIP().toString()));

  MDNS.begin(KIT_HOSTNAME);          // acessa em esp32name.local
}
void loop() {
    wserial.update();
}
