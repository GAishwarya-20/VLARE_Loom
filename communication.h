#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <Arduino.h>

// Packet constants based on the guide
const byte HEADER = 0xA5;
const byte FOOTER = 0x5A;
const int PACKET_BUFFER_SIZE = 32; // Max packet size as per your request


// Command definitions from the guide
enum Command : byte {
    CMD_CONNECT_WIFI = 0x10,
    CMD_SEND_WIFI_PASS = 0x11,
    CMD_CONFIRM_WIFI_CONN = 0x12,
    CMD_GET_FILE_STATUS = 0x13,
    CMD_START_FILE_TRANSFER = 0x14,
    CMD_END_FILE_TRANSFER = 0x15,
    CMD_SET_PICK_VALUE = 0x16,
    CMD_SET_RPM = 0x17
};

// Error codes for responses
enum ResponseError : byte {
    SUCCESS = 0x00,
    LENGTH_MISMATCH = 0x01,
    CRC_CHECK_FAILED = 0x02
};

// Function prototypes
void processIncomingPacket(byte* buffer, int len);
void sendResponsePacket(byte command, byte* data, byte dataLength);
byte calculateCRC(const byte* data, int len);

#endif // COMMUNICATION_H