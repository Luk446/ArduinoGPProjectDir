---
layout: default
title: Arduino Architecture
permalink: /architecture/arduino/
---

{% include nav.html %}

# Arduino Architecture

The firmware is split into two cooperating sketches:
- Sensor Node: `Arduino/ESP-Node/ESP-Node.ino`
- Gateway: `Arduino/ESP-Gateway/ESP-Gateway.ino`

The flow-level view is captured in the diagrams below, while the rest of this page documents the concrete behavior that appears in the code.

![Node Flow](images/node_flow_new.png)

[Gateway Loop Flow](images/gateway_flow.svg)

---

## Sensor Node Firmware (`ESP-Node.ino`)

### Hardware and libraries
- ESP32 running in STA mode with ESP-NOW enabled on WiFi channel 10.
- DHT11 sensor on pin `DHT_PIN = 4` via the `DHT` library.
- Single NeoPixel on `RGB_PIN = 5` driven by `Adafruit_NeoPixel`.

### Power and timing strategy
- Deep sleep for 10 seconds (`DEEP_SLEEP_DURATION = 10,000,000` microseconds) between wake cycles.
- On wake: initialize peripherals, restore the last LED color from RTC memory, and re-register the gateway peer on channel 10.
- After transmitting data, the node keeps ESP-NOW active for a 10-second listening window (`LISTEN_WINDOW_MS = 10000`) so it can receive LED updates before returning to deep sleep.

### Sensor publishing flow
1. Read temperature and humidity from the DHT11.
2. Skip transmission if the temperature has not changed since the previous send (`lastSentTemp`).
3. Populate the packed struct:
	- `int nodeID`
	- `float temp`
	- `float hum`
4. Send via `esp_now_send` to the hard-coded gateway MAC `0xF0F5BDFB26B4`.
5. Update `lastSentTemp` only on successful send.

### LED handling
- LED commands reuse a packed struct (`turnOn`, `r`, `g`, `b`).
- The node prints the sender MAC, validates the payload size, and immediately updates the NeoPixel color.
- Latest LED color is persisted in RTC memory (`lastLedR/G/B`) and restored on the next wake so the LED state survives deep sleep.

---

## Gateway Firmware (`ESP-Gateway.ino`)

### Connectivity and peers
- Connects to WiFi SSID `B31IOT-MQTT` (station mode). ESP-NOW is initialized afterward so it shares the WiFi channel.
- Registers up to four ESP-NOW peers (MACs listed in the sketch) corresponding to each sensor node.
- MQTT broker: `broker.hivemq.com` using the `PubSubClient` library.

### MQTT topics
- Publish telemetry: `EnvPublish4482`
- Threshold updates (subscribe): `ThreshCheck4482`
- LED override control (subscribe): `LEDOverride4482`
- Testing/diagnostics publish: `TestTopic4482`

### Data handling pipeline
1. `OnDataRecv` copies the packed `sensor_data` struct and records temperature, humidity, last update timestamp, and active state per node.
2. Every 5 seconds (`publishInterval = 5000`), `publishToNodeRED()` emits a JSON payload of the form `{ "d": { node1_temp, node1_hum, ... }, "alarm": bool, "threshold": value }` on `EnvPublish4482` when MQTT is connected.
3. Threshold updates arrive as JSON on `ThreshCheck4482` and directly adjust `TEMP_THRESHOLD`.
4. `checkTemperatureThreshold()` sets `alarmActive` if any active node exceeds the current threshold within the last 30 seconds.

### LED strategy
- Remote override: JSON payload on `LEDOverride4482` toggles `remoteOverrideActive` and supplies RGB values that are broadcast immediately.
- Default behavior: even without an override, the gateway stores the last LED command (`overrideOn/R/G/B`) and pushes it to the responding node whenever data is received.
- Periodic broadcast: every 3 seconds (`ledBroadcastInterval = 3000`) the gateway calls `broadcastLEDCommand()` so sleeping nodes that wake between transmissions still receive the latest LED state.

### Reliability aspects
- MQTT connection watchdog attempts to reconnect every 5 seconds when disconnected.
- ESP-NOW callbacks log send failures to help troubleshoot peer registrations.
- WiFi channel alignment is logged so mismatches between node channel 10 and the access point channel can be addressed quickly.

---

## Data and LED Flow Summary

1. Nodes wake, sample, and send only when the temperature changes, minimizing airtime and energy use.
2. Gateway ingests ESP-NOW frames, mirrors LED status back to the sender, and updates MQTT clients with the aggregated view.
3. Node-RED consumes `EnvPublish4482`, displays dashboards, and can publish threshold or LED override commands that immediately influence gateway logic.
4. LED commands remain synchronized due to the periodic gateway broadcast and the node's 10-second listening window after each send.

These behaviors correspond to the flowcharts above and the detailed Node-RED accompaniment documented in `docs/node_arch.md`.

