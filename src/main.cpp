#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NewPing.h>

#include <config.cpp>

const byte PumpPin = D2;
const byte LEDPin = D1;
const byte SonarTrgPin = D5;
const byte SonarEcoPin = D6e;
const byte MoisturePin = A0;
const byte WaterInterval_h = 5; //time between 2 waterings
const byte WaterDuration_sec = 120; //duration of one watering
const byte WaitDuration_min = 5; //how long to wait after watering before normal operation continues
const byte minLevel_cm = 1;
const byte rLevelD_cm = 3;
const byte yLevelD_cm = 5;
const byte yLevelU_cm = 7;
const byte gLevelU_cm = 10;
const byte blinkFreq_sec = 3;
const byte height = 15;
const byte length = 60;
const byte depth = 25;
const float moistureLimit = 10.0;


NewPing sonar(SonarTrgPin, SonarEcoPin);
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
    moistureLevel = 0.0;
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
    if (moistureLevel < moistureLimit){
      //activate pump
    }
  }
};

MOISTURE moisture(MoisturePin);

class PUMP {
  const byte pin;
  unsigned long nextStart;
  enum Status {
    OFF=0,
    ON=1,
    WAIT=2
  } status;
  enum Active {
    FALSE=0,
    TRUE=1
  } active;

public:
  PUMP(byte attachTo) :
    pin(attachTo)
  {
  }

  void setup(){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    status = OFF;
    active = TRUE;
    nextStart = millis();
  }

  void loop(){
    if(active == TRUE) {
      switch(status){
        case OFF:
          if (millis() >= nextStart){
            digitalWrite(pin, HIGH);
            status=ON;
            Serial.println("*****Pump activated.");
            client.publish(status_topic, String("ON").c_str(), true);
            break;
          }
        case ON:
          if (millis() >= nextStart + (WaterDuration_sec*1000)) {
            digitalWrite(pin, LOW);
            status=WAIT;
            Serial.println("*****Pump deactivated. Waiting period.");
            client.publish(status_topic, String("WAIT").c_str(), true);
            break;
          }
        case WAIT:
          if (millis() >= nextStart + (WaterDuration_sec*1000) + (WaitDuration_min*1000)){
          //if (millis() >= nextStart + (WaterDuration_sec*1000) + (WaitDuration_min*60*1000)){
            status=OFF;
            Serial.println("*****Waiting time over. Pump off.");
            client.publish(status_topic, String("OFF").c_str(), true);
            nextStart = nextStart + (WaterInterval_h*1000);
            //nextStart = nextStart + (WaterInterval_h*60*60*1000);
          }
          break;
      }
    }
    else {
      digitalWrite(pin, LOW);
      status=OFF;
    }
  }

  void setActive(byte val){
    if(val == 1){
      active = TRUE;
    }
    else {
      active = FALSE;
    }
  }
};

PUMP pump(PumpPin);

class LED {
  const byte pin;
  enum Status{
    OFF=0,
    ON=1,
    BLINK=2
  } status;
  unsigned long lastToggle;

public:
  LED(byte attachTo) :
    pin(attachTo)
    {
    }
  void setup(){
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    status = ON;
  }
  void loop(){
    //Serial.print("Current LED Status: ");
    //Serial.println(status);
    switch (status){
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
        status = OFF;
        //Serial.println("OFF");
        break;
      case 1:
        status = ON;
        //Serial.println("ON");
        break;
      case 2:
        status = BLINK;
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
    curWaterLevel_cm = height - sonar.convert_cm(sonar.ping_median(5));
    Serial.print("Waterlevel: ");
    Serial.println(curWaterLevel_cm);
    if(curWaterLevel_cm <= minLevel_cm){
      pump.setActive(0);
      led.setStatus(2);
      //Serial.println("Debug Pump 1");
    }
    else if(curWaterLevel_cm > minLevel_cm and curWaterLevel_cm <= rLevelD_cm){
      pump.setActive(1);
      led.setStatus(2);
      //Serial.println("Debug Pump 2");
    }
    else if(curWaterLevel_cm > rLevelD_cm and curWaterLevel_cm <= yLevelU_cm){
      pump.setActive(1);
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
      pump.setActive(1);
      led.setStatus(1);
      //Serial.println("Debug Pump 5");
    }
    else if(curWaterLevel_cm > yLevelD_cm and curWaterLevel_cm <= gLevelU_cm){
      pump.setActive(1);
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
      pump.setActive(1);
      led.setStatus(0);
      //Serial.println("Debug Pump 8");
    }

/*
    switch(curWaterLevel_cm){
      case 0 ... minLevel_cm:
        pump.setActive(0);
        led.setStatus(2);
      case minLevel_cm+1 ... rLevelD_cm:
        pump.setActive(1);
        led.setStatus(2);
      case rLevelD_cm+1 ... yLevelU_cm:
        pump.setActive(1);
        if (curWaterLevel_cm > lastWaterLevel_cm){
          led.setStatus(2);
        }
        else {
          led.setStatus(1);
        }
      case yLevelU_cm+1 ... yLevelD_cm:
        pump.setActive(1);
        led.setStatus(1);
      case yLevelD_cm+1 ... gLevelU_cm:
        pump.setActive(1);
        if (curWaterLevel_cm > lastWaterLevel_cm){
          led.setStatus(1);
        }
        else {
          led.setStatus(0);
        }
      case gLevelU_cm+1 ... height:
        pump.setActive(1);
        led.setStatus(0);
    }
*/
    if (curWaterLevel_cm < lastWaterLevel_cm-1 or curWaterLevel_cm > lastWaterLevel_cm+1){
      client.publish(waterlevel_topic, String(curWaterLevel_cm).c_str(), true);
      client.publish(watervolume_topic, String((float)curWaterLevel_cm*(float)length*(float)depth/1000).c_str(), true);
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
  if (client.connect("nodemcu", mqtt_user, mqtt_password)) {
  Serial.println("connected");
  } else {
  Serial.print("failed, rc=");
  Serial.print(client.state());
  Serial.println(" try again in 2 seconds");
  delay(2000);
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
  client.loop();

  moisture.setup();
  pump.setup();
  led.setup();
  wLevel.setup();

}

void loop() {
  moisture.loop();
  pump.loop();
  led.loop();
  wLevel.loop();
  //xxx delay(100);
  delay(10000);
}
