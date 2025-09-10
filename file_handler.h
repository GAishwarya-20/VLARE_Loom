#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

#include <Arduino.h>

void setupFileSystem();
bool prepareForFileTransfer(const char* filename, byte fileType);
void writeFileChunk(byte* buffer, size_t len);
void finalizeFileTransfer();
void getFileStatus(byte* buffer);
uint32_t getBmpTotalPicks(const char* filename);

#endif