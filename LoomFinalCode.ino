#include <WiFi.h>
#include <LittleFS.h>
#include "BluetoothSerial.h"

//======================================================================
// *********************** Protocol Definitions ************************
//======================================================================
#define HEADER 0xA5
#define FOOTER 0x5A

struct Packet {
  uint8_t command;
  uint8_t length;
  uint8_t data[256];
};

//======================================================================
// ************************ Command Mapping Table **********************
//======================================================================
struct CommandMap {
  const char* name;
  uint8_t code;
};

CommandMap cmdTable[] = {
  {"CMD_GET_FILE_STATUS",     0x13},
  {"CMD_START_FILE_TRANSFER", 0x14},
  {"CMD_END_FILE_TRANSFER",   0x15},
  {"CMD_SET_PICK",            0x16},
  {"CMD_SET_RPM",             0x17},
};

String bodyFile = "body.bmp";
String borderFile = "border.bmp";

uint32_t bodyTotalPicks   = 1000;
uint32_t bodyCurrentPicks = 200;
uint32_t borderTotalPicks = 500;
uint32_t borderCurrentPicks = 50;
int currentRPM = 0;

//======================================================================
// ************************* Wi-Fi Configuration ***********************
//======================================================================
const char* ssid = "VLARE_LOOM_ESP32_AP";
const char* password = "12345678";
const int   tcp_port = 21234;

WiFiServer server(tcp_port);
WiFiClient wifiClient; // Global Wi-Fi client

// The device IP must be different from the gateway IP.
IPAddress local_IP(192, 168, 4, 10); // ESP32 AP IP
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// ---------- Wi-Fi transfer state ----------
bool  isWiFiTransferInProgress = false;
File  wifiReceivedFile;
long  wifiTotalBytesReceived = 0;

// Streaming end-command detector (find "CMD_END_FILE_TRANSFER\n" inside binary)
const char* WIFI_END_CMD = "CMD_END_FILE_TRANSFER\n";
const int   WIFI_END_CMD_LEN = 22;              // length of the above string
int   wifiMatchIndex = 0;                       // how many chars matched so far
char  wifiEndBuf[WIFI_END_CMD_LEN];             // holds matched chars until confirmed

// Where to store received file
const char* WIFI_RX_PATH = "/wifi_received.bin";

//======================================================================
// *********************** Bluetooth Configuration *********************
//======================================================================
BluetoothSerial SerialBT;
const char* bt_device_name = "VLARE_ESP32_BT";

// BT transfer flags (kept from your code — not used in this flow)
bool  isBluetoothTransferInProgress = false;
File  btReceivedFile;
long  btTotalBytesReceived = 0;
bool  isBmpFile = false;
unsigned long btLastDataTime = 0;
const unsigned long BT_TRANSFER_TIMEOUT = 5000; // 5s

//======================================================================
// *********************** Parse String -> Packet **********************
//======================================================================
int getCommandCode(const String &cmd) {
  for (auto &entry : cmdTable) {
    if (cmd.equalsIgnoreCase(entry.name)) return entry.code;
  }
  return -1; // not found
}

bool parseStringCommand(const String &line, Packet &pkt) {
  int firstSpace = line.indexOf(' ');
  String cmdName = (firstSpace == -1) ? line : line.substring(0, firstSpace);
  String args    = (firstSpace == -1) ? ""   : line.substring(firstSpace + 1);

  int cmdCode = getCommandCode(cmdName);
  if (cmdCode == -1) return false;

  pkt.command = cmdCode;
  pkt.length  = 0;

  if (cmdCode == 0x16) { // CMD_SET_PICK <fileType> <pickValue>
    int spacePos = args.indexOf(' ');
    if (spacePos == -1) return false;
    pkt.data[0] = args.substring(0, spacePos).toInt();
    pkt.data[1] = args.substring(spacePos + 1).toInt();
    pkt.length  = 2;
  } else if (cmdCode == 0x17) { // CMD_SET_RPM <value>
    pkt.data[0] = args.toInt();
    pkt.length  = 1;
  } else {
    // 0x13/0x14/0x15 have no args
  }
  return true;
}

//======================================================================
// *************************** Utilities *******************************
//======================================================================
// sum of data bytes (LSB)
uint8_t calculateCRC(const uint8_t *data, uint8_t length) {
  uint16_t sum = 0;
  for (int i = 0; i < length; i++) sum += data[i];
  return (uint8_t)(sum & 0xFF);
}

// Plain-text SUCCESS/FAILURE responses
void sendResponse(Stream &out, uint8_t command, uint8_t status, const char* msg = "") {
  String response;

  switch (command) {
    case 0x14: response = (status == 0x01) ? "START_FILE_TRANSFER SUCCESS"
                                           : "START_FILE_TRANSFER FAILURE"; break;
    case 0x15: response = (status == 0x01) ? "END_FILE_TRANSFER SUCCESS"
                                           : "END_FILE_TRANSFER FAILURE";   break;
    case 0x13: response = (status == 0x01) ? "GET_FILE_STATUS SUCCESS"
                                           : "GET_FILE_STATUS FAILURE";     break;
    case 0x16: response = (status == 0x01) ? "SET_PICK SUCCESS"
                                           : "SET_PICK FAILURE";            break;
    case 0x17: response = (status == 0x01) ? "SET_RPM SUCCESS"
                                           : "SET_RPM FAILURE";             break;
    default:   response = "UNKNOWN_COMMAND FAILURE";                        break;
  }

  out.println(response);
  Serial.printf("[RESP] CMD:0x%02X STATUS:0x%02X -> %s %s\n",
                command, status, response.c_str(), msg);
}

//======================================================================
// ************************* Command Processor *************************
// (used for non-file commands and for BT path)
//======================================================================
void processCommand(Stream &client, const Packet &pkt) {
  switch (pkt.command) {
    case 0x13: // GET_FILE_STATUS
      sendResponse(client, pkt.command, 0x01, "File status OK");
      break;

    case 0x14: // START_FILE_TRANSFER (generic; Wi-Fi path handles file I/O itself)
      sendResponse(client, pkt.command, 0x01, "File transfer started");
      break;

    case 0x15: // END_FILE_TRANSFER (generic; Wi-Fi path sends size)
      sendResponse(client, pkt.command, 0x01, "File transfer ended");
      break;

    case 0x16: // SET_PICK <fileType> <pickValue>
      if (pkt.data[1] > 0) sendResponse(client, pkt.command, 0x01, "Pick value set");
      else                 sendResponse(client, pkt.command, 0x00, "Invalid pick value");
      break;

    case 0x17: // SET_RPM <value>
      if (pkt.data[0] > 0) sendResponse(client, pkt.command, 0x01, "RPM set");
      else                 sendResponse(client, pkt.command, 0x00, "Invalid RPM");
      break;

    default:
      sendResponse(client, pkt.command, 0x00, "Unknown command");
      break;
  }
}

//======================================================================
// ***************** Forward Declarations for handlers *****************
//======================================================================
void handleWiFiClient();
void handleBluetoothClient();
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);

//======================================================================
// ***************************** SETUP *********************************
//======================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n--- ESP32 File Receiver ---");

  // 1) Filesystem
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS Mount Failed. System halted.");
    while (1);
  }
  Serial.println("[FS] LittleFS Mounted Successfully.");

  // 2) Wi-Fi AP
  Serial.println("[WiFi] Configuring Access Point...");
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("[WiFi] AP Config Failed");
  }
  WiFi.softAP(ssid, password);
  Serial.print("[WiFi] AP Started. IP Address: ");
  Serial.println(WiFi.softAPIP());

  // 3) TCP Server
  server.begin();
  Serial.printf("[WiFi] TCP Server listening on port %d\n", tcp_port);

  // 4) Bluetooth
  Serial.println("[BT] Starting Bluetooth...");
  SerialBT.begin(bt_device_name);
  SerialBT.register_callback(btCallback);
  Serial.printf("[BT] Bluetooth ready. Device name: %s\n", bt_device_name);

  Serial.println("\nSystem ready. Awaiting connections via Wi-Fi or Bluetooth.");
}

//======================================================================
// ****************************** LOOP *********************************
//======================================================================
void loop() {
  handleWiFiClient();
  handleBluetoothClient();
  delay(1);
}

//======================================================================
// *********************** Wi-Fi CLIENT HANDLER ************************
//  - Reads text commands when idle
//  - Streams raw bytes to LittleFS between START and END
//  - Detects "CMD_END_FILE_TRANSFER\n" inside the binary stream
//======================================================================
void handleWiFiClient() {
  // Accept new client if none or previous disconnected
  if (!wifiClient || !wifiClient.connected()) {
    WiFiClient newClient = server.available();
    if (newClient) {
      wifiClient = newClient;
      Serial.print("[WiFi] New client connected. IP: ");
      Serial.println(wifiClient.remoteIP());
    }
    return;
  }

  if (!wifiClient.available()) return;

  // --------- If a transfer is ongoing, treat incoming bytes as file data ---------
  if (isWiFiTransferInProgress) {
    while (wifiClient.available()) {
      int ib = wifiClient.read();
      if (ib < 0) break;
      char b = (char)ib;

      if (b == WIFI_END_CMD[wifiMatchIndex]) {
        // Progress match
        wifiEndBuf[wifiMatchIndex++] = b;

        if (wifiMatchIndex == WIFI_END_CMD_LEN) {
          // Found the end marker -> finalize
          wifiReceivedFile.flush();
          wifiReceivedFile.close();
          isWiFiTransferInProgress = false;
          wifiMatchIndex = 0;

          // Reply with SUCCESS and byte count on a single line
          wifiClient.print("END_FILE_TRANSFER SUCCESS:");
          wifiClient.print(wifiTotalBytesReceived);
          wifiClient.print("\n");

          Serial.printf("[WiFi] File received. Size: %ld bytes\n", wifiTotalBytesReceived);
          break; // leave streaming mode; next loop will handle further commands
        }
      } else {
        // Mismatch -> write any matched prefix to file, then this byte
        if (wifiMatchIndex > 0) {
          wifiReceivedFile.write((const uint8_t*)wifiEndBuf, wifiMatchIndex);
          wifiTotalBytesReceived += wifiMatchIndex;
          wifiMatchIndex = 0;
        }
        wifiReceivedFile.write((const uint8_t*)&b, 1);
        wifiTotalBytesReceived += 1;
      }
    }
    return;
  }

  // --------- Idle: parse incoming line as a command ---------
  String line = wifiClient.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  Serial.println("[WiFi] Client is connected and data available");
  Serial.println(String("[WiFi] CMD: ") + line);

  Packet pkt;
  if (!parseStringCommand(line, pkt)) {
    Serial.println("[WiFi] Unknown command: " + line);
    return;
  }

  if (pkt.command == 0x14) {
    // START_FILE_TRANSFER
    if (isWiFiTransferInProgress) {
      // Already in progress -> reject
      sendResponse(wifiClient, pkt.command, 0x00, "Already in progress");
      return;
    }

    LittleFS.remove(WIFI_RX_PATH);
    wifiReceivedFile = LittleFS.open(WIFI_RX_PATH, "w");
    if (!wifiReceivedFile) {
      sendResponse(wifiClient, pkt.command, 0x00, "Open file failed");
      return;
    }

    wifiTotalBytesReceived = 0;
    wifiMatchIndex = 0;
    isWiFiTransferInProgress = true;

    sendResponse(wifiClient, pkt.command, 0x01, "File transfer started");
    Serial.println("[WiFi] START_FILE_TRANSFER: ready to receive data...");
  }
  else if (pkt.command == 0x15) {
    // END before any data -> fail
    if (!isWiFiTransferInProgress) {
      // Keep string format for consistency
      wifiClient.println("END_FILE_TRANSFER FAILURE");
      Serial.println("[WiFi] END received but no transfer in progress");
    } else {
      // If sender chose to send END as a plain line (not in-stream),
      // close now and return size.
      wifiReceivedFile.flush();
      wifiReceivedFile.close();
      isWiFiTransferInProgress = false;
      wifiMatchIndex = 0;

      wifiClient.print("END_FILE_TRANSFER SUCCESS:");
      wifiClient.print(wifiTotalBytesReceived);
      wifiClient.print("\n");

      Serial.printf("[WiFi] END received (line). Size: %ld bytes\n", wifiTotalBytesReceived);
    }
  }
  else {
    // Other commands use generic processor
    processCommand(wifiClient, pkt);
  }
}

//======================================================================
// ************ BLUETOOTH CLIENT HANDLER (string commands) *************
//======================================================================
void handleBluetoothClient() {
  if (!SerialBT.available()) return;

  String line = SerialBT.readStringUntil('\n');
  line.trim();

  Packet pkt;
  if (parseStringCommand(line, pkt)) {
    // For BT we only send the SUCCESS/FAILURE strings; no file I/O here.
    processCommand(SerialBT, pkt);
  } else {
    Serial.println("[BT] Unknown command: " + line);
  }
}

//======================================================================
// ******************** Bluetooth connection events ********************
//======================================================================
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println("[BT] Client Connected");
  } else if (event == ESP_SPP_CLOSE_EVT) {
    Serial.println("[BT] Client disconnected");
    if (isBluetoothTransferInProgress) {
      btReceivedFile.close();
      isBluetoothTransferInProgress = false;
      Serial.printf("[BT] Transfer completed on disconnect. Total bytes: %ld\n", btTotalBytesReceived);
      isBmpFile = false;
    }
  }
}
