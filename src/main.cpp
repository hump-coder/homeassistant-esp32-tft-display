#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
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
  float value = 0.0f; // 0-100
  int width = 8;
  uint16_t color = TFT_WHITE;
};

static Arc arcs[3];
static float dialValue = 0.0f; // 0-100

static void drawArcs() {
  int r = RADIUS;
  for (int i = 0; i < 3; ++i) {
    if (arcs[i].width <= 0) continue;
    int inner = r - arcs[i].width;
    float angle = arcs[i].value * 3.6f - 90.0f;
    canvas.fillArc(CENTER, CENTER, r, inner, -90, angle, arcs[i].color);
    r = inner - 2; // small gap
  }
}

static void drawDial() {
  int r = RADIUS - 40;
  canvas.drawCircle(CENTER, CENTER, r, TFT_WHITE);
  float angle = dialValue * 2.7f - 135.0f;
  float rad = angle * DEG_TO_RAD;
  int x = CENTER + (int)(sinf(rad) * (r - 10));
  int y = CENTER - (int)(cosf(rad) * (r - 10));
  canvas.drawLine(CENTER, CENTER, x, y, TFT_RED);
}

static void updateDisplay() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  Serial.print("Temperature: ");
  Serial.print(temp.temperature);
  Serial.println(" C");
  Serial.print("Dial value: ");
  Serial.println(dialValue);
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
  float f = val.toFloat();
  if (strcmp(topic, "ha_display/arc1") == 0) {
    arcs[0].value = f;
    Serial.print("Arc1 set to ");
    Serial.println(f);
  } else if (strcmp(topic, "ha_display/arc2") == 0) {
    arcs[1].value = f;
    Serial.print("Arc2 set to ");
    Serial.println(f);
  } else if (strcmp(topic, "ha_display/arc3") == 0) {
    arcs[2].value = f;
    Serial.print("Arc3 set to ");
    Serial.println(f);
  } else if (strcmp(topic, "ha_display/dial") == 0) {
    dialValue = f;
    Serial.print("Dial set to ");
    Serial.println(f);
  }
}

static void mqttConnect() {
  while (!mqtt.connected()) {
    String clientId = "esp32-display-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT...");
    if (mqtt.connect(clientId.c_str())) {
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
