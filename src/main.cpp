#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_AHTX0.h>
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;
public:
  LGFX(void) {
    auto cfg = _bus.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.freq_read  = 16000000;
    cfg.dma_channel = 1;
    cfg.pin_sclk = 7;
    cfg.pin_mosi = 11;
    cfg.pin_miso = -1;
    cfg.pin_dc   = 6;
    _bus.config(cfg);
    _panel.setBus(&_bus);

    auto p_cfg = _panel.config();
    p_cfg.invert = true;
    p_cfg.panel_width = 240;
    p_cfg.panel_height = 240;
    p_cfg.pin_cs  = 12;
    p_cfg.pin_rst = 13;
    _panel.config(p_cfg);

    auto l_cfg = _light.config();
    l_cfg.pin_bl = 5;
    l_cfg.freq   = 44100;
    l_cfg.pwm_channel = 7;
    _light.config(l_cfg);
    _panel.setLight(&_light);

    setPanel(&_panel);
  }
};

LGFX tft;
LGFX_Sprite canvas(&tft);

Adafruit_AHTX0 aht;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

static const int CENTER = 120;
static const int RADIUS = 120;

struct Arc {
  float value = 0.0f;
  float min = 0.0f;
  float max = 100.0f;
  int width = 8;
  uint16_t color = TFT_WHITE;
};

struct Dial {
  float value = 0.0f;
  float min = 0.0f;
  float max = 100.0f;
  uint16_t color = TFT_RED;
};

static Arc arcs[3];
static Dial dial;

