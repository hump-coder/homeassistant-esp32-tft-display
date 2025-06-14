# homeassistant-esp32-tft-display

This project shows how to display values from Home Assistant on a round 240x240 TFT display using an ESP32 based LOLIN S2 Mini board.

Values are received over MQTT and rendered using a set of simple widgets:

* **Outer arcs** – up to three progress arcs around the edge of the display.
* **Dial/meter** – a central needle style gauge.
* **Temperature** – reading from the onboard AHT10 sensor.

MQTT topics used:

```
ha_display/arc1
ha_display/arc2
ha_display/arc3
ha_display/dial
ha_display/temperature (published by the device)
```

Configure WiFi and MQTT parameters in `include/config.h` or create `config_private.h` with overriding values.
