#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// --- BOARD SELECTION ---
// Uncomment ONLY ONE of the following lines to select the board configuration.

#define VLARE_MCB         // Mother Board Classic Bluetooth
// #define VLARE_CCB         // Control Board Classic Bluetooth
// #define VLARE_CBLE_AP     // Control Board BLE + Wi-Fi Access Point
// #define VLARE_CBLE_IOT    // Control Board BLE + Wi-Fi Station (IoT)


// --- BLUETOOTH CONFIGURATIONS ---
#if defined(VLARE_MCB)
    #define BLUETOOTH_DEVICE_NAME "VLARE_MCB"
#elif defined(VLARE_CCB)
    #define BLUETOOTH_DEVICE_NAME "VLARE_CCB"
#elif defined(VLARE_CBLE_AP)
    #define BLUETOOTH_DEVICE_NAME "VLARE_CBLE_AP"
#elif defined(VLARE_CBLE_IOT)
    #define BLUETOOTH_DEVICE_NAME "VLARE_CBLE_IOT"
#endif

// --- WIFI CONFIGURATIONS (for AP mode) ---
#if defined(VLARE_CBLE_AP)
    #define WIFI_AP_SSID "VLARE_LOOM_AP"
    #define WIFI_AP_PASSWORD "12345678"
    #define WIFI_TCP_PORT 8080
#endif

// --- WIFI CONFIGURATIONS (for Station mode) ---
#if defined(VLARE_CBLE_IOT)
    #define WIFI_TCP_PORT 8080
#endif

#endif // BOARD_CONFIG_H