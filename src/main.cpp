#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <math.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;  // display driver class
  lgfx::Bus_SPI _bus;         // SPI bus
  lgfx::Light_PWM _light;     // backlight control

public:
  LGFX(void) {
    auto cfg = _bus.config();
    cfg.spi_host = SPI2_HOST;  // use general purpose SPI2 bus
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000; // 40MHz SPI write
    cfg.freq_read  = 16000000;
    cfg.dma_channel = 1;       // use DMA channel 1
    cfg.pin_sclk = 7;          // SCK
    cfg.pin_mosi = 11;         // MOSI
    cfg.pin_miso = -1;         // not used
    cfg.pin_dc   = 6;          // D/C
    _bus.config(cfg);
    _panel.setBus(&_bus);

    auto p_cfg = _panel.config();
    p_cfg.invert = true;       // typical for GC9A01
    p_cfg.panel_width = 240;
    p_cfg.panel_height = 240;
    p_cfg.offset_x = 0;
    p_cfg.offset_y = 0;
    p_cfg.pin_cs   = 12;       // chip select
    p_cfg.pin_rst  = 13;       // reset
    _panel.config(p_cfg);

    auto l_cfg = _light.config();
    l_cfg.pin_bl = 5;          // backlight pin
    l_cfg.invert = false;
    l_cfg.freq   = 44100;
    l_cfg.pwm_channel = 7;
    _light.config(l_cfg);
    _panel.setLight(&_light);

    setPanel(&_panel);
  }
};

LGFX tft;  // create display object
LGFX_Sprite canvas(&tft);  // off-screen buffer to reduce flicker

enum DemoMode {
  DEMO_STARFIELD,
  DEMO_CLOCK,
  DEMO_PLASMA,
  DEMO_ARC_CLOCK,
  DEMO_ARC_ANALOG_CLOCK,
  DEMO_AHT10,
  DEMO_CLOCK_TEMP,
  DEMO_TEMP_ARC_CLOCK
};

// Change this constant to pick which demo runs
static const DemoMode DEMO_MODE = DEMO_TEMP_ARC_CLOCK;

static const char* WIFI_SSID =  CONFIG_WIFI_SSID;
static const char* WIFI_PASS = CONFIG_WIFI_PASS;
static const long GMT_OFFSET_SEC = 9 * 3600 + 1800;  // +9.5h

static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

struct Star {
  float x;
  float y;
  float z;
};

static const int NUM_STARS = 80;
static Star stars[NUM_STARS];
static const float STAR_SPEED = 0.6f;
static const int CENTER = 120;
static const int RADIUS = 120;
static const int ANALOG_RADIUS = 65;
static const int AHT_SDA = 8;
static const int AHT_SCL = 9;
Adafruit_AHTX0 aht;

static void resetStar(Star &s) {
  s.x = random(-RADIUS, RADIUS);
  s.y = random(-RADIUS, RADIUS);
  s.z = random(1, RADIUS);
}

static void initStars() {
  for (int i = 0; i < NUM_STARS; ++i) {
    resetStar(stars[i]);
  }
}

static void runStarfield() {
  tft.fillScreen(TFT_BLACK);
  tft.startWrite();
  for (int i = 0; i < NUM_STARS; ++i) {
    Star &s = stars[i];
    s.z -= STAR_SPEED;
    if (s.z <= 1) {
      resetStar(s);
      continue;
    }
    int sx = CENTER + (int)(s.x / s.z * CENTER);
    int sy = CENTER + (int)(s.y / s.z * CENTER);
    if (sx >= 0 && sx < 240 && sy >= 0 && sy < 240) {
      int dx = sx - CENTER;
      int dy = sy - CENTER;
      if (dx * dx + dy * dy <= RADIUS * RADIUS) {
        tft.drawPixel(sx, sy, TFT_WHITE);
      }
    }
  }
  tft.endWrite();
}

static void drawHand(float angleDeg, int length, uint16_t color) {
  float rad = angleDeg * DEG_TO_RAD;
  int x = CENTER + (int)(sinf(rad) * length);
  int y = CENTER - (int)(cosf(rad) * length);
  canvas.drawLine(CENTER, CENTER, x, y, color);
}

static void runClock() {
  canvas.fillScreen(TFT_BLACK);
  canvas.drawCircle(CENTER, CENTER, RADIUS - 1, TFT_WHITE);
  for (int i = 0; i < 12; ++i) {
    float a = i * 30 * DEG_TO_RAD;
    int x1 = CENTER + (int)((RADIUS - 10) * sinf(a));
    int y1 = CENTER - (int)((RADIUS - 10) * cosf(a));
    int x2 = CENTER + (int)((RADIUS - 2) * sinf(a));
    int y2 = CENTER - (int)((RADIUS - 2) * cosf(a));
    canvas.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  uint32_t seconds = millis() / 1000;
  int s = seconds % 60;
  int m = (seconds / 60) % 60;
  int h = (seconds / 3600) % 12;

  drawHand(h * 30 + m * 0.5f, RADIUS - 50, TFT_WHITE);
  drawHand(m * 6 + s * 0.1f, RADIUS - 30, TFT_WHITE);
  drawHand(s * 6, RADIUS - 20, TFT_RED);
  canvas.fillCircle(CENTER, CENTER, 3, TFT_WHITE);
  canvas.pushSprite(0, 0);
}

static void runArcClock() {
  canvas.fillScreen(TFT_BLACK);

  uint32_t seconds = millis() / 1000;
  int s = seconds % 60;
  int m = (seconds / 60) % 60;
  int h = (seconds / 3600) % 12;

  float secAngle = s * 6 - 90;
  float minAngle = m * 6 + s * 0.1f - 90;
  float hourAngle = h * 30 + m * 0.5f - 90;

  int thickness = 10;
  int r = RADIUS - 10;

  canvas.fillArc(CENTER, CENTER, r, r - thickness, -90, hourAngle, TFT_BLUE);
  canvas.fillArc(CENTER, CENTER, r - thickness - 5, r - 2 * thickness - 5, -90, minAngle, TFT_GREEN);
  canvas.fillArc(CENTER, CENTER, r - 2 * thickness - 10, r - 3 * thickness - 10, -90, secAngle, TFT_RED);

  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", h == 0 ? 12 : h, m, s);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString(buf, CENTER, CENTER);
  canvas.pushSprite(0, 0);
}

static void runArcAnalogClock() {
  canvas.fillScreen(TFT_BLACK);

  uint32_t seconds = millis() / 1000;
  int s = seconds % 60;
  int m = (seconds / 60) % 60;
  int h = (seconds / 3600) % 12;

  float secAngle = s * 6 - 90;
  float minAngle = m * 6 + s * 0.1f - 90;
  float hourAngle = h * 30 + m * 0.5f - 90;

  int thickness = 10;
  int r = RADIUS - 10;

  canvas.fillArc(CENTER, CENTER, r, r - thickness, -90, hourAngle, TFT_BLUE);
  canvas.fillArc(CENTER, CENTER, r - thickness - 5, r - 2 * thickness - 5, -90, minAngle, TFT_GREEN);
  canvas.fillArc(CENTER, CENTER, r - 2 * thickness - 10, r - 3 * thickness - 10, -90, secAngle, TFT_RED);

  canvas.drawCircle(CENTER, CENTER, ANALOG_RADIUS, TFT_WHITE);
  for (int i = 0; i < 12; ++i) {
    float a = i * 30 * DEG_TO_RAD;
    int x1 = CENTER + (int)((ANALOG_RADIUS - 5) * sinf(a));
    int y1 = CENTER - (int)((ANALOG_RADIUS - 5) * cosf(a));
    int x2 = CENTER + (int)((ANALOG_RADIUS - 1) * sinf(a));
    int y2 = CENTER - (int)((ANALOG_RADIUS - 1) * cosf(a));
    canvas.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  drawHand(h * 30 + m * 0.5f, ANALOG_RADIUS - 25, TFT_WHITE);
  drawHand(m * 6 + s * 0.1f, ANALOG_RADIUS - 15, TFT_WHITE);
  drawHand(s * 6, ANALOG_RADIUS - 10, TFT_RED);
  canvas.fillCircle(CENTER, CENTER, 3, TFT_WHITE);
  canvas.pushSprite(0, 0);
}

static void runPlasma() {
  static float t = 0.0f;
  t += 0.05f;
  canvas.startWrite();
  for (int y = 0; y < 240; ++y) {
    for (int x = 0; x < 240; ++x) {
      int dx = x - CENTER;
      int dy = y - CENTER;
      if (dx * dx + dy * dy <= RADIUS * RADIUS) {
        float v = sinf(dx * 0.05f + t) + sinf(dy * 0.05f + t) + sinf((dx + dy) * 0.05f + t);
        uint8_t r = sinf(v + t) * 127 + 128;
        uint8_t g = sinf(v + t + 2.1f) * 127 + 128;
        uint8_t b = sinf(v + t + 4.2f) * 127 + 128;
        canvas.drawPixel(x, y, color565(r, g, b));
      } else {
        canvas.drawPixel(x, y, TFT_BLACK);
      }
    }
  }
  canvas.endWrite();
  canvas.pushSprite(0, 0);
}

static void runAHT10Demo() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  char buf[32];
  snprintf(buf, sizeof(buf), "Temp: %.1f C", temp.temperature);
  canvas.drawString(buf, CENTER, CENTER - 10);
  snprintf(buf, sizeof(buf), "Humidity: %.1f%%", humidity.relative_humidity);
  canvas.drawString(buf, CENTER, CENTER + 10);
  canvas.pushSprite(0, 0);
}

static void runClockTemp() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("Time error", CENTER, CENTER);
    canvas.pushSprite(0, 0);
    return;
  }

  int s = timeinfo.tm_sec;
  int m = timeinfo.tm_min;
  int h24 = timeinfo.tm_hour;
  int h = h24 % 12;

  canvas.fillScreen(TFT_BLACK);

  float secAngle = s * 6 - 90;
  float minAngle = m * 6 + s * 0.1f - 90;
  float hourAngle = h * 30 + m * 0.5f - 90;

  int thickness = 10;
  int r = RADIUS - 10;

  canvas.fillArc(CENTER, CENTER, r, r - thickness, -90, hourAngle, TFT_BLUE);
  canvas.fillArc(CENTER, CENTER, r - thickness - 5, r - 2 * thickness - 5, -90,
                 minAngle, TFT_GREEN);
  canvas.fillArc(CENTER, CENTER, r - 2 * thickness - 10, r - 3 * thickness - 10,
                 -90, secAngle, TFT_RED);

  canvas.drawCircle(CENTER, CENTER, ANALOG_RADIUS, TFT_WHITE);
  for (int i = 0; i < 12; ++i) {
    float a = i * 30 * DEG_TO_RAD;
    int x1 = CENTER + (int)((ANALOG_RADIUS - 5) * sinf(a));
    int y1 = CENTER - (int)((ANALOG_RADIUS - 5) * cosf(a));
    int x2 = CENTER + (int)((ANALOG_RADIUS - 1) * sinf(a));
    int y2 = CENTER - (int)((ANALOG_RADIUS - 1) * cosf(a));
    canvas.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  drawHand(h * 30 + m * 0.5f, ANALOG_RADIUS - 25, TFT_WHITE);
  drawHand(m * 6 + s * 0.1f, ANALOG_RADIUS - 15, TFT_WHITE);
  drawHand(s * 6, ANALOG_RADIUS - 10, TFT_RED);
  canvas.fillCircle(CENTER, CENTER, 3, TFT_WHITE);

  char buf[32];
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  sprintf(buf, "%02d:%02d", h24, m);
  canvas.drawString(buf, CENTER, CENTER - ANALOG_RADIUS - 20);

  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.setTextSize(2);
  snprintf(buf, sizeof(buf), "%.1fC", temp.temperature);
  canvas.drawString(buf, CENTER, CENTER + ANALOG_RADIUS + 10);
  snprintf(buf, sizeof(buf), "%.0f%%", humidity.relative_humidity);
  canvas.drawString(buf, CENTER, CENTER + ANALOG_RADIUS + 40);
  canvas.setTextSize(1);

  canvas.pushSprite(0, 0);
}

static const float TEMP_MIN_C = -5.0f;
static const float TEMP_MAX_C = 50.0f;

static uint16_t temperatureColor(float temp) {
  float ratio = (temp - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C);
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  struct RGB { uint8_t r, g, b; };
  const RGB stops[] = {{0,0,255}, {0,255,0}, {255,165,0}, {255,0,0}};
  const float pos[] = {0.0f, 0.5f, 0.8f, 1.0f};
  int idx = 0;
  while (idx < 3 && ratio > pos[idx+1]) idx++;
  float local = (ratio - pos[idx]) / (pos[idx+1] - pos[idx]);
  uint8_t r = stops[idx].r + (stops[idx+1].r - stops[idx].r) * local;
  uint8_t g = stops[idx].g + (stops[idx+1].g - stops[idx].g) * local;
  uint8_t b = stops[idx].b + (stops[idx+1].b - stops[idx].b) * local;
  return color565(r, g, b);
}

// 7-segment style digit rendering ------------------------------

static int sevenSegDigitWidth(int scale) {
  int t = scale * 2;
  int w = scale * 6;
  return w + 2 * t;
}

static int sevenSegDigitHeight(int scale) {
  int t = scale * 2;
  int h = scale * 8;
  return 2 * h + 3 * t;
}

static const uint8_t DIGIT_SEGMENTS[10] = {
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F  // 9
};

static const bool DRAW_OFF_SEGMENTS = false;  // set to true to dim inactive segments
static const uint16_t OFF_SEGMENT_COLOR = color565(30, 40, 30);

static int drawSevenSegDigit(int x, int y, int digit, int scale, uint16_t color) {
  int t = scale * 2;
  int h = scale * 8;
  int w = scale * 6;
  uint8_t seg = DIGIT_SEGMENTS[digit];
  if (seg & 0x01) {
    canvas.fillRect(x + t, y, w, t, color);                 // A
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x + t, y, w, t, OFF_SEGMENT_COLOR);
  }
  if (seg & 0x02) {
    canvas.fillRect(x + w + t, y + t, t, h, color);         // B
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x + w + t, y + t, t, h, OFF_SEGMENT_COLOR);
  }
  if (seg & 0x04) {
    canvas.fillRect(x + w + t, y + t + h + t, t, h, color); // C
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x + w + t, y + t + h + t, t, h, OFF_SEGMENT_COLOR);
  }
  if (seg & 0x08) {
    canvas.fillRect(x + t, y + 2 * h + 2 * t, w, t, color); // D
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x + t, y + 2 * h + 2 * t, w, t, OFF_SEGMENT_COLOR);
  }
  if (seg & 0x10) {
    canvas.fillRect(x, y + t + h + t, t, h, color);         // E
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x, y + t + h + t, t, h, OFF_SEGMENT_COLOR);
  }
  if (seg & 0x20) {
    canvas.fillRect(x, y + t, t, h, color);                 // F
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x, y + t, t, h, OFF_SEGMENT_COLOR);
  }
  if (seg & 0x40) {
    canvas.fillRect(x + t, y + h + t, w, t, color);         // G
  } else if (DRAW_OFF_SEGMENTS) {
    canvas.fillRect(x + t, y + h + t, w, t, OFF_SEGMENT_COLOR);
  }
  return sevenSegDigitWidth(scale);
}

static int drawColonSeg(int x, int y, int scale, uint16_t color) {
  int t = scale * 2;
  int h = scale * 8;
  canvas.fillCircle(x + t / 2, y + h / 2 + t, t / 2 + 1, color);
  canvas.fillCircle(x + t / 2, y + h + t + h / 2 + t, t / 2 + 1, color);
  return t;
}

static int measureSevenSegString(const char *s, int scale) {
  int width = 0;
  int digitW = sevenSegDigitWidth(scale);
  int colonW = scale * 2;
  while (*s) {
    width += (*s == ':') ? colonW : digitW;
    if (*(s + 1)) width += scale;
    ++s;
  }
  return width;
}

static void drawSevenSegString(int x, int y, const char *s, int scale,
                               uint16_t color) {
  int digitW = sevenSegDigitWidth(scale);
  int colonW = scale * 2;
  while (*s) {
    if (*s == ':') {
      x += drawColonSeg(x, y, scale, color) + scale;
    } else {
      x += drawSevenSegDigit(x, y, *s - '0', scale, color) + scale;
    }
    ++s;
  }
}

static void runTempArcClock() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_RED, TFT_BLACK);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("Time error", CENTER, CENTER);
    canvas.pushSprite(0, 0);
    return;
  }

  int s = timeinfo.tm_sec;
  int m = timeinfo.tm_min;
  int h = timeinfo.tm_hour % 12;

  canvas.fillScreen(TFT_BLACK);

  float ratio = (temp.temperature - TEMP_MIN_C) / (TEMP_MAX_C - TEMP_MIN_C);
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  float arcAngle = ratio * 360.0f - 90.0f;
  uint16_t tColor = temperatureColor(temp.temperature);
  int thickness = 8;
  canvas.fillArc(CENTER, CENTER, RADIUS - 1, RADIUS - 1 - thickness,
                 -90, arcAngle, tColor);

  int r = RADIUS - thickness - 4;

  char timebuf[6];
  sprintf(timebuf, "%02d:%02d", timeinfo.tm_hour, m);
  int scale = 3;
  int textWidth = measureSevenSegString(timebuf, scale);
  int timeY = (CENTER - r + r / 2);
  int timeX = CENTER - textWidth / 2;
  drawSevenSegString(timeX, timeY, timebuf, scale, TFT_BLUE);

  canvas.drawCircle(CENTER, CENTER, r, TFT_WHITE);
  for (int i = 0; i < 12; ++i) {
    float a = i * 30 * DEG_TO_RAD;
    int x1 = CENTER + (int)((r - 10) * sinf(a));
    int y1 = CENTER - (int)((r - 10) * cosf(a));
    int x2 = CENTER + (int)((r - 2) * sinf(a));
    int y2 = CENTER - (int)((r - 2) * cosf(a));
    canvas.drawLine(x1, y1, x2, y2, TFT_WHITE);
  }

  drawHand(h * 30 + m * 0.5f, r - 40, TFT_WHITE);
  drawHand(m * 6 + s * 0.1f, r - 20, TFT_WHITE);
  drawHand(s * 6, r - 10, TFT_RED);
  canvas.fillCircle(CENTER, CENTER, 3, TFT_WHITE);

  
  char buf[16];
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(tColor, TFT_BLACK);
  canvas.setTextSize(3);
  snprintf(buf, sizeof(buf), "%.1fC", temp.temperature);
  int y = (CENTER + r - r / 2) - 10;
  canvas.drawString(buf, CENTER, y);
  canvas.setTextSize(1);

  canvas.pushSprite(0, 0);
}


static void drawClippingTest() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Clipping test", CENTER, CENTER);

  // draw diagonals
  tft.drawLine(0, 0, 239, 239, TFT_RED);
  tft.drawLine(0, 239, 239, 0, TFT_RED);

  // draw nested rectangles to visualize corners
  for (int r = 0; r < CENTER; r += 20) {
    tft.drawRect(r, r, 240 - 2 * r, 240 - 2 * r, TFT_BLUE);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup...");

  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());


  tft.begin();
  Serial.println("Display initialized");
  tft.setRotation(0);
  Serial.println("Rotation set");
  tft.setBrightness(200);
  Serial.println("Brightness set (if supported)");
  Wire.begin(AHT_SDA, AHT_SCL);
  if (!aht.begin(&Wire)) {
    Serial.println("AHT10 not found");
  } else {
    Serial.println("AHT10 initialized");
  }
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println(" connected");
  configTime(GMT_OFFSET_SEC, 0, "pool.ntp.org");
  canvas.setColorDepth(16);
  canvas.setPsram(true);
  if (!canvas.createSprite(240, 240)) {
    Serial.println("Failed to allocate sprite buffer");
  }
  canvas.setSwapBytes(true);
  canvas.fillScreen(TFT_BLACK);

  drawClippingTest();
  Serial.println("Clipping test displayed");
  tft.clearClipRect();
  delay(3000);  // view clipping test for 3 seconds
  if (DEMO_MODE == DEMO_STARFIELD) {
    initStars();
    Serial.println("Stars initialized");
  }
}

void loop() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    Serial.println("Loop running");
    lastPrint = millis();
  }
  switch (DEMO_MODE) {
    case DEMO_STARFIELD:
      runStarfield();
      break;
    case DEMO_CLOCK:
      runClock();
      break;
    case DEMO_PLASMA:
      runPlasma();
      break;
    case DEMO_ARC_CLOCK:
      runArcClock();
      break;
    case DEMO_ARC_ANALOG_CLOCK:
      runArcAnalogClock();
      break;
    case DEMO_AHT10:
      runAHT10Demo();
      break;
    case DEMO_CLOCK_TEMP:
      runClockTemp();
      break;
    case DEMO_TEMP_ARC_CLOCK:
      runTempArcClock();
      break;
  }
}
