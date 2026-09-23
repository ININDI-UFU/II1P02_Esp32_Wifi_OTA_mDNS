#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "services\wserial.h"

const char *ssid = "InovaIndustria";
const char *password = "industria50";
const char *hostName = KIT_HOSTNAME;

void setup() {
  #if CONFIG_ETH_USE_OPENETH
    startOpenEth();   // QEMU + lab-router
  #else
    WiFi.begin(ssid, password);
  #endif
  while (WiFi.status() != WL_CONNECTED) delay(100);

  // Tenta listen até conseguir
  wserial.begin(115200, 47268UL);
  wserial.println("[IP] is " + String(WiFi.localIP().toString()));

  if (!MDNS.begin(hostName)) wserial.println("[mDNS] begin failed");
  else wserial.println("[mDNS] begin in " + String(hostName));

  ArduinoOTA
      // .onStart([]() {Serial.println("[OTA] Start");})
      // .onEnd([]() {Serial.println("[OTA] End"); })
      // .onProgress([](unsigned int p, unsigned int t) {Serial.println("[OTA] " + String((p*100)/t));})
      // .onError([](ota_error_t e) { Serial.println("[OTA] Error " + String(e)); })
      .setHostname(hostName) //Tem que ter para poder fazer o download pelo nome
      .begin();
}

void loop() {
  ArduinoOTA.handle();
  wserial.update();
}