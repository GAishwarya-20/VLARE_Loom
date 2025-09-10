#include "bluetooth_handler.h"
#include "board_config.h"
#include "communication.h"

// Use preprocessor directives to include the correct library
#if defined(VLARE_MCB) || defined(VLARE_CCB)
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;  // Make sure the object is accessible here

void bt_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println("Bluetooth Client Connected");
  } else if (event == ESP_SPP_CLOSE_EVT) {
    Serial.println("Bluetooth Client Disconnected");
  }
}

#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- BLE Server Callbacks (Connect/Disconnect) ---
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println(">>>> BLE Client Connected <<<<");
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println(">>>> BLE Client Disconnected <<<<");
    // When a client disconnects, we must start advertising again
    // so a new connection can be made.
    Serial.println("Restarting BLE advertising...");
    pServer->getAdvertising()->start();
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      Serial.println("*********");
      Serial.print("Received Value: ");
      for (int i = 0; i < value.length(); i++)
        Serial.printf("%02X ", value[i]);
      Serial.println();
      Serial.println("*********");

      // Process the received data
      processIncomingPacket((byte*)value.c_str(), value.length());
    }
  }
};

#endif

byte packetBuffer[PACKET_BUFFER_SIZE];
int bufferIndex = 0;
int expectedPacketLength = 0;

void setupBluetooth() {
#if defined(VLARE_MCB) || defined(VLARE_CCB)
  // --- Classic Bluetooth (SPP) Setup ---
  SerialBT.begin(BLUETOOTH_DEVICE_NAME);
  Serial.printf("Classic Bluetooth device \"%s\" is ready to pair.\n", BLUETOOTH_DEVICE_NAME);

  // --- ADD THIS LINE ---
  SerialBT.register_callback(bt_callback);  // Register the connection status callback
#else
  // --- BLE Setup ---
  BLEDevice::init(BLUETOOTH_DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  BLEDevice::startAdvertising();
  Serial.printf("BLE device \"%s\" is advertising.\n", BLUETOOTH_DEVICE_NAME);
#endif
}

void handleBluetooth() {
#if defined(VLARE_MCB) || defined(VLARE_CCB)
  // --- Classic Bluetooth Data Handling ---
  while (SerialBT.available()) {
    byte inByte = SerialBT.read();

    // --- STATE 1: WAITING FOR A HEADER ---
    if (bufferIndex == 0) {
      if (inByte == HEADER) {
        // Found the start of a potential packet
        packetBuffer[bufferIndex++] = inByte;
        expectedPacketLength = 0;  // Reset expected length
      }
      // If it's not a header, we do nothing and just discard the byte.
      continue;  // Move to the next byte in the serial buffer
    }

    // --- STATE 2: BUILDING A PACKET ---
    // If we are here, it means we have already received a HEADER.
    if (bufferIndex < PACKET_BUFFER_SIZE) {
      packetBuffer[bufferIndex++] = inByte;
    } else {
      // Buffer overflow. This packet is invalid. Reset everything.
      Serial.println("ERROR: Packet buffer overflow. Discarding.");
      bufferIndex = 0;
      expectedPacketLength = 0;
      continue;
    }

    // --- LOGIC TO DETERMINE TOTAL PACKET LENGTH ---
    // Once we have the first 3 bytes, we can read the data length field.
    if (bufferIndex == 3) {
      byte dataLength = packetBuffer[2];
      // Total length = 5 bytes of protocol overhead + the data payload length
      expectedPacketLength = 5 + dataLength;

      // Sanity check: is the expected packet too big for our buffer?
      if (expectedPacketLength > PACKET_BUFFER_SIZE) {
        Serial.printf("ERROR: Declared packet length (%d) exceeds buffer size. Discarding.\n", expectedPacketLength);
        bufferIndex = 0;
        expectedPacketLength = 0;
        continue;
      }
    }

    // --- CHECK FOR COMPLETION ---
    // Only proceed if we have determined the expected length and have received that many bytes.
    if (expectedPacketLength > 0 && bufferIndex == expectedPacketLength) {
      // We have received the complete packet. Now, validate the footer.
      if (packetBuffer[bufferIndex - 1] == FOOTER) {
        // Footer is correct! Process the packet.
        Serial.printf("Complete packet received with length %d. Processing...\n", bufferIndex);
        processIncomingPacket(packetBuffer, bufferIndex);
      } else {
        // The last byte was not a footer. The packet is corrupt.
        Serial.printf("ERROR: Packet received, but footer is incorrect. Expected 0x5A, got 0x%02X. Discarding.\n", packetBuffer[bufferIndex - 1]);
      }

      // Reset for the next packet, whether it was valid or not.
      bufferIndex = 0;
      expectedPacketLength = 0;
    }
  }
#else
  // --- BLE Handling ---
  // BLE is handled via callbacks, so this function is for future use,
  // like checking connection status and re-advertising if needed.
  if (!deviceConnected) {
    // Optional: you could start advertising again if it stops
  }
#endif
}

void sendBluetoothData(const byte* data, size_t len) {
  Serial.print("Sending Response: ");
  for (int i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
  Serial.println();

#if defined(VLARE_MCB) || defined(VLARE_CCB)
  SerialBT.write(data, len);
#else
  if (deviceConnected) {
    pCharacteristic->setValue((uint8_t*)data, len);
    pCharacteristic->notify();
  }
#endif
}