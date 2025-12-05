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

The architecture can be split into 4 different sections.
1. Initializing
2. Publishing / Manipulating
3. Plotting / Finalizing
4. Thresholds



**Initializing**

First, before manipulating the information, Node-RED saves the previous values sent by the gateway. This is because the nodes are either sleeping to reduce energy consumption or there is no temperature change. Saving the values allows the dashboard to not have missing values.
Below is the First part, Initializing the values. 
[[Initializing Values Flow]](images/InitializingValuesNodeRed.png)

1. Node-RED recieves the values from the Gateway using a MQTT Broker. (_EnvPublish4482_)
2. Seperate each value from the recieved json file (_Set Temp Payload Temp #_)
4. Save last values
     Since the gateway sends values every second it sometimes sends values that are empty, function (_save temp #_) checks if its empty. If not it saves the value. The value is saved as a flow instead of msg. 
5. Inject Current Value every second (_flow.MyNumber#_)

It does this process for each of the nodes and saves the temperatures when it recieves them. 

**Publishing / Manipulating**

The values are published onto the dashboard using a text node (_Current Temp ESP-#_), while temperature values are checked using function nodes (_Get Message for Temperature_). [[Publishing/Manipulating Flow]](images/Pub_Man_Flow.png)

The manipulating is done using a function. The function checks for current saved temperature values and theshold values (see Threshold section). From the [[Code]](images/messageCode.png) Both values are initialized and checked between eachother to determine if values are either too cold, good or too hot to then output a string message stating the current state. 

**Plotting / Finalizing**
The chart node (_Temp vs Time (Last 10 Readings)_) recieves the continuous values from the inject node (_flow.MyNumber#_) to plot it over time.

***the chart used only 10 readings for visuals, can be changed to a longer time interval in real life situations***


