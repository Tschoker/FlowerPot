#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NewPing.h>

#include <config.cpp>

#define MAX_MSG_LEN (128)

const byte PumpPin1 = D2;
const byte PumpPin2 = D7;
const byte PumpPin3 = D0;
const byte LEDPin = D1;
const byte SonarTrgPin = D5;
const byte SonarEcoPin = D6;
const byte MoisturePin = A0;
const byte WaterDuration_sec = 60; //duration of one watering
const byte minLevel_cm = 1;
const byte rLevelD_cm = 2;
const byte yLevelD_cm = 3;
const byte yLevelU_cm = 4;
const byte gLevelU_cm = 7;
const byte blinkFreq_sec = 3;
const byte sensorHeight = 15;
//const byte height = 15;
const byte length = 60;
const byte depth = 25;
const float moistureLimit = 10.0;
float waterLevel = 0;
unsigned long lastHeartbeat = 0;
char lastHeartbeatVal[MAX_MSG_LEN+1] = "";

enum Status {
  OFF=0,
  ON=1,
  PUMP1=2,
  PUMP2=3,
  PUMP3=4,
  ERROR=5,
  STARTED=6
} status = OFF, lastStatus = STARTED;
const char* statusName[] = {"OFF", "ON", "PUMP1", "PUMP2", "PUMP3", "ERROR", "STARTED"};
unsigned long lastStart = 0;


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
    //Serial.print("Moisture Level: ");
    //Serial.println(moistureLevel);
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
        //Serial.println("LED Off.");
        break;
      case ON:
        digitalWrite(pin, HIGH);
        //Serial.println("LED On.");
        break;
      case BLINK:
        if (millis() >= lastToggle + (blinkFreq_sec*1000)){
          digitalWrite(pin, !digitalRead(pin));
          //Serial.println("LED blinking.");
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
    //Serial.print("Waterlevel: ");
    //Serial.println(curWaterLevel_cm);
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

  if (client.connect("FLOWERPOT", mqtt_user, mqtt_password, will_topic, 0, 1, "false")) {
      Serial.println("MQTT connected");
      // Once connected, publish an announcement...
      client.publish(status_topic, "OFF");
      client.publish(will_topic, "true");
      delay(1000);
      // ... and resubscribe
      Serial.println("Subscribe to Status");
      client.subscribe(status_topic);
      delay(500);
      client.subscribe(heartbeat_topic);

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
  //int mylength = length;
  if (msgLength > MAX_MSG_LEN) {
    //mylength = MAX_MSG_LEN;
    strncpy(message, (char *)msgPayload, MAX_MSG_LEN);
    message[MAX_MSG_LEN] = '\0';
  }
  else {
    strncpy(message, (char *)msgPayload, msgLength);
    message[msgLength] = '\0';
  }

  Serial.print("topics message received: ");
  Serial.print(msgTopic);
  Serial.print(" / ");
  Serial.print(message);
  Serial.print(" / ");
  Serial.println(msgLength);

  if (String(msgTopic) == status_topic) {
    if (!String(message).startsWith(statusName[status])) {
    //if (strcmp(message, statusName[status]) != 0) {
      Serial.println("Changed Status received");
      if (String(message).startsWith("ON")) {
        Serial.println("Set status to on");
        status = ON;
        lastStatus = ON; //to prevent resending the just received status
        lastStart = millis();
      } else if (String(message).startsWith("OFF")) {
        Serial.println("Set status to off");
        status = OFF;
        lastStatus = OFF; //to prevent resending the just received status
      }
    }
  }
  if (String(msgTopic) == heartbeat_topic) {
    if (String(message)!=String(lastHeartbeatVal)) {
      Serial.print("New Heartbeat Value: ");
      Serial.println(message);
      //lastHeartbeatVal=String(message);
      strncpy(lastHeartbeatVal, (char *)message, MAX_MSG_LEN+1);
      lastHeartbeat = millis();
    }
  }
}

void setup()
{
  //initialize serial
  Serial.begin(74880);
  while (!Serial); // Leonardo: wait for serial monitor

  pinMode(PumpPin1, OUTPUT);
  digitalWrite(PumpPin1, LOW);
  pinMode(PumpPin2, OUTPUT);
  digitalWrite(PumpPin2, LOW);
  pinMode(PumpPin3, OUTPUT);
  digitalWrite(PumpPin3, LOW);

  //initiate WiFi
  setup_wifi();

  //connect MQTT
  client.setServer(mqtt_server, 1883);
  if (!client.connected()) {
   Serial.println("MQTT not connected - trying reconnect!");
   reconnect();
  }
  client.setCallback(callback);

  delay(500);
  moisture.setup();
  led.setup();
  wLevel.setup();

}

void loop() {
  client.loop();
  moisture.loop();
  led.loop();
  wLevel.loop();
  Serial.print("Status: ");
  Serial.print(String(statusName[status]).c_str());
  Serial.print(" / ");
  Serial.println(String(statusName[lastStatus]).c_str());
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
      case OFF:
          digitalWrite(PumpPin1, LOW);
          digitalWrite(PumpPin2, LOW);
          digitalWrite(PumpPin3, LOW);
          status = OFF;
          break;
    }
  }
  //xxx delay(100);
  if (lastHeartbeat + 120000 < millis()){
    Serial.println("Heartbeat lost - restart");
    ESP.restart();
  }
  delay(100);
}
