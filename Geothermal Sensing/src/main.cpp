#include <Arduino.h>
#include <WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>

const int oneWirePin = 4;
OneWire oneWire(oneWirePin);
DallasTemperature sensors(&oneWire);

const char* ssid = "Yugo River Market";       
const char* password = "LemonGuineaPig34";

const char* mqtt_server = "100.70.63.139"; // Your Laptop's IP
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  sensors.begin();
  client.setServer(mqtt_server, 1883);

  WiFi.begin(ssid, password);
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
    // Reconnect logic here
    client.connect("ESP32_Temp_Sensor");
  }
  client.loop();

  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  
  // Convert float to string and publish
  char tempString[8];
  dtostrf(tempC, 1, 2, tempString);
  client.publish("sensors/temperature", tempString);

  delay(5000);
}