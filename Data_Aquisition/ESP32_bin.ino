/************************************************************************************
    @file     ESP32_bin.ino
    @author   Colin Laganier
    @version  V0.3
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

// Variables

// WiFi Parameters
const char *ssid = "Imperial-WPA";

// MQTT Parameters
const char *mqtt_server = "146.169.185.200";
// const char* device_name = "ESP32_bin1";
// const char* mqtt_topic = "smellStation/bin1";
const char *device_name = "ESP32_bin2";
const char *mqtt_topic = "smellStation/bin2";

// Time Server Parameters
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;

// Ultrasound Sensor Parameters
const int triggerPin = 27;
const int echoPin = 34;
const float speedSound = 0.034;
const int numberSamples = 5;
const int maxErrors = 1;
long duration;
float distance;
int failCounter = 0;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Function Prototypes
void SetupWifi();
void callback(char *topic, byte *message, unsigned int length);
void reconnect();
void LocalTime();
bool PublishExpected();
float GetSensorData();
float CalculateVolume();

void LocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

float GetSensorData()
{
  // Clears the trigPin
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance
  distance = duration * speedSound / 2;

  // Prints the distance in the Serial Monitor
  Serial.print("Distance (cm): ");
  Serial.println(distance);

  return distance;
}

float CalculateVolume()
{
  float _measurement;
  float _average = 0;
  int _errorFlag = 0;

  for (int i = 0; i < numberSamples; i++)
  {
    _measurement = GetSensorData();

    // Do not take value into account
    if (_measurement > 750)
    {
      _errorFlag++;
    }
    else
    {
      _average += _measurement;
    }
    delay(500);
  }

  if (_errorFlag > maxErrors)
  {
    return -1;
  }

  int _result = (int)(_average / (numberSamples - _errorFlag) * 100.0);

  return (float)_result / 100.0;
}

bool PublishExpected()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return false;
  }
  if (timeinfo.tm_min % 10 == 0)
  {
    return true;
  }
  return false;
}

void SetupWifi()
{
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

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
}

// Function to reconnect the MQTT client to the broker in case the connection is lost
void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(device_name))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe(mqtt_topic);
      failCounter = 0;
    }
    else
    {
      // Restart ESP if too many errors occured
      if (failCounter == 5)
      {
        ESP.restart()
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

void setup()
{
  // Serial monitor initialisation
  Serial.begin(115200);

  Serial.println(MQTT_topic);

  SetupWifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(triggerPin, OUTPUT); // Trigger output pin
  pinMode(echoPin, INPUT);     // Echo input pin

  configTime(gmtOffset_sec, 0, ntpServer);
  LocalTime();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  if (PublishExpected())
  {
    float _volume = CalculateVolume();
    char _buffer[8];
    dtostrf(_volume, 6, 2, _buffer);
    client.publish(mqtt_topic, _buffer);
    // Sleep for 1 minute
    delay(60000);
  }
}
