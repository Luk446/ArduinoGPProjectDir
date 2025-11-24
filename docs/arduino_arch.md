---
layout: default
title: Arduino Architecture
permalink: /architecture/arduino/
---

{% include nav.html %}

# Arduino Architecture

Arduino architecture is split into two parts: node and gateway.

The node activicty can be described by the flowchart below.

![Node Flow](images/node_flow_new.png)

## Gateway Loop

The gateway control loop coordinates WiFi, MQTT connectivity, ESP-NOW data reception, periodic publishing, and LED override broadcasting.

![Gateway Loop Flow](images/gateway_flow.svg)

