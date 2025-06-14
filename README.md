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

Payloads for `arc1`, `arc2`, `arc3` and `dial` can be either a simple numeric
value or a JSON object that allows additional configuration. Example:

```json
{ "value": 75, "min": 0, "max": 150, "width": 10, "color": "#ff0000" }
```

Supported fields:

* `value` – current measurement
* `min`/`max` – optional range used to calculate the percentage
* `width` – arc thickness (for `arc1`..`arc3` only)
* `color` – hex color (e.g. `"#00ff00"`)

Configure WiFi and MQTT parameters in `include/config.h` or create `config_private.h` with overriding values.
If your MQTT broker requires authentication, define `CONFIG_MQTT_USER` and `CONFIG_MQTT_PASS` in `config_private.h`.
