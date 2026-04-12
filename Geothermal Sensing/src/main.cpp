#include <Arduino.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <time.h>
#include "SPIFFS.h"
#include "settings.h"

//max SPIFFS cache size (bytes) before oldest data is discarded
#define MAX_CACHE_SIZE 65536   //64 KB, probably can go higher

const int oneWirePin = 4;
OneWire oneWire(oneWirePin);
DallasTemperature sensors(&oneWire);

WiFiClient espClient;
PubSubClient client(espClient);

//sensor positions on board from left to right
const char* topics[] = { "hotIn", "loopOut", "hotOut", "loopIn" };
unsigned long lastReadTime  = 0;
const unsigned long READ_INTERVAL = 15000; //15 seconds

//returns Unix epoch in seconds; 0 if time not yet synced.
time_t getEpoch() {
  time_t now;
  struct tm ti;
  if (!getLocalTime(&ti)) return 0;
  time(&now);
  return now;
}

void storeLocally(const String& line) {
  //guard against unbounded growth
  if (SPIFFS.totalBytes() > 0) {
    File f = SPIFFS.open("/data.txt", FILE_APPEND);
    if (!f) { Serial.println("SPIFFS open failed"); return; }
    //simple size guard, if cache is too large, warn and skip
    if (f.size() >= MAX_CACHE_SIZE) {
      f.close();
      Serial.println("SPIFFS cache full — dropping sample");
      return;
    }
    f.println(line);
    f.close();
    Serial.println("Stored locally: " + line);
  }
}

//replays every cached line over MQTT, then clears the file.
void replayStoredData() {
  File file = SPIFFS.open("/data.txt", FILE_READ);
  if (!file || file.size() == 0) {
    if (file) file.close();
    return;
  }
  
  delay(READ_INTERVAL); //delay to ensure connection is fully established

  Serial.println("Replaying stored data...");
  bool allSent = true;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    //line protocol topic is the measurement name
    //publish to a dedicated replay topic so Telegraf can ingest it identically.
    if (!client.publish("geothermal/data", line.c_str())) {
      Serial.println("Publish failed during replay");
      allSent = false;
      break;
    }
    Serial.println("Replayed: " + line);
    delay(500); //small wait time to avoid flooding
  }

  file.close();
  if (allSent) {
    SPIFFS.remove("/data.txt");
    Serial.println("Replay complete — cache cleared.");
  }
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setKeepAlive(60);

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected — IP: " + WiFi.localIP().toString());

  //sync time via NTP (blocking until synced, max ~5 s)
  configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
  Serial.print("Waiting for NTP sync");
  time_t epoch = 0;
  unsigned long start = millis();
  while (epoch < 1000000 && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
    epoch = getEpoch();
  }
  Serial.println(epoch > 0 ? " synced." : " failed (will retry in loop).");
}

void loop() {
  //wifiCheck();
  //verify the wifi connection
  if (WiFi.status() != WL_CONNECTED) {//return;
    Serial.print("Reconnecting WiFi");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println(WiFi.status() == WL_CONNECTED ? " connected." : " failed.");
  }
  //verify the mqtt connection
  //mqttCheck();
  if (!client.connected()) {//return;
    Serial.print("Attempting MQTT connection... ");
    if (client.connect("ESP32_Temp_Sensor")) {
      Serial.println("connected.");
      replayStoredData();
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
  }

  client.loop();

  unsigned long now = millis();
  if (now - lastReadTime < READ_INTERVAL) return;
  lastReadTime = now;

  time_t epoch = getEpoch();
  if (epoch == 0) {
    //re-attempt NTP if we still don't have time
    configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER);
    Serial.println("No NTP time yet, skipping read.");
    return;
  }

  sensors.requestTemperatures();
  int deviceCount = sensors.getDeviceCount();
  Serial.printf("Sensors detected: %d\n", deviceCount);

  for (int i = 0; i < deviceCount && i < 4; i++) {
    float tempF = sensors.getTempFByIndex(i);

    if (tempF == DEVICE_DISCONNECTED_F) {
      Serial.printf("Sensor %s: disconnected!\n", topics[i]);
      continue;
    }
    if (tempF <-20.00) {
      continue;
    }

    //formats one InfluxDB line-protocol record.
    //e.g.  geothermal,sensor=hot value=98.60 1712000000000000000
    const char* sensor = topics[i];
    char tempStr[12];
    dtostrf(tempF, 1, 2, tempStr);
    String line = "geothermal,sensor=" + String(sensor) + ",topic=geothermal/data" +
         " value=" + String(tempStr) + " " + String((unsigned long long)epoch * 1000000000ULL);

    if (client.connected()) {
      String mqttTopic = "geothermal/" + String(sensor);
      //publlish line protocol to topic
      if (!client.publish("geothermal/data", line.c_str())) {
        Serial.println("Publish failed — storing locally.");
        storeLocally(line);
      }
    } else {
      storeLocally(line);
    }

    Serial.printf("Sensor %s: %s \n", sensor, line.c_str());
  }
}