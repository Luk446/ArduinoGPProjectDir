---
layout: default
title: Node-RED Architecture
permalink: /architecture/node/
---

{% include nav.html %}

# Node-RED Architecture
Hello

Node-red does most of the processing in terms of determining the temperature status and reporting back to the gateway. 
Below is the complete flow of the Node-RED architecture. 
[[Full Node-RED flow]](images/FlowNodeREd.png)

The architecture can be split into 5 different category.
1. Initializing
2. Publishing
3. Plotting
4. Manipulating
5. Finalizing



[Initializing values]
First, before manipulating the information, Node-RED saves the previous values sent by the gateway. This is because the nodes are either sleeping to reduce energy consumption or there is no temperature change. Saving the values allows the dashboard to not have missing values.
Below is the First part, Initializing the values. 
(images/InitializingValuesNodeRed.png)
1. Node-RED recieves the values from the Gateway using a MQTT Broker.
2. Seperate each value from the 
