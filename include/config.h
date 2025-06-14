#ifndef LOCAL_CONFIG_H
#define LOCAL_CONFIG_H

#ifdef __has_include
#if __has_include("config_private.h")
#include "config_private.h"
#endif
#endif

#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef CONFIG_WIFI_PASS
#define CONFIG_WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

#ifndef CONFIG_MQTT_HOST
#define CONFIG_MQTT_HOST "mqtt.local"
#endif

#ifndef CONFIG_MQTT_PORT
#define CONFIG_MQTT_PORT 1883
#endif

#ifndef CONFIG_MQTT_USER
#define CONFIG_MQTT_USER ""
#endif

#ifndef CONFIG_MQTT_PASS
#define CONFIG_MQTT_PASS ""
#endif

// Pin assignments for the Wemos LOLIN S2 mini board
// SCL   GPIO7  - grey
// SDA   GPIO11 - purple
// CS    GPIO12 - green
// DC    GPIO6  - blue
// RST   GPIO13 - yellow
// BL (backlight, if available) GPIO5
// AHT10 SDA GPIO8
// AHT10 SCL GPIO9

#endif // LOCAL_CONFIG_H
