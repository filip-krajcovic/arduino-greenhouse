#include <DHT.h>
#include <Wire.h>
#include <Servo.h>
#include <ArduinoMqttClient.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"
#include "Arduino.h"
#include "uRTCLib.h"

// Set DHT pin:
#define DHTPIN 7

#define DS3231_I2C_ID 0x68    
 
#define DHTTYPE DHT22 
  
#define sensorPower 8
#define sensorPin A0

DHT dht = DHT(DHTPIN, DHTTYPE);

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo myServo; // create servo object to control a servo
// a maximum of eight servo objects can be created

int servoPin = 9;
const int pumpPin = 10;
int lightsPin = 11;

int OnHour = 0;
int OnMin = 0;
int OffHour = 0;
int OffMin = 0;

int pos = 0; // variable to store the servo position

// sensitive data are stored in arduino_secrets.h
char ssid[] = SECRET_SSID;     // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

const char* mqtt_host = SECRET_MQTT_HOST;
const int mqtt_port = 1883; //= SECRET_MQTT_PORT;
char mqtt_clientid[] = SECRET_MQTT_CLIENT_ID;
char mqtt_outTopic[] = SECRET_MQTT_OUT_TOPIC;
char mqtt_username[] = SECRET_MQTT_USERNAME;
char mqtt_password[] = SECRET_MQTT_PASSWORD;

#define TIMEOUT 5000

WiFiClient wifiClient;
PubSubClient client(wifiClient);

unsigned long previousMillisTemperature = 0; 
unsigned long previousMillis = 0;
unsigned long previousMillisSoilMoisture = 0;


const long intervalTemperature = 5000;
const long intervalSoilMoisture = 10000;
const long intervalTimer = 1000;

unsigned long currentMillis = 0;
 

void setup() {   
  
  pinMode(sensorPower, OUTPUT);
  pinMode(pumpPin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(lightsPin,OUTPUT);

  myServo.attach(servoPin);// attaches the servo on pin 9 to the servo object
  myServo.write(0);
  myServo.detach();
  // Initially keep the sensor OFF
  digitalWrite(sensorPower, LOW);
  
  // Begin serial communication at a baud rate of 9600:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  // Setup sensor:
  dht.begin();
  Wire.begin();
  
  lcd.init();
  lcd.backlight();
  // Print a message to the LCD.
  lcd.setCursor(0,0);
  lcd.print("Connecting to ");
  lcd.setCursor(0,1);
  lcd.print(ssid);

  setDate(00, 9, 10, 6, 8, 12, 23);  // 24 hour format; Sun=1 

  connectToNetwork();
  delay(1000);
  connectToEmqx();
  }

  
float lastH;
float lastT;
float lastP;
int percentage;


void loop() {
    // Wait a few seconds between measurements:
  delay(TIMEOUT);

  currentMillis = millis();
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

  // Read the humidity in %:
  float h = dht.readHumidity();
  // Read the temperature as Celsius:
  float t = dht.readTemperature();
  //get the reading from the function below and print it

  float m = readMoistureSensor();
  percentage = 100-(m-0) /( 1023.0-0) * 100;

  // Check if any reads failed and exit early (to try again):
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  //calculate difference between current values and previous values
  float diffH = fabs(h - lastH);
  float diffT = fabs(t - lastT);
  float diffP = fabs(percentage - lastP);

  if (diffH > 1) {
    Serial.print ("Vlhkost sa zmenila o ");
    Serial.println (diffH);
    if(h >= 1 && h < 60)
    {
      Serial.print("( Slabá vlhkosť ! )");
      Serial.println("");
    }
    if(h >= 60 && h < 85)
    {
      Serial.print("( Ideálna vlhkosť ! )");
      Serial.println("");
    }
    if(h >= 85 && h <= 100)
    {
      Serial.print("( Veľmi vlhko ! )");
      Serial.println("");
    }
    publish("arduino/humidity", h);
  }
    
  if (diffT > 0.1) {
    Serial.print("Teplota sa zmenila o ");
    Serial.println (diffT);
    if(t >= 1 && t < 30)
    {
      Serial.print("( Nízka teplota )");
      Serial.println("");
    }
    if(t >= 30 && t <= 40)
    {
      Serial.print("( Ideálna teplota )");
      Serial.println("");
    }
    if(t >= 41 && t <= 50)
    {
      Serial.print("( Vysoká teplota )");
      Serial.println("");
    }
    publish("arduino/temperature", t);
    
  }
  if (diffP > 1) {
    Serial.print("Vlhkost pody sa zmenila o ");
    Serial.println (diffP);
    publish("arduino/soil-moisture", percentage);

    if(percentage >= 1 && percentage < 50)
    {
      //digitalWrite(pumpPin, HIGH);
      Serial.print("( Too dry ! Prebieha polievanie )");
      Serial.println("");
    }
    if(percentage >= 50 && percentage < 70)
    {
      //digitalWrite(pumpPin, LOW);
      Serial.print("( Ideal humidity ! )");
      Serial.println("");
    }
    if(percentage >= 70 && percentage <= 100)
    {
      Serial.print("( Too wet ! )");
      Serial.println("");
    }
  }
    
    if (currentMillis - previousMillisTemperature >= intervalTemperature  ) {
      
       previousMillisTemperature = currentMillis;

       lcd.clear();
       lcd.setCursor(0,0);
       lcd.print("Teplota");
       lcd.setCursor(8,0);
       lcd.print(t);
       lcd.setCursor(13,0);
       lcd.print("C");
       lcd.setCursor(0,1);
       lcd.print("Vlhkost");
       lcd.setCursor(8,1);
       lcd.print(h);
       lcd.setCursor(13,1);
       lcd.print("%");
    }
  
    if ( currentMillis - previousMillisSoilMoisture >= intervalSoilMoisture ) {
    
      previousMillisSoilMoisture = currentMillis;
       
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Vlhkost");
      lcd.setCursor(0,1);
      lcd.print("pody");
      lcd.setCursor(5,1);
      lcd.print(percentage);
      lcd.setCursor(6,1);
      lcd.print("%");
    }
  
    //store last values
    lastH = h;
    lastT = t;
    lastP = percentage;

    setDate;
    readDate();
    delay(1000);
    
    client.loop();
  }

float readMoistureSensor() {
  digitalWrite(sensorPower, HIGH);  // Turn the sensor ON
  delay(10);              // Allow power to settle
  float val = analogRead(sensorPin);  // Read the analog value form sensor
  digitalWrite(sensorPower, LOW);   // Turn the sensor OFF
  return val;             // Return analog moisture value
}



void publish(char topic[], float value) {

  //if (client.connected()) {
    StaticJsonDocument<256> doc;
    doc = value;
    //[topic] 
    char out[128];
    int b =serializeJson(doc, out);
    Serial.print("topic = ");
    Serial.print(topic);
    Serial.print("| publishing value = ");
    Serial.print(value);
    Serial.print("| publishing bytes = ");
    Serial.println(b,DEC);

    boolean rc = client.publish(topic, out);
  
}
void setDate(byte Second, byte Minute, byte Hour, byte WeekDay, byte Day, byte Month, byte Year) {
  Wire.beginTransmission(DS3231_I2C_ID);
  Wire.write(0x0);  // register address for the time and date
  Wire.write(dec2bcd(Second));
  Wire.write(dec2bcd(Minute));
  // Hour must be in 24h format
  bool h12 = 1;  // set 12 hour mode
  Hour = dec2bcd(Hour) & 0b10111111;
  Wire.write(Hour);
  Wire.write(dec2bcd(WeekDay));
  Wire.write(dec2bcd(Day));
  Wire.write(dec2bcd(Month));
  Wire.write(dec2bcd(Year));
  Wire.endTransmission();
}
void readDate() {
  Wire.beginTransmission(DS3231_I2C_ID);
  Wire.write(0x0);  // register address for the time and date
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ID, 7);  // get 7 bytes

  byte Second = bcd2dec(Wire.read() & 0b1111111);
  byte Minute = bcd2dec(Wire.read() & 0b1111111);
  byte Hour = Wire.read();
  bool h12 = Hour & 0b01000000;
  bool PM;
  if (h12) {  // 12 hour clock
    PM = Hour & 0b00100000;
    Hour = bcd2dec(Hour & 0b00011111);
  } else {  // 24 hour clock
    Hour = bcd2dec(Hour & 0b00111111);
  }
  byte WeekDay = bcd2dec(Wire.read());  // 1=Sunday
  byte Day = bcd2dec(Wire.read());
  byte Month = bcd2dec(Wire.read());
  byte Year = bcd2dec(Wire.read());
  
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

  Serial.print(days[WeekDay-1]);
  Serial.print("  ");
  Serial.print(Month);
  Serial.print("/");
  Serial.print(Day);
  Serial.print("/20");
  Serial.print(Year);
  Serial.print("  ");
  Serial.print(Hour);
  Serial.print(":");
  Serial.print(Minute);
  Serial.print(":");
  if (Second < 10) Serial.print("0");
  Serial.println(Second);
  if(Hour == OnHour && Minute == OnMin){
      digitalWrite(LED_BUILTIN,HIGH);
      Serial.println("LIGHT ON");
    }
    else if(Hour == OffHour && Minute == OffMin){
      digitalWrite(LED_BUILTIN,LOW);
      Serial.println("LIGHT OFF");
    } 
}
byte bcd2dec(byte val) {
  return ((val/16*10) + (val%16));
}

// Convert normal decimal number to binary coded decimal
byte dec2bcd(byte val) {
  return ((val/10*16) + (val%10));
}

void connectToNetwork() {
  // attempt to connect to Wifi network:
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    // failed, retry
    Serial.print(".");
    delay(5000);
  }

  Serial.println("You're connected to the network");
  Serial.println();
  lcd.clear();
  lcd.print("Connected");
}

void messageReceived(char* topic, byte* message, unsigned int length) {
  Serial.print("message received | ");
  Serial.print("topic = ");
  Serial.print(topic);
  Serial.print("| message length = ");
  Serial.println(length);

  String messageTemp;
 
  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.print("payload=");
  Serial.println(messageTemp);

  if (String(topic) == "arduino/lights") {
    if(messageTemp == "1"){        
      Serial.println("Lights ON");
      digitalWrite(lightsPin, HIGH);
    }
    else if(messageTemp == "0"){    
      Serial.println("Lights OFF");
      digitalWrite(lightsPin,LOW);
    }
  }
  if (String(topic) == "arduino/window") {
    if(messageTemp == "1"){        
      Serial.println("Window OPENED");
      myServo.attach(servoPin);
      for (pos = 0; pos < 90; pos += 1){ // goes from 0 degrees to 90 degrees // in steps of 1 degree
         myServo.write(pos); // tell servo to go to position in variable 'pos'
         delay(15); // waits 15ms for the servo to reach the position
       }
       myServo.detach();
      }
    else if(messageTemp == "0"){    
      Serial.println("Window CLOSED");
      myServo.attach(servoPin);
      for (pos = 90; pos>=1; pos-=1) {// goes from 90 degrees to 0 degrees
        myServo.write(pos); // tell servo to go to position in variable 'pos'
        delay(15); // waits 15ms for the servo to reach the position
      }
      myServo.detach();   
    }
  }
  if(String(topic) == "arduino/timer"){
    Serial.print(topic);
    Serial.print(" : ");
    Serial.println (messageTemp);
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, messageTemp);
    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    OnHour = doc["hourOn"];
    OnMin = doc["minuteOn"];
    OffHour = doc["hourOff"];
    OffMin = doc["minuteOff"];
  }
}

void connectToEmqx() {
  //connecting to a mqtt broker
  Serial.print("mqtt_host = ");
  Serial.println(mqtt_host);
  Serial.print("mqtt_port = ");
  Serial.println(String(mqtt_port));
  client.setServer(mqtt_host, mqtt_port);
 
  //client.setCallback(callback);
  if (!client.connected()) {
      String client_id = mqtt_clientid;
      client_id += String(WiFi.localIP());
      Serial.print("Connecting to the mqtt broker, client ID = ");
      Serial.println(client_id);
      lcd.clear();
      lcd.print("Connecting MQTT");
      if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
          Serial.println("Public emqx mqtt broker connected");
          lcd.clear();
          lcd.print("MQTT Connected");

          client.subscribe("arduino/lights");
          client.subscribe("arduino/window");
          client.subscribe("arduino/timer");
          client.setCallback(messageReceived);
          publish("arduino/status", 1);

      } else {
          Serial.print("failed with state ");
          Serial.println(client.state());
          lcd.clear();
          lcd.print("MQTT failed");
          delay(2000);
      }
  }
}
