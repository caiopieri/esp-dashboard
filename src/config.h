#pragma once

// Wi-Fi Configuration
#define WIFI_SSID     "SEU_WIFI_AQUI"
#define WIFI_PASSWORD "SUA_SENHA_AQUI"

// Product/OTA metadata. Keep OTA password out of source control; provide
// -D DESK_OTA_PASSWORD=... in a private PlatformIO build configuration when
// using the development ArduinoOTA transport.
#define DESK_FIRMWARE_VERSION "0.2.0-dev"

// Hardware settings
#define SCREEN_BRIGHTNESS 220 // 0 to 255
