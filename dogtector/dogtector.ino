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

// Network -- WiFi and MQTT configuration -----------------

// WiFi setup

const char* ssid = "Arkane Systems";
const char* password = "LauraFraser09";

// MQTT server

const char* mqttserver = "ariadne.arkane-systems.lan";
const char* subTopic = "dogtector/command";
const char* pubStatusTopic = "dogtector/status";
const char* pubAlertTopic = "dogtector/alert";
const char* pubDoorTopic = "dogtector/door";

// Create ESP8266 WiFiClient class to connect to the MQTT server
WiFiClient wifiClient;

PubSubClient client (wifiClient);

// Pins -- pin definitions --------------------------------

#define statusPin 0
#define detectPin 2
#define pirPin 4
#define speakerPin 14
#define doorPin 12

// Tickers -- periodic calls to scanning routines ---------

Ticker doorScan;
Ticker pirScan;

// Setup -- set up the Dogtector --------------------------

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

  // set up the door pin
  pinMode(doorPin, INPUT_PULLUP);

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

  // Enable door monitor.
  enableDoorMonitor();

  // Enable detector.
  enableDetector();
}

// Loop -- run the MQTT loop forever ---------------------

int enabled = 0;

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

// Process incoming MQTT messages.
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

  // if we are asked for a status report, give it
  if ((char)payload[0] == 'S')
  {
    Serial.println("status report");
    if (enabled == 1)
      client.publish(pubStatusTopic, "1");
    else
      client.publish(pubStatusTopic, "0");
    return;
  }

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

// Buzz -- sound piezo buzzer -----------------------------

Ticker buzzer;

int numTones = 2;
int tones[] = {440, 300};

int buzzState = 0;

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

// LED -- LED management ----------------------------------

Ticker statusBlink;

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

// Door -- check open/closed status of door ---------------

int doorState = HIGH ;         // assume door closed on start
int doorFirstPass = 1;

void doorScanProc ()
{
  int reading = digitalRead (doorPin);

//  Serial.print ("door reading: ");
//  Serial.println (reading);

  if (reading == HIGH) // door is open
  {
    if (doorState == LOW || doorFirstPass == 1) // door was previously closed, or first time through
    {
      // we have opened
      doorState = HIGH ;

      // communicate this to the MQTT server
      int result = client.publish (pubDoorTopic, "1");
      Serial.println ("door open");
    }
  }
  else // door is closed
  {
    if (doorState == HIGH || doorFirstPass == 1) // door was previously open, or first time through
    {
      // we have closed
      doorState = LOW ;

      // communicate this to the MQTT server
      int result = client.publish (pubDoorTopic, "0");
      Serial.println ("door closed");
    }
  }

  doorFirstPass = 0;
}

// PIR -- scan for dogs in the IR zone --------------------

int pirState = LOW;           // assume no motion detected on start
int skipFirst = 1;            // PIR always active on boot

void pirScanProc ()
{
  int reading = digitalRead (pirPin);

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
        int result = client.publish (pubAlertTopic, "1");
        
        Serial.println ("IR detect!");

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

// Enable/Disable -- control dogtector functionality ------

void enableDetector()
{
  pirScan.attach (1, pirScanProc);

  setStatusEnabled();
}

void disableDetector()
{
  pirScan.detach();
  pirState = LOW;
  setDetectLedDisabled();

  setStatusDisabled();
}

void enableDoorMonitor()
{
  doorScan.attach (1, doorScanProc);
}



