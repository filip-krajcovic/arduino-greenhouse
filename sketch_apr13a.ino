#include <DHT.h>
#include <LiquidCrystal.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "arduino_secrets.h"

// Set DHT pin:
#define DHTPIN 7

// Set DHT type, uncomment whatever type you're using!
// #define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)

// Initialize DHT sensor for normal 16mhz Arduino:
DHT dht = DHT(DHTPIN, DHTTYPE);

// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// sensitive data are stored in arduino_secrets.h
char ssid[] = SECRET_SSID;     // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

// akenza parameters
char mqtt_host[] = SECRET_MQTT_HOST;
//char mqtt_port[] = SECRET_MQTT_PORT;
const int mqtt_port = 1883; //= SECRET_MQTT_PORT;
char mqtt_clientid[] = SECRET_MQTT_CLIENT_ID;
char mqtt_outTopic[] = SECRET_MQTT_OUT_TOPIC;
char mqtt_username[] = SECRET_MQTT_USERNAME;
char mqtt_password[] = SECRET_MQTT_PASSWORD;

//mqtt_port = SECRET_MQTT_PORT - '0';

#define TIMEOUT 5000

WiFiClient wifiClient;
PubSubClient client(wifiClient);
//PubSubClient client(host, port, NULL, wifiClient);

void setup() {
  // Begin serial communication at a baud rate of 9600:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Setup sensor:
  dht.begin();
  
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Connecting to ");
  lcd.setCursor(0,1);
  lcd.print(ssid);
  
  connectToNetwork();
  delay(1000);
  connectToEmqx();
}

float lastH;
float lastT;

void loop() {
    // Wait a few seconds between measurements:
  delay(TIMEOUT);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

  // Read the humidity in %:
  float h = dht.readHumidity();
  // Read the temperature as Celsius:
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again):
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Compute heat index in Celsius:
  float hic = dht.computeHeatIndex(t, h, false);

  //calculate difference between current values and previous values
  float diffH = fabs(h - lastH);
  float diffT = fabs(t - lastT);

  if (diffH > 0.1 || diffT > 0.1) {
   
    //print to serial port
    print(t, h, hic);

    //display
    display(t, h, hic);
    
    //
    publish(t, h, hic);

    //store last values
    lastH = h;
    lastT = t;
    
    delay(5000);
    client.loop();
  }
}

void print(float t, float h, float hic) {
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print(" % ");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.print(" \xC2\xB0");
  Serial.print("C | ");
  Serial.print("Heat index: ");
  Serial.print(hic);
  Serial.print(" \xC2\xB0");
  Serial.print("C");
  Serial.print("\n");
}

void display(float t, float h, float hic) {
  // LCD display
  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print("Teplota:");
  lcd.setCursor(9,0);
  lcd.print(t);
  lcd.setCursor(14,0);
  lcd.print("\xB0");
  lcd.print("C");
  lcd.setCursor(0,1);
  lcd.print("Vlhkost:");
  lcd.setCursor(9,1);
  lcd.print(h);
  lcd.setCursor(15,1);
  lcd.print("%");
}

void publish(float t, float h, float hic) {
  StaticJsonDocument<256> doc;
  doc["temperature"] = t;
  doc["humidity"] = h;

  char out[128];
  int b =serializeJson(doc, out);
  Serial.print("publishing bytes = ");
  Serial.println(b,DEC);
  
  boolean rc = client.publish(mqtt_outTopic, out);
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
      } else {
          Serial.print("failed with state ");
          Serial.println(client.state());
          lcd.clear();
          lcd.print("MQTT failed");
          delay(2000);
      }
  }
}