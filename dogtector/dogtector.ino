/*
   Dogtector - operating code for the Dogtector

   Copyright Alistair Young, 2017. All Rights Reserved.
*/

#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <PubSubClient.h>

// Expose Espressif SDK functionality - wrapped in ifdef so that it still
// compiles on other platforms
#ifdef ESP8266
extern "C" {
#include "user_interface.h"
}
#endif

// WiFi setup

const char* ssid = "Arkane Systems";
const char* password = "VeryMuchNotMyRealWifiPassword";

// MQTT server

const char* mqttserver = "calmirie.arkane-systems.lan";
const char* pubTopic = "sensors/dogtector";
const char* subTopic = "enable/dogtector";

// Create ESP8266 WiFiClient class to connect to the MQTT server
WiFiClient wifiClient;

PubSubClient client (wifiClient);

// pins

int statusPin = 0;
int detectPin = 2;
int pirPin = 4;
int speakerPin = 14;

// tickers

Ticker statusBlink;
Ticker pirScan;
Ticker buzzer;

// PIR scanner

int pirState = LOW;           // assume no motion detected on start
int reading = 0 ;             // temp reading used by scan()

int skipFirst = 1;            // PIR always active on boot

// buzzer

int numTones = 2;
int tones[] = {440, 300};

int buzzState = 0;

void setup()
{
  // Setup the system.
  Serial.begin (9600);
  Serial.println("DOGTECTOR: Setup");

  // set up the LED pins
  pinMode(statusPin, OUTPUT);
  pinMode(detectPin, OUTPUT);
  setDetectLedDisabled();

  // set up the PIR pin
  pinMode(pirPin, INPUT);

  // set up the WiFi
  wifi_station_set_hostname("iot-dogtector");
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());

  // Setup MQTT client.
  client.setServer (mqttserver, 1883);
  client.setCallback (MQTT_callback);

  // Test buzzer.
  buzz();

  // Enable detector.
  enableDetector();
}

void loop()
{
  // MQTT server connect and loop
  if (!client.connected())
  {
    MQTT_connect();
  }

  client.loop();
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("Dogtector"))
    {
      Serial.println("connected");
      
      // ... and subscribe to topic
      client.subscribe(subTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void MQTT_callback (char* topic, byte* payload, unsigned int length)
{
  Serial.print("Received MQTT topic=");
  Serial.println(topic);
  
  // check topic
  if (strcmp (topic, subTopic) != 0)
    return;

  // check length
  if (length != 1)
    return;

  if ((char)payload[0] == '0')
  {
    Serial.println("disabling");
    disableDetector();
    return;
  }
  
  if ((char)payload[0] == '1')
  {
    Serial.println("enabling");
    enableDetector();
    return;
  }

  Serial.println("unknown enable, noop");
}

void enableDetector()
{
  pirScan.attach (1, scan);

  setStatusEnabled();
}

void disableDetector()
{
  pirScan.detach();
  pirState = LOW;
  reading = 0;
  setDetectLedDisabled();

  setStatusDisabled();
}

void scan ()
{
  reading = digitalRead (pirPin);

  if (reading == HIGH)  // motion currently detected
  {
    if (pirState == LOW)  // motion not previously detected
    {
      // we have just turned on
      pirState = HIGH;

      if (!skipFirst)
      {
        setDetectLedEnabled();

        // communicate this to the MQTT server
        int result = client.publish (pubTopic, "1");
        
        Serial.print ("detect-publish");

        buzz();
      }
    }
  }
  else // motion not detected
  {
    if (pirState == HIGH) // motion was previously detected
    {
      // we have just turned off
      pirState = LOW ;
      setDetectLedDisabled();

      skipFirst = 0;
    }
  }
}

void setStatusEnabled ()
{
  statusBlink.attach (0.5, blink);
}

void setStatusDisabled ()
{
  statusBlink.detach();
  digitalWrite(statusPin, LOW);
}

void setDetectLedEnabled ()
{
  digitalWrite(detectPin, LOW);
}

void setDetectLedDisabled ()
{
  digitalWrite(detectPin, HIGH);
}

void blink()
{
  int state = digitalRead(statusPin);
  digitalWrite(statusPin, !state);
}

void buzz()
{
  if (buzzState != 0)   // already buzzing
    return;

  buzz_impl();
}

void buzz_impl ()
{
  if (buzzState == 0)
  {
    buzzer.attach (0.5, buzz_impl);
  }

  if (buzzState < numTones)
  {
    tone (speakerPin, tones[buzzState]);
    buzzState++;
  }
  else
  {
    noTone (speakerPin);
    buzzer.detach();
    buzzState = 0;
  }
}

