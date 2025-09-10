#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>

void setupWiFi();
void handleWiFiClient();
void connectToWiFi(const char* password);
byte getWifiConnectionStatus();

#endif // WIFI_HANDLER_H