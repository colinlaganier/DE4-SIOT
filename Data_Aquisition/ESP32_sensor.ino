/************************************************************************************
    @file     ESP32_sensor.ino
    @author   Colin Laganier
    @version  V0.1
    @date     2021-12-05
    @brief   This
  **********************************************************************************
    @attention  Requires standard Arduino Libraries: PubSubClient.h
*/

#include "passwords.h"
#include "esp_wpa2.h"
#include "time.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// Variables

// WiFi Parameters
const char* ssid = "Imperial-WPA";

// MQTT Parameters
const char* mqtt_server = "146.169.185.200";
const char* device_name = "ESP32_sensor";
const char* mqtt_topic[3] = { "smellStation/H2S", "smellStation/temperature", "smellStation/humidity" };
int failCounter = 0;

// Time Server Parameters
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;

// DHT Sensor Parameters
DHTesp dht;
const int dhtPin = 32;

// H2S Gas Sensor Parameters
const int sensorPin = 33;
int _gasMeasurement;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Function Prototypes
void setup_wifi();
void callback(char* topic, byte* message, unsigned int length);
void reconnect();
bool PublishExpected();
float GetSensorData();
int CalculateVolume();

bool PublishExpected()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  if (timeinfo.tm_min % 10 == 0)
  {
    return true;
  }
  return false;

}

void setup_wifi() {
  Serial.println((String)EAP_USERNAME);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  // WPA2 Enterprise Connection
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ID, strlen(EAP_ID));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
  esp_wifi_sta_wpa2_ent_enable(&config);

  WiFi.begin(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

// Function to reconnect the MQTT client to the broker in case the connection is lost
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(device_name)) {
      Serial.println("connected");
      failCounter = 0;
      for (int i = 0; i < 3; i++)
      {
        client.subscribe(mqtt_topic[i]);
      }
    } else {
      if (failCounter == 5){
        ESP.restart();
      }
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      failCounter++;
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  // Serial monitor initialisation
  Serial.begin(115200);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.setup(dhtPin, DHTesp::DHT11);

  configTime(gmtOffset_sec, 0, ntpServer);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  //  client.loop();
  if (PublishExpected())
  {
    TempAndHumidity _measurement = dht.getTempAndHumidity();
    if (dht.getStatus() != 0)
    {
      Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    }
    else
    {
      char _temperatureBuffer[8];
      dtostrf(_measurement.temperature, 6, 2, _temperatureBuffer);
      client.publish(mqtt_topic[1], _temperatureBuffer);

      char _humidityBuffer[8];
      dtostrf(_measurement.humidity, 6, 2, _humidityBuffer);
      client.publish(mqtt_topic[2], _humidityBuffer);
    }

    _gasMeasurement = analogRead(sensorPin);
    char _H2SBuffer[8];
    itoa(_gasMeasurement, _H2SBuffer, 10);
    client.publish(mqtt_topic[0], _H2SBuffer);
    Serial.println((String) "temp: " + _measurement.temperature + " humidity: " + _measurement.humidity + " H2S: " + _H2SBuffer);
    delay(60000);
  }
  client.loop();


}
