#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <Arduino.h>

void setupBluetooth();
void handleBluetooth();
void sendBluetoothData(const byte* data, size_t len);

#endif // BLUETOOTH_HANDLER_H