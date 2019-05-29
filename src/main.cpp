#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NewPing.h>

#include <config.cpp>

const byte PumpPin1 = D2;
const byte PumpPin2 = D7;
const byte PumpPin3 = D0;
const byte LEDPin = D1;
const byte SonarTrgPin = D5;
const byte SonarEcoPin = D6;
const byte MoisturePin = A0;
const byte WaterDuration_sec = 60; //duration of one watering
const byte minLevel_cm = 1;
const byte rLevelD_cm = 3;
const byte yLevelD_cm = 5;
const byte yLevelU_cm = 7;
const byte gLevelU_cm = 10;
const byte blinkFreq_sec = 3;
const byte sensorHeight = 20;
const byte height = 15;
const byte length = 60;
const byte depth = 25;
const float moistureLimit = 10.0;
float waterLevel = 0;
enum Status {
  STARTED=-1,
  OFF=0,
  ON=1,
  PUMP1=2,
  PUMP2=3,
  PUMP3=4,
  ERROR=5
} status = STARTED, lastStatus = OFF;
const char* statusName[] = {"OFF", "ON", "PUMP1", "PUMP2", "PUMP3", "ERROR"};
unsigned long lastStart = 0;
#define MAX_MSG_LEN (128)

NewPing sonar(SonarTrgPin, SonarEcoPin, sensorHeight);
WiFiClient espClient;
PubSubClient client(espClient);


class MOISTURE {
  const byte pin;
  float moistureLevel;
  float lastMoistureLevel;

public:
  MOISTURE(byte attachTo) :
    pin(attachTo)
  {
  }

  void setup(){
    pinMode(pin, INPUT);
    moistureLevel = (float)analogRead(pin);
    lastMoistureLevel = 0.0;
  }
  void loop(){
    //moistureLevel = (float)analogRead(pin)*100.0f/1024.0f;
    moistureLevel = (float)analogRead(pin);
    Serial.print("Moisture Level: ");
    Serial.println(moistureLevel);
    if(moistureLevel < (lastMoistureLevel - 5) or moistureLevel > (lastMoistureLevel + 5)){
      Serial.println("Moisture Level Changed.");
      client.publish(moisture_topic, String(moistureLevel).c_str(), true);
      lastMoistureLevel = moistureLevel;
    }
  }
};

MOISTURE moisture(MoisturePin);

class LED {
  const byte pin;
  enum LEDStatus{
    OFF=0,
    ON=1,
    BLINK=2
  } LEDstatus;
  unsigned long lastToggle;

public:
  LED(byte attachTo) :
    pin(attachTo)
    {
    }
  void setup(){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    LEDstatus = ON;
  }
  void loop(){
    //Serial.print("Current LED Status: ");
    //Serial.println(status);
    switch (LEDstatus){
      case OFF:
        digitalWrite(pin, LOW);
        Serial.println("LED Off.");
        break;
      case ON:
        digitalWrite(pin, HIGH);
        Serial.println("LED On.");
        break;
      case BLINK:
        if (millis() >= lastToggle + (blinkFreq_sec*1000)){
          digitalWrite(pin, !digitalRead(pin));
          Serial.println("LED blinking.");
          lastToggle = millis();
        }
        break;
    }
  }

  void setStatus(byte val) {
    //Serial.println("<<<<<I'm called");
    //Serial.println(val);
    switch (val){
      case 0:
        LEDstatus = OFF;
        //Serial.println("OFF");
        break;
      case 1:
        LEDstatus = ON;
        //Serial.println("ON");
        break;
      case 2:
        LEDstatus = BLINK;
        //Serial.println("BLINK");
        break;
    }
  }
};

LED led(LEDPin);

class WATERLEVEL {
  byte lastWaterLevel_cm;
  byte curWaterLevel_cm;

public:

  void setup(){
    lastWaterLevel_cm = 0;
  }
  void loop(){
    curWaterLevel_cm = sensorHeight - sonar.convert_cm(sonar.ping_median(5));
    Serial.print("Waterlevel: ");
    Serial.println(curWaterLevel_cm);
    if(curWaterLevel_cm <= minLevel_cm){
      status = ERROR;
      led.setStatus(2);
      //Serial.println("Debug Pump 1");
    }
    else if (curWaterLevel_cm > minLevel_cm and status == ERROR){
      status = OFF;
    }
    else if(curWaterLevel_cm > minLevel_cm and curWaterLevel_cm <= rLevelD_cm){
      led.setStatus(2);
      //Serial.println("Debug Pump 2");
    }
    else if(curWaterLevel_cm > rLevelD_cm and curWaterLevel_cm <= yLevelU_cm){
      if (curWaterLevel_cm > lastWaterLevel_cm){
        led.setStatus(2);
        //Serial.println("Debug Pump 3");
      }
      else {
        led.setStatus(1);
        //Serial.println("Debug Pump 4");
      }
    }
    else if(curWaterLevel_cm > yLevelU_cm and curWaterLevel_cm <= yLevelD_cm){
      led.setStatus(1);
      //Serial.println("Debug Pump 5");
    }
    else if(curWaterLevel_cm > yLevelD_cm and curWaterLevel_cm <= gLevelU_cm){
      if (curWaterLevel_cm > lastWaterLevel_cm){
        led.setStatus(1);
        //Serial.println("Debug Pump 6");
      }
      else {
        led.setStatus(0);
        //Serial.println("Debug Pump 7");
      }
    }
    else if(curWaterLevel_cm > gLevelU_cm){
      led.setStatus(0);
      //Serial.println("Debug Pump 8");
    }

    if (curWaterLevel_cm < lastWaterLevel_cm-1 or curWaterLevel_cm > lastWaterLevel_cm+1){
      waterLevel=curWaterLevel_cm;
      client.publish(waterlevel_topic, String(waterLevel).c_str(), true);
      client.publish(watervolume_topic, String((float)waterLevel*(float)length*(float)depth/1000).c_str(), true);
      Serial.println("MQTT Message updated.");
      lastWaterLevel_cm = curWaterLevel_cm;
    }
  }
};

WATERLEVEL wLevel;





void setup_wifi() {
 delay(10);
 // We start by connecting to a WiFi network
 Serial.println();
 Serial.print("Connecting to ");
 Serial.println(wifi_ssid);
 WiFi.begin(wifi_ssid, wifi_password);
 while (WiFi.status() != WL_CONNECTED) {
  delay(500);
  Serial.print(".");
 }
 Serial.println("");
 Serial.println("WiFi connected");
 Serial.println("IP address: ");
 Serial.println(WiFi.localIP());
}

void reconnect() {
 // Loop until we're reconnected
 while (!client.connected()) {
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect("FLOWERPOT")) {
      Serial.println("MQTT connected");
      // Once connected, publish an announcement...
      client.publish(status_topic, "OFF");
      // ... and resubscribe
      client.subscribe(status_topic);
  } else {
    Serial.print("MQTT failed, state ");
    Serial.print(client.state());
    Serial.println(", retrying...");
    // Wait before retrying
    delay(2500);
  }
 }
}

void callback(char *msgTopic, byte *msgPayload, unsigned int msgLength) {
  // copy payload to a static string
  static char message[MAX_MSG_LEN+1];
  int mylength = length;
  if (length > MAX_MSG_LEN) {
    mylength = MAX_MSG_LEN;
  }
  strncpy(message, (char *)msgPayload, mylength);
  message[mylength] = '\0';

  Serial.print("topics message received: ");
  Serial.print(msgTopic);
  Serial.print(" / ");
  Serial.println(message);

  if (strcmp(message, statusName[status]) != 0) {
    Serial.println("Changed Status received");
    if (strcmp(message, "ON") == 0) {
      status = ON;
      lastStart = millis();
    } else if (strcmp(message, "OFF") == 0) {
      status = OFF;
    }
  }
}

void setup()
{
  //initialize serial
  Serial.begin(74880);
  while (!Serial); // Leonardo: wait for serial monitor

  //initiate WiFi
  setup_wifi();

  //connect MQTT
  client.setServer(mqtt_server, 18830);
  if (!client.connected()) {
   Serial.println("MQTT not connected - trying reconnect!");
   reconnect();
  }
  client.setCallback(callback);

  moisture.setup();
  led.setup();
  wLevel.setup();

}

void loop() {
  client.loop();
  moisture.loop();
  led.loop();
  wLevel.loop();
  if (status != lastStatus){
    Serial.print("Updating MQTT Status to: ");
    Serial.println(String(statusName[status]).c_str());
    client.publish(status_topic, String(statusName[status]).c_str(), true);
    lastStatus = status;
  }
  if (status != ERROR) {
    switch(status) {
      case ON:
        digitalWrite(PumpPin1, HIGH);
        digitalWrite(PumpPin2, LOW);
        digitalWrite(PumpPin3, LOW);
        status = PUMP1;
        break;
      case PUMP1:
        if(lastStart + (WaterDuration_sec*1000) < millis()) {
          digitalWrite(PumpPin1, LOW);
          digitalWrite(PumpPin2, HIGH);
          digitalWrite(PumpPin3, LOW);
          status = PUMP2;
        }
        break;
      case PUMP2:
        if(lastStart + (WaterDuration_sec*1000*2) < millis()) {
          digitalWrite(PumpPin1, LOW);
          digitalWrite(PumpPin2, LOW);
          digitalWrite(PumpPin3, HIGH);
          status = PUMP3;
        }
        break;
      case PUMP3:
        if(lastStart + (WaterDuration_sec*1000*3) < millis()) {
          digitalWrite(PumpPin1, LOW);
          digitalWrite(PumpPin2, LOW);
          digitalWrite(PumpPin3, LOW);
          status = OFF;
        }
        break;
    }
  }
  //xxx delay(100);
  delay(1000);
}
