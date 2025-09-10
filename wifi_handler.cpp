#include "wifi_handler.h"
#include "board_config.h"
#include "file_handler.h"

// Include WiFi library only if a Wi-Fi enabled board is selected
#if defined(VLARE_CBLE_AP) || defined(VLARE_CBLE_IOT)
#include <WiFi.h>

WiFiServer tcpServer(WIFI_TCP_PORT);
WiFiClient client;
String currentSSID;

#endif

void setupWiFi() {
#if defined(VLARE_CBLE_AP)
    // --- Wi-Fi Access Point Mode ---
    Serial.println("Starting Wi-Fi in AP Mode...");
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    tcpServer.begin();
    Serial.printf("TCP Server started on port %d\n", WIFI_TCP_PORT);
#elif defined(VLARE_CBLE_IOT)
    // --- Wi-Fi Station Mode ---
    // We don't connect here. We wait for commands.
    Serial.println("Wi-Fi is in Station mode, awaiting credentials...");
#endif
}

void connectToWiFi(const char* password) {
#if defined(VLARE_CBLE_IOT)
    Serial.printf("Attempting to connect to SSID: %s\n", currentSSID.c_str());
    WiFi.begin(currentSSID.c_str(), password);
    // In a real application, you'd handle the connection timeout here.
    // For now, we assume it will connect. The mobile app will poll for status.
#endif
}

byte getWifiConnectionStatus() {
#if defined(VLARE_CBLE_IOT)
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Wi-Fi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        tcpServer.begin(); // Start server only after connection is successful
        Serial.printf("TCP Server started on port %d\n", WIFI_TCP_PORT);
        return 0xF0; // Wi-fi Connect Successfully
    } else {
        // You can return different error codes here based on WiFi.status()
        // See the guide for specific codes like 0xC8, 0xC9, 0xCA, 0xCB
        return 0xCA; // Authentication failure (example)
    }
#else
    return 0xF0; // For AP mode, it's always "connected"
#endif
}


void handleWiFiClient() {
#if defined(VLARE_CBLE_AP) || defined(VLARE_CBLE_IOT)
    if (!client || !client.connected()) {
        client = tcpServer.available();
        if (!client) {
            return;
        }
        Serial.println("New TCP client connected!");
    }
    
    // If we have a client, check for incoming data (the file bytes)
    if (client.available()) {
        // Read file bytes and write them using the file handler
        // This is a simple implementation. A more robust one would handle chunking.
        byte buffer[512];
        int bytesRead = client.read(buffer, sizeof(buffer));
        if (bytesRead > 0) {
            writeFileChunk(buffer, bytesRead);
        }
    }
#endif
}