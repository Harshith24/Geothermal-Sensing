#include <Arduino.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include "secrets.h"

const int oneWirePin = 4;
OneWire oneWire(oneWirePin);
DallasTemperature sensors(&oneWire);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  sensors.begin();
  client.setServer(MQTT_HOST, 1883);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");

  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
  // sensors.requestTemperatures(); 
  // float temperatureC = sensors.getTempCByIndex(0);
  // float temperatureF = sensors.getTempFByIndex(0);
  // Serial.print(temperatureC);
  // Serial.println("ºC");
  // Serial.print(temperatureF);
  // Serial.println("ºF");
  // delay(5000);

  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Temp_Sensor")) {
      Serial.println("connected");
    } else {
      delay(5000);
      return;
    }
  }
  client.loop();

  sensors.requestTemperatures();
  
  int deviceCount = sensors.getDeviceCount();

  for (int i = 0; i < deviceCount; i++) {
    float tempC = sensors.getTempCByIndex(i);
    
    if (tempC != DEVICE_DISCONNECTED_C) {
      String topic = "sensors/temp/" + String(i);
      
      char tempString[8];
      dtostrf(tempC, 1, 2, tempString);
      
      client.publish(topic.c_str(), tempString);
      
      Serial.print("Sensor ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(tempString);
    }
  }

  delay(5000);
}