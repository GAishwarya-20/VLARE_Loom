#include "communication.h"
#include "wifi_handler.h"
#include "file_handler.h"
#include "bluetooth_handler.h"

// Calculate CRC: Sum of all data bytes, truncated to 1 byte (LSB)
byte calculateCRC(const byte* data, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return (byte)(sum & 0xFF); // Return the least significant byte
}

// Function to process a complete, received packet
void processIncomingPacket(byte* buffer, int len) {
    // 1. Basic Validation: Header, Footer, and Minimum Length (Header, Cmd, Len, CRC, Footer)
    if (len < 5 || buffer[0] != HEADER || buffer[len - 1] != FOOTER) {
        Serial.println("Invalid packet structure (Header/Footer/Length).");
        return;
    }

    byte command = buffer[1];
    byte dataLength = buffer[2];
    byte* data = &buffer[3];
    byte receivedCRC = buffer[3 + dataLength];

    // 2. Data Length Validation
    // The total length should be Header(1) + Cmd(1) + Len(1) + Data(dataLength) + CRC(1) + Footer(1)
    if (len != dataLength + 5) {
        Serial.println("Packet length mismatch.");
        byte errorData[] = { LENGTH_MISMATCH };
        sendResponsePacket(command, errorData, 1);
        return;
    }

    // 3. CRC Validation
    byte calculated_crc = calculateCRC(data, dataLength);
    if (calculated_crc != receivedCRC) {
        Serial.printf("CRC mismatch! Received: 0x%02X, Calculated: 0x%02X\n", receivedCRC, calculated_crc);
        byte errorData[] = { CRC_CHECK_FAILED };
        sendResponsePacket(command, errorData, 1);
        return;
    }

    Serial.printf("Packet Validated! Command: 0x%02X\n", command);

    // 4. Handle the Command using a switch statement
    switch (command) {
        case CMD_CONNECT_WIFI: {
            // Data contains SSID. Convert to string and pass to Wi-Fi handler
            char ssid[dataLength + 1];
            memcpy(ssid, data, dataLength);
            ssid[dataLength] = '\0'; // Null-terminate the string
            Serial.printf("Received Wi-Fi connect command for SSID: %s\n", ssid);
            // This is just acknowledging the SSID receipt. Password comes next.
            // According to the doc, a success response is sent before asking for password.
            byte successData[] = { SUCCESS };
            sendResponsePacket(command, successData, 1);
            break;
        }

        case CMD_SEND_WIFI_PASS: {
            char password[dataLength + 1];
            memcpy(password, data, dataLength);
            password[dataLength] = '\0';
            Serial.println("Received Wi-Fi password. Attempting to connect...");
            
            // Initiate Wi-Fi connection (this function should be non-blocking in a real scenario)
            connectToWiFi(password); 
            
            // The mobile app will send a final confirmation command (0x12) later.
            // The response here just confirms the password was received.
            byte successData[] = { SUCCESS };
            sendResponsePacket(command, successData, 1);
            break;
        }

        case CMD_CONFIRM_WIFI_CONN: {
            // Mobile app is asking for the final connection status
            byte status = getWifiConnectionStatus();
            byte responseData[] = { status };
            sendResponsePacket(command, responseData, 1);
            break;
        }

        case CMD_GET_FILE_STATUS: {
            Serial.println("Received Get File Status command.");
            // This command is complex, requiring body and border file info.
            // We need a buffer to hold the response data.
            // Format: 4b(BodyTotal) + 4b(BodyCurrent) + 16b(BodyName) + 4b(BorderTotal) + 4b(BorderCurrent) + 16b(BorderName)
            byte statusData[44];
            getFileStatus(statusData); // This function populates the buffer
            sendResponsePacket(command, statusData, sizeof(statusData));
            break;
        }

        case CMD_START_FILE_TRANSFER: {
            // Data[0] is file type (0x01=BODY, 0x02=BORDER)
            // Data[1..] is filename
            byte fileType = data[0];
            char fileName[dataLength]; // dataLength includes the type byte
            memcpy(fileName, &data[1], dataLength - 1);
            fileName[dataLength - 1] = '\0';
            
            Serial.printf("Start File Transfer. Type: %s, Name: %s\n", (fileType == 0x01 ? "BODY" : "BORDER"), fileName);
            
            // Prepare to receive the file over TCP
            if (prepareForFileTransfer(fileName, fileType)) {
                byte successData[] = { SUCCESS };
                sendResponsePacket(command, successData, 1);
            } else {
                // Handle error
            }
            break;
        }

        case CMD_END_FILE_TRANSFER: {
            // This is sent after the TCP transfer is complete.
            Serial.println("End File Transfer command received.");
            finalizeFileTransfer();
            byte successData[] = { SUCCESS };
            sendResponsePacket(command, successData, 1);
            break;
        }
        
        default:
            Serial.printf("Unknown command: 0x%02X\n", command);
            break;
    }
}

// Function to create and send a response packet
void sendResponsePacket(byte command, byte* data, byte dataLength) {
    byte responseBuffer[PACKET_BUFFER_SIZE];
    responseBuffer[0] = HEADER;
    responseBuffer[1] = command; // Echo the command
    responseBuffer[2] = dataLength;
    memcpy(&responseBuffer[3], data, dataLength);
    responseBuffer[3 + dataLength] = calculateCRC(data, dataLength);
    responseBuffer[4 + dataLength] = FOOTER;

    int responseLen = 5 + dataLength;
    
    // Now send this buffer over the active Bluetooth connection
    sendBluetoothData(responseBuffer, responseLen);
}