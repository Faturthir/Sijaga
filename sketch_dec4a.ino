#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

const int lock = 21;
const int buzzer = 32;
const int LED_R = 12;
const int LED_G = 14;
const int button = 33;
const int pinGetar = 15;
const int pinBuzz = 32; 
#define TRIG_PIN 18
#define ECHO_PIN 5
#define VIBRATION_SENSOR 15 
#define ACCESS_DELAY 2000
#define DENIED_DELAY 1000
#define RST_PIN 4 
#define SS_PIN 2
#define WIFI_SSID "CPSRG"
#define WIFI_PASSWORD "CPSJAYA123"
MFRC522 mfrc522(SS_PIN, RST_PIN);

const char* gktep = "";
bool isFirstTap = true;        
bool refresh = false;
const char* tap = "KUNCI"; 

String API_URL = ""; //api url
String API_KEY = ""; //apikey
String TableName = ""; //table name
const int httpsPort = 443;

HTTPClient https;
WiFiClientSecure client;
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 
  client.setInsecure();
  Serial.begin(9600);
  pinMode(lock, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(pinGetar, INPUT);
  pinMode(pinBuzz, OUTPUT);
  digitalWrite(lock, HIGH);
  digitalWrite(LED_R, LOW);
  digitalWrite(LED_G, HIGH);
  noTone(buzzer);
  isFirstTap = true;
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.print("Connecting to ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  
}
String uidString = "";
String status = "";
void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW); 

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  int distance_cm = (duration / 2) / 29.1;

  Serial.print("Distance: ");
  Serial.print(distance_cm);
  Serial.println(" cm");
  delay(1000);

  if (distance_cm < 30) {
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_G, LOW);
    Serial.println("Ada");
    status = "ADA BARANG";
  }
  if (distance_cm > 30) {
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);
    Serial.println("Kosong");
    status = "TIDAK ADA BARANG";
  }

  if (digitalRead(button) == LOW) {
    refresh = true;
  }
  else if (refresh) {
      Serial.println("System refreshed");
      digitalWrite(lock, LOW);
      digitalWrite(pinGetar, LOW);
      Serial.print("Solenoid deactivated");
      delay(1000);
      ESP.restart();
      refresh = false;
    }

  long nilaigetar = pulseIn(pinGetar, HIGH);
  Serial.print("Nilai getar : ");
    Serial.print(nilaigetar);
  if (nilaigetar == 00) {
    Serial.println(" tidak ada getaran");
    digitalWrite(pinBuzz, LOW);
  } else if (nilaigetar > 5000) {
    Serial.println(" alat bergetar");
    digitalWrite(pinBuzz, HIGH);
  }
  delay(1000);
    if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print("UID tag :");
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  content.toUpperCase();

  uidString = content;

  
  bool authorized = false;
    if (uidString == "551E9552" || uidString == "1637C942" || uidString == "8518D952" || uidString == "351C6452" || uidString == "4AD7073C" || uidString == "458B2552" || uidString == "48EFC453" || uidString == "25FF6446" ) {
      authorized = true;

      if (uidString == "551E9552") uidString = "AWP 21";
      else if (uidString == "351C6452") uidString = "MFA 21";
      else if (uidString == "48EFC453") uidString = "ASF 21";
      else if (uidString == "458B2552") uidString = "OKA 21";
      else if (uidString == "1637C942") uidString = "RIA 21";
      else if (uidString == "8518D952") uidString = "RHN 21";
      else if (uidString == "4AD7073C") uidString = "ITF 20";
      else if (uidString == "25FF6446") uidString = "MFT 22";
    }
    
  
if (authorized) {
  if (isFirstTap) {
    Serial.println("Solenoid activated");
    digitalWrite(lock, LOW);
    digitalWrite(pinBuzz, LOW);
    isFirstTap = false;
  } else {
    Serial.println("Solenoid deactivated");
    digitalWrite(lock, HIGH);
    isFirstTap = true;
  }  
  tap = isFirstTap ? "KUNCI" : "BUKA";

  Serial.print("Authorized card detected: ");
  Serial.println(uidString);  // Menampilkan nama kartu yang terdaftar
} else {
  Serial.println("Access denied");
  digitalWrite(buzzer, HIGH); 
  delay(1000);
  digitalWrite(buzzer, LOW); 
}
delay(1000);

  //https.begin(client, API_URL + "/rest/v1/" + TableName);
  //https.addHeader("Content-Type", "application/json");
  //https.addHeader("Prefer", "return=representation");
  //https.addHeader("apikey", API_KEY);
  //https.addHeader("Authorization", "Bearer " + API_KEY);
  //int httpCode = https.POST("{\"uid\":\"" + uidString + "\",\"status\":\"" + status + "\",\"selenoid\":\"" + tap + "\"}");
  //String payload = https.getString(); 
  //Serial.println(httpCode);   
  //Serial.println(payload);    
  //https.end();

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("Authorized card detected: ");
  Serial.println(uidString);
} else {
  Serial.println("Access denied");
  digitalWrite(buzzer, HIGH); 
  delay(1000);
  digitalWrite(buzzer, LOW); 
}
delay(1000);
}