#include "file_handler.h"
#include "FS.h"
#include "SPIFFS.h"

File currentFile;
String bodyFilename = "nofile";
String borderFilename = "nofile";

void setupFileSystem() {
    if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    Serial.println("SPIFFS mounted successfully.");
}

bool prepareForFileTransfer(const char* filename, byte fileType) {
    String path = "/";
    path += filename;

    // Delete the old file if it exists, then open a new one for writing
    if (SPIFFS.exists(path)) {
        SPIFFS.remove(path);
    }
    
    currentFile = SPIFFS.open(path, FILE_WRITE);
    if (!currentFile) {
        Serial.println("Failed to open file for writing");
        return false;
    }
    
    if (fileType == 0x01) { // BODY
        bodyFilename = filename;
    } else if (fileType == 0x02) { // BORDER
        borderFilename = filename;
    }

    return true;
}

void writeFileChunk(byte* buffer, size_t len) {
    if (currentFile) {
        currentFile.write(buffer, len);
    }
}

void finalizeFileTransfer() {
    if (currentFile) {
        currentFile.close();
        Serial.println("File transfer finalized.");
    }
}

// Function to get the total horizontal rows (height) from a BMP file header.
// A BMP header is well-defined. Image height is a 4-byte little-endian integer at offset 22 (0x16).
uint32_t getBmpTotalPicks(const char* filename) {
    String path = "/";
    path += filename;

    if (!SPIFFS.exists(path) || filename == "nofile") {
        return 0;
    }

    File file = SPIFFS.open(path, FILE_READ);
    if (!file) {
        return 0;
    }

    // Seek to the position where height is stored (22 bytes from the start)
    if (!file.seek(22)) {
        file.close();
        return 0;
    }
    
    // Read the 4 bytes for height
    byte heightBytes[4];
    if (file.read(heightBytes, 4) != 4) {
        file.close();
        return 0;
    }
    
    file.close();

    // Convert the 4 little-endian bytes to a uint32_t
    return (uint32_t)(heightBytes[0] | (heightBytes[1] << 8) | (heightBytes[2] << 16) | (heightBytes[3] << 24));
}


void getFileStatus(byte* buffer) {
    // Get total picks (image height)
    uint32_t bodyTotalPicks = getBmpTotalPicks(bodyFilename.c_str());
    uint32_t borderTotalPicks = getBmpTotalPicks(borderFilename.c_str());
    
    // Current picks would be tracked during operation. For now, we set to 0.
    uint32_t bodyCurrentPicks = 0;
    uint32_t borderCurrentPicks = 0;

    // Populate the buffer according to the guide's format
    // Body Total Picks (4 bytes)
    memcpy(buffer, &bodyTotalPicks, 4);
    // Body Current Picks (4 bytes)
    memcpy(buffer + 4, &bodyCurrentPicks, 4);
    // Body Name (16 bytes) - pad with zeros
    memset(buffer + 8, 0, 16);
    strncpy((char*)(buffer + 8), bodyFilename.c_str(), 15);
    
    // Border Total Picks (4 bytes)
    memcpy(buffer + 24, &borderTotalPicks, 4);
    // Border Current Picks (4 bytes)
    memcpy(buffer + 28, &borderCurrentPicks, 4);
    // Border Name (16 bytes) - pad with zeros
    memset(buffer + 32, 0, 16);
    strncpy((char*)(buffer + 32), borderFilename.c_str(), 15);
}