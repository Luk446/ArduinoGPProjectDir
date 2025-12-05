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
- Libraries referenced: see `libraries/` for Adafruit, ArduinoJson, PubSubClient, DHT sensor, etc.

## Design and Architecture
- Hardware: ESP32 Nodes with sensors; ESP32 Gateway connected to host running Node‑RED.
- Network: ESP‑NOW from Nodes to Gateway; optional Wi‑Fi/MQTT beyond Gateway.
- Software: Node publishes alerts/telemetry; Gateway receives and forwards; Node‑RED manages dashboard and alerting.
- More details: [Architecture](architecture), [Arduino Node](arduino_arch), [Gateway/Node‑RED](node_arch)

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

Some text 

## Future Improvements

Some text
