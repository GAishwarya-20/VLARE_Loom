#include "board_config.h"
#include "communication.h"
#include "bluetooth_handler.h"
#include "wifi_handler.h"
#include "file_handler.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- VLARE Loom Firmware Initializing ---");

  // Initialize the file system first
  setupFileSystem();
  
  // Initialize Bluetooth (either Classic or BLE based on config)
  setupBluetooth();

  // Initialize Wi-Fi (either AP or Station based on config)
  setupWiFi();

  Serial.println("--- System Ready ---");
}

void loop() {
  // Handle incoming Bluetooth data (commands)
  handleBluetooth();
  
  // Handle incoming Wi-Fi data (file transfers over TCP)
  // This needs to be called continuously to accept clients and receive data.
  handleWiFiClient();

  // Add any other recurring tasks here.
  // Using delay() is not recommended as it blocks execution.
}