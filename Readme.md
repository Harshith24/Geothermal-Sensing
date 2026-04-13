# Geothermal Sensor - ESP32 firmware code
Thos folder contains the actual workspace for the esp32. this is where im getting the sensor data, connecting the esp32 to wifi, and sending it over to the broker.

# Server - influx DB 

## 2 sensors values data expoler:
<img width="1182" height="247" alt="image" src="https://github.com/user-attachments/assets/07809bc9-228f-454a-bd42-72cbcf0e2b13" />


## Dashboard showing 2 sensors values:
<img width="1826" height="486" alt="image" src="https://github.com/user-attachments/assets/a0e57293-f29f-4809-a1fd-db3f91bf1db2" />


# tailscale
TO access the influxDB dashboard outside of the local network, we use `tailscale VPN` to access it from outside. On the free tier it supports 100 devices and 3 users. essentially enter the tailnet IP followed by the port you want to check. TO access the influx DB from outside the network, enter `100.64.205.82:8086` since the influx DB runs on port 8086.

## setup
the server that is running the containers should have tailscale installed first. The owner should authenticate with their email on that device. They can add other devices like their phone by installing the tailscale app and authenticating with the same email. Other users (external) should also install tailscale app and authenticate and the owner should add their email to their server network. 

# workflow
The system follows a decoupled, event-driven architecture to ensure data is captured reliably and stored for long-term analysis.

1. Hardware
DS18B20 digital thermometers use the 1-Wire protocol to send temperature readings to the ESP32.
The ESP32 (Firmware) reads the raw digital signal, converts it to Celsius, and formats it into a string.

2. Data Transmission (MQTT)
Publishing: The ESP32 acts as an MQTT Client. It publishes the data over Wi-Fi to a specific topic (e.g., sensors/temp/0) on the Mosquitto Broker. Essentially the broker is a message queue that holds the data before it gets pushed to DB. This prevents a massive flood of data from sensors from crashing the DB. 

3. Data Routing (Telegraf)
The Bridge: Telegraf is the "translator" sitting in a Docker container. It is subscribed to the sensors/temp/+ wildcard topic.

Processing: Every time a message hits Mosquitto, Telegraf grabs it, attaches metadata (like the topic name and host ID), and converts it into Line Protocol (the language InfluxDB speaks).

4. Data Storage (InfluxDB)
Ingestion: Telegraf performs an HTTP POST to the InfluxDB API. This requires setting up a http all access token and providing that to telegraf. 

Persistence: InfluxDB stores the data in a Time-Series Bucket. Unlike a traditional database (like postgres), it is optimized specifically for timestamps and high-speed writes.

5. Visualization (UI)
Querying: The InfluxDB Dashboard (or Grafana) runs Flux queries to pull the data from the bucket and render it into real-time line charts or gauges.
LEt's just ignore grafana for now, we got a dashboard working on influxDB.

# debugging
verifying the broker is receiving data from the esp32 directly from looking at the container logs: `docker exec -it mosquitto mosquitto_sub -h localhost -t "sensors/#" -v`
<img width="145" height="81" alt="image" src="https://github.com/user-attachments/assets/ba2d1f70-a3e9-44ed-aeb1-ea2fa520740b" />

verifying if telegraf (mqtt consumer) is able to connect to mosquitto (running on 1883) and loading the influxdb output: `docker logs telegraf`
<img width="953" height="187" alt="image" src="https://github.com/user-attachments/assets/4be136e9-435b-45e2-a7a4-002ad8f54440" />


# next steps
1. Figure out placement of the sensors
2. Set Up alerts so we know of any sudden temperature spikes. Alerts can be done through influxdb directly or through pagerduty? will have to explore
3. Add more sensors to it (currently has 2 but can add more easily)
4. add option for both Celcius and Fahrenheit
5. add more esp32 - will need to update `client.connect();` and update `sensors/room1/temp/0` and `sensors/room3/temp/0` in the esp32 `main.cpp` file and update `sensors/+/temp/+` telegraf config file. 
