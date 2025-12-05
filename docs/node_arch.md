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
[[Full Node-RED flow]](images/FinalNodeREd.png)

The architecture can be split into 3 different category.
 - Plotting
 - Publishing
 - Subscribing
[Initializing values]
First, before manipulating the information, Node-RED saves the previous values sent by the gateway. This is because the nodes are either sleeping to reduce energy consumption or there is no temperature change. Saving the values allows the dashboard to not have missing values.