static uint16_t parseColor(const String& str) {
  String s = str;
  if (s.startsWith("#")) s.remove(0, 1);
  if (s.startsWith("0x") || s.startsWith("0X")) s.remove(0, 2);
  uint32_t rgb = strtoul(s.c_str(), nullptr, 16);
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void handleArc(int idx, JsonVariantConst obj) {
  if (obj["value"].is<float>()) arcs[idx].value = obj["value"].as<float>();
  if (obj["min"].is<float>()) arcs[idx].min = obj["min"].as<float>();
  if (obj["max"].is<float>()) arcs[idx].max = obj["max"].as<float>();
  if (obj["width"].is<int>()) arcs[idx].width = obj["width"].as<int>();
  if (obj.containsKey("color")) arcs[idx].color = parseColor(obj["color"].as<String>());
}

static void handleDial(JsonVariantConst obj) {
  if (obj["value"].is<float>()) dial.value = obj["value"].as<float>();
  if (obj["min"].is<float>()) dial.min = obj["min"].as<float>();
  if (obj["max"].is<float>()) dial.max = obj["max"].as<float>();
  if (obj.containsKey("color")) dial.color = parseColor(obj["color"].as<String>());
}

static void drawArcs() {
  int r = RADIUS;
  for (int i = 0; i < 3; ++i) {
    if (arcs[i].width <= 0) continue;
    int inner = r - arcs[i].width;
    float range = arcs[i].max - arcs[i].min;
    if (range <= 0) range = 1.0f;
    float pct = (arcs[i].value - arcs[i].min) / range;
    if (pct < 0) pct = 0; else if (pct > 1) pct = 1;
    float angle = pct * 360.0f - 90.0f;
    canvas.fillArc(CENTER, CENTER, r, inner, -90, angle, arcs[i].color);
    r = inner - 2; // small gap
  }
}

static void drawDial() {
  int r = RADIUS - 40;
  canvas.drawCircle(CENTER, CENTER, r, TFT_WHITE);

  // draw tick marks every 10 units
  for (int i = 0; i <= 100; i += 10) {
    float a = i * 2.7f - 135.0f;
    float rad = a * DEG_TO_RAD;
    int x1 = CENTER + (int)(sinf(rad) * (r - 5));
    int y1 = CENTER - (int)(cosf(rad) * (r - 5));
    int x2 = CENTER + (int)(sinf(rad) * r);
    int y2 = CENTER - (int)(cosf(rad) * r);
    canvas.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  // label the major ticks
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  for (int i = 0; i <= 100; i += 50) {
    float a = i * 2.7f - 135.0f;
    float rad = a * DEG_TO_RAD;
    int xt = CENTER + (int)(sinf(rad) * (r - 15));
    int yt = CENTER - (int)(cosf(rad) * (r - 15));
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", i);
    canvas.drawString(buf, xt, yt);
  }

  float range = dial.max - dial.min;
  if (range <= 0) range = 1.0f;
  float pct = (dial.value - dial.min) / range;
  if (pct < 0) pct = 0; else if (pct > 1) pct = 1;
  float angle = pct * 270.0f - 135.0f;
  float rad = angle * DEG_TO_RAD;
  int x = CENTER + (int)(sinf(rad) * (r - 10));
  int y = CENTER - (int)(cosf(rad) * (r - 10));
  canvas.drawLine(CENTER, CENTER, x, y, dial.color);
}

static void updateDisplay() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" C");
  Serial.print("Dial value: ");
  Serial.println(dial.value);
  Serial.print("Arc values: ");
  Serial.print(arcs[0].value);
  Serial.print(", ");
  Serial.print(arcs[1].value);
  Serial.print(", ");
  Serial.println(arcs[2].value);
  canvas.fillScreen(TFT_BLACK);
  drawArcs();
  drawDial();
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1fC", temp.temperature);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString(buf, CENTER, 200);
  canvas.pushSprite(0, 0);
  mqtt.publish("ha_display/temperature", buf, true);
  Serial.println("Temperature published");
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String val;
  for (unsigned int i = 0; i < length; ++i) val += (char)payload[i];
  Serial.print("MQTT message ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(val);
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (!err) {
    Serial.print("Parsed JSON: ");
    serializeJson(doc, Serial);
    Serial.println();
    if (strcmp(topic, "ha_display/arc1") == 0) {
      handleArc(0, doc);
    } else if (strcmp(topic, "ha_display/arc2") == 0) {
      handleArc(1, doc);
    } else if (strcmp(topic, "ha_display/arc3") == 0) {
      handleArc(2, doc);
    } else if (strcmp(topic, "ha_display/dial") == 0) {
      handleDial(doc);
    }
  } else {
    float f = val.toFloat();
    if (strcmp(topic, "ha_display/arc1") == 0) {
      arcs[0].value = f;
    } else if (strcmp(topic, "ha_display/arc2") == 0) {
      arcs[1].value = f;
    } else if (strcmp(topic, "ha_display/arc3") == 0) {
      arcs[2].value = f;
    } else if (strcmp(topic, "ha_display/dial") == 0) {
      dial.value = f;
    }
  }
}

static void mqttConnect() {
  while (!mqtt.connected()) {
    String clientId = "esp32-display-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT...");
    bool connected = false;
    if (strlen(CONFIG_MQTT_USER) > 0) {
      connected = mqtt.connect(clientId.c_str(), CONFIG_MQTT_USER, CONFIG_MQTT_PASS);
    } else {
      connected = mqtt.connect(clientId.c_str());
    }
    if (connected) {
      Serial.println("connected");
      mqtt.subscribe("ha_display/arc1");
      mqtt.subscribe("ha_display/arc2");
      mqtt.subscribe("ha_display/arc3");
      mqtt.subscribe("ha_display/dial");
      Serial.println("MQTT subscriptions set");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }
  Serial.println("MQTT connection ready");
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup starting");
  tft.begin();
  tft.setRotation(0);
  tft.setBrightness(200);
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  canvas.createSprite(240, 240);
  canvas.setSwapBytes(true);

  Wire.begin(8, 9);
  aht.begin(&Wire);

  WiFi.begin(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);
  Serial.print("Connecting to WiFi: ");
  Serial.println(CONFIG_WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("WiFi connected. IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("MQTT server: ");
  Serial.print(CONFIG_MQTT_HOST);
  Serial.print(":");
  Serial.println(CONFIG_MQTT_PORT);
  mqtt.setServer(CONFIG_MQTT_HOST, CONFIG_MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  Serial.println("Setup complete");
}

void loop() {
  if (!mqtt.connected()) {
    Serial.println("MQTT disconnected, reconnecting");
    mqttConnect();
  }
  mqtt.loop();
  static unsigned long last = 0;
  if (millis() - last > 1000) {
    Serial.println("Updating display");
    updateDisplay();
    last = millis();
  }
}
