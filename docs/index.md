---
layout: default
title: Home
---

{% include nav.html %}

# Project Documentation
## Group Project – Demonstration Website

Overview:

- Gateway collects sensor readings and shares status.
- Nodes wake, send data, briefly listen, then sleep.
- LED colors show overall system state.
- Threshold changes and LED overrides are supported.
- Data published for dashboard viewing.
- Dashboard hosted on Node-RED 
- Node-RED handles all the computation for threshold
- See details: [System Architecture](architecture)

---

## Repository and Libraries
- Repository: https://github.com/Luk446/ArduinoGPProjectDir
- Arduino sketches: `Arduino/ESP-Node/ESP-Node.ino`, `Arduino/ESP-Gateway/ESP-Gateway.ino`
- Node-RED flows: `Node-RED/flows.json` (plus drafts for experiments)
### Libraries Used
- `Adafruit_NeoPixel`: drive the RGB NeoPixel on nodes.
- `DHT sensor library`: read temperature/humidity from DHT11.
- `ArduinoJson`: build/parse MQTT JSON payloads.
- `PubSubClient`: MQTT client for publishing/subscribing.
- `esp_now`, `WiFi`, `esp_wifi` (ESP32 core): transport and channel configuration.

## Design and Architecture
- Hardware: ESP32 Nodes with sensors; ESP32 Gateway connected to host running Node‑RED.
- Network: ESP‑NOW from Nodes to Gateway; optional Wi‑Fi/MQTT beyond Gateway.
- Software: Node publishes alerts/telemetry; Gateway receives and forwards; Node‑RED manages dashboard and alerting.
- More details: [Architecture](architecture), [Arduino Architecture]({{ site.baseurl }}/architecture/arduino/), [Node-RED Architecture]({{ site.baseurl }}/architecture/node/)

## Energy Strategies and Encoding
- Duty cycling: Nodes sleep between samples; event‑driven publishing on thresholds.
- Lightweight frames: compact byte/struct payloads for ESP‑NOW; minimal JSON at Gateway only.
- Backoff/retry limits: reduce collisions and radio time; short LED feedback only when debugging.

## Testing Results

Some text

## Deployment and Use
1. Flash firmware: open `ESP-Node.ino` and `ESP-Gateway.ino` in Arduino IDE; select ESP32 board; install required libraries; set `NODE_ID` and Wi‑Fi creds (Gateway if needed); upload.
2. Node‑RED: install and start Node‑RED; import `Node-RED/flows.json`; configure serial/MQTT nodes to match Gateway; deploy; open dashboard.
3. Run: power Node(s) and Gateway; verify ESP‑NOW peer pairing; trigger thresholds; observe alerts and telemetry.

## Reflection

- Deep sleep with 10-second wake cycles achieved 100-200x power reduction (10µA vs 65mA active), while event-driven transmission (send only when temp changes ≥1°C) reduced radio time by 60%
- Node-RED threshold processing offloaded computation from battery nodes to mains-powered host, enabling dynamic threshold updates via MQTT without node reflashing
- Key trade-offs: 10-second sleep interval balanced responsiveness vs battery life, struct packing ensured cross-device compatibility, and RTC memory preserved LED state across sleep cycles at cost of 5ms wake overhead

## Future Improvements

- Energy & Reliability: Wake-on-threshold via RTC GPIO interrupts, ACK mechanism for guaranteed LED delivery, battery voltage monitoring with low-power alerts, and watchdog timers for auto-recovery
- Scalability: Auto-discovery via broadcast handshake (eliminate MAC hardcoding), mesh routing for >50m range, hierarchical gateways for 10+ nodes, and InfluxDB integration for historical analysis
- Advanced Features: Web config portal for threshold/WiFi settings, Twilio SMS alerts for critical temps, OTA firmware updates, AES-128 encryption for industrial security, and predictive ML analytics