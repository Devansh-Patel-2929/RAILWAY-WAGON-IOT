#include <HX711_ADC.h>
#include <DHT.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "XXXXXXXXX";
const char* password = "XXXXXXXXX";
const char* serverURL = "XXXXXXXX";

// Pins for Load Cell
const int HX711_dout = 4;
const int HX711_sck = 5;
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const float multiplier = 255;

// DHT11 Sensor
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Thermistor
const int thermistorPin = 34;

// Vibration Sensor
const int vibrationPin = 35;
const int vibrationThreshold = 1000;

// GPS Module
static const int RXPin = 16, TXPin = 17;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

// RFID
#define SS_PIN 21
#define RST_PIN 22
MFRC522 mfrc522(SS_PIN, RST_PIN);

float lastStableWeight = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Initialize sensors
  LoadCell.begin();
  LoadCell.start(2000, true);
  LoadCell.setCalFactor(multiplier);
  while (!LoadCell.update());

  dht.begin();
  gpsSerial.begin(9600, SERIAL_8N1, RXPin, TXPin);
  SPI.begin();
  mfrc522.PCD_Init();
  pinMode(vibrationPin, INPUT);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    DynamicJsonDocument doc(1024);
    JsonObject data = doc.to<JsonObject>();

    // Read Load Cell
    if (LoadCell.update()) {
      float sum = 0;
      const int numReadings = 10;
      for (int i = 0; i < numReadings; i++) {
        LoadCell.update();
        sum += LoadCell.getData();
        delay(10);
      }
      float avgWeight = sum / numReadings;
      data["weight"] = avgWeight * 4.2;
      lastStableWeight = avgWeight;
    }

    // Read DHT11
    data["dht_temperature"] = dht.readTemperature();
    data["dht_humidity"] = dht.readHumidity();

    // Read Thermistor
    float resistance = (10000.0 * (4095.0 / analogRead(thermistorPin) - 1.0));
    float steinhart = 1.0 / (0.001129148 + (0.000234125 * log(resistance / 10000.0)) - 273.15;
    data["thermistor_temp"] = steinhart;

    // Vibration Sensor
    int vibrationValue = analogRead(vibrationPin);
    data["vibration"] = vibrationValue;
    data["vibration_alert"] = vibrationValue > vibrationThreshold;

    // GPS Data
    bool newGPSData = false;
    while (gpsSerial.available() > 0) {
      if (gps.encode(gpsSerial.read())) newGPSData = true;
    }
    if (newGPSData && gps.location.isValid()) {
      data["latitude"] = gps.location.lat();
      data["longitude"] = gps.location.lng();
      data["satellites"] = gps.satellites.value();
    }

    // RFID
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String rfid = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        rfid += String(mfrc522.uid.uidByte[i], HEX);
      }
      data["rfid_uid"] = rfid;
      mfrc522.PICC_HaltA();
    } else {
      data["rfid_uid"] = "none";
    }

    // JSON to server
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
  else {
    Serial.println("WiFi Disconnected");
  }

  delay(5000); // Send data every 5 seconds
}