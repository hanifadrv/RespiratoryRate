#include "secrets.h"
#include <PeakDetection.h>
#include <Wire.h>
#include <filters.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

PeakDetection peakDetection;

const int MPU_addr=0x68;
int16_t AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;
int minVal=265;
int maxVal=402;
int peakCounter=0;
int previousValue=0;
int programStatus=0;
int ageCat=4;
int rateCat=0;
unsigned long tStart;
unsigned long tStart2;
unsigned long tEnd=60000;
unsigned long tEnd2=50;
float x;
float y;
float z;

const float cutoff_freq   = 0.6;      // High cutoff frequency in Hz
const float sampling_time = 0.01;     //Sampling time in seconds
IIR::ORDER  order  = IIR::ORDER::OD2; // Order (OD1 to OD4)

// Low-pass filter
Filter f(cutoff_freq, sampling_time, order);

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

long lastMsg = 0;
long lastMsg2 =0;
char msg[50];
int value = 0;

void callback(char* topic, byte* message, unsigned int length) {
  //Serial.print("Message arrived on topic: ");
  //Serial.print(topic);
  //Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    //Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  // String messageTemp;
  if (String(topic) == "esp32/initializeMeasurement"){
    Serial.print("Measurement Status: ");
    if(messageTemp == "on"){
      Serial.println("Active");
      client.publish("esp32/clearChart", "clear");
      client.publish("esp32/broker", "AWS IoT connected");
      delay(1000);
      programStatus = 1;
      tStart = millis();
    }
    else if (messageTemp == "off"){
      Serial.println("Inactive");
      programStatus=0;
    }
  }
  else if (String(topic) == "esp32/ageOut"){
    Serial.print("Age: ");
    Serial.println(messageTemp);
    int mT = messageTemp.toInt();
    if (mT >= 1 && mT < 3){
      ageCat=1;
    }
    else if (mT >= 3 && mT < 6){
      ageCat=2;
    }
    else if (mT >= 6 && mT < 12){
      ageCat=3;
    }
    else if (mT >= 12){
      ageCat=4; 
    }
  }
}

void connectAWS()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(callback);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    client.publish("esp32/broker", "AWS IoT disconnected");
    return;
  }
  client.publish("esp32/broker", "AWS IoT connected");
  client.subscribe("esp32/initializeMeasurement");
  client.subscribe("esp32/ageOut");
  Serial.println("AWS IoT Connected");
}

void setup(){
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  Serial.begin(115200);
  peakDetection.begin(30, 3, 0.5);

  connectAWS();
}

void loop(){
  client.loop();
  if (programStatus == 1){
    rateMeasurement();
  }
  else {
    peakCounter=0;
  }
}

void rateMeasurement(){
  long now2 = millis();
  long now = millis();
  if (now - lastMsg > 10) {
    lastMsg = now;
  
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);
  AcX=Wire.read()<<8|Wire.read();
  AcY=Wire.read()<<8|Wire.read();
  AcZ=Wire.read()<<8|Wire.read();
  int xAng = map(AcX,minVal,maxVal,-90,90);
  int yAng = map(AcY,minVal,maxVal,-90,90);
  int zAng = map(AcZ,minVal,maxVal,-90,90);
 
  x= RAD_TO_DEG * (atan2(-yAng, -zAng)+PI);
  y= RAD_TO_DEG * (atan2(-xAng, -zAng)+PI);
  z= RAD_TO_DEG * (atan2(-yAng, -xAng)+PI);

  // Butterworth Filtering
  float filteredval = f.filterIn(x);

  // Peak Detection Algorithm
  peakDetection.add(filteredval);
  int peak = peakDetection.getPeak();

  // Peak Counter
  int currentValue = peak;
  if (currentValue == 1 && previousValue == 0)
  {
    peakCounter++;
  }

  previousValue = currentValue;
  
  if((millis() - tStart) > tEnd){
    char counterString[3];
    dtostrf(peakCounter,1,2,counterString);
    Serial.print("Number of Peaks: "); 
    Serial.println(peakCounter);
    Serial.println(ageCat);
    client.publish("esp32/respirationCounter", counterString);

    if (ageCat == 1){
      if (peakCounter >= 25 && peakCounter <= 35){
        client.publish("esp32/ageIn", "Normal");
      }
      else{
        client.publish("esp32/ageIn", "Abnormal");
      }
    }
    else if (ageCat == 2){
      if (peakCounter >= 20 && peakCounter <= 30){
        client.publish("esp32/ageIn", "Normal");
      }
      else{
        client.publish("esp32/ageIn", "Abnormal");
      }
    }
    else if (ageCat == 3){
      if (peakCounter >= 18 && peakCounter <= 26){
        client.publish("esp32/ageIn", "Normal");
      }
      else{
        client.publish("esp32/ageIn", "Abnormal");
      }
    }
    else if (ageCat == 4){
      if (peakCounter >= 12 && peakCounter <= 20){
        client.publish("esp32/ageIn", "Normal");
      }
      else{
        client.publish("esp32/ageIn", "Abnormal");
      }
    }
    Serial.println("Measurement is completed. Please check the data on Node-red Dashboard");
    programStatus = 0;
    client.publish("esp32/reset", "off");
  }  
  Serial.print(x); Serial.print(", ");
  Serial.print(filteredval); Serial.print(", ");
  Serial.println(peak);
  
  if (now2 - lastMsg2 > 50) {
    lastMsg2 = now2;
    char filteredString[8];
    dtostrf(filteredval,1,2,filteredString);
    client.publish("esp32/filteredAngle", filteredString);

    char peakString[2];
    dtostrf(peak,1,2,peakString);
    client.publish("esp32/inhalePeak", peakString);
  }
  }
}
