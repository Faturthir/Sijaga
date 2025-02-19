#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include <WiFiManager.h>

// Pin definitions
const int lock = 21;
const int buzzer = 32;
const int LED_R = 12;
const int LED_G = 14;
const int pinGetar = 15;
const int pinBuzz = 32;

#define RELAY_PIN 21  
#define TRIG_PIN 26
#define ECHO_PIN 25
#define RST_PIN 4
#define SS_PIN 5

// Deklarasi variabel sensor getar
unsigned long pulseDuration = 0;  // Durasi pulsa yang diterima dari sensor
int buzzerLevel = 0;

//Define database & endpoint
String API_URL = ""; //link dari api (url)
String PostUID = ""; //Endpoint Post UID
String PostLog = ""; //Endpoint post LogStatus
String endpointStatusBarang = ""; //endpoint availability status
String UsageHistory = ""; //endpoint usage history
const int httpsPort = 443;

//status solenoid
String solenoidStatus = "KUNCI";

// RFID setup
MFRC522 mfrc522(SS_PIN, RST_PIN);
String uid = "";
String status = "";
bool isFirstTap = true;
bool refresh = false;
String tap = "KUNCI";

// Cache untuk menyimpan UID yang sudah diotorisasi
String authorizedUID = "";

// Deklarasi Interrupt untuk sensor getar
volatile bool getaranTerdeteksi = false;

WiFiClientSecure client;

// Function declarations
void setupWiFi();
void ukurjarak();
void SensorGetar();
void ReadRFID();
void StatusBarang(String status);
void sendUidToDatabase(String uid);
void ControlSolenoid(String uid);
bool checkAuthorization(String uid);
void logSolenoidStatus(String uid, String time, String status);
void historypemakaian(String cardId, String status, String solenoidStatus);
String getFormattedTime();

// Definisikan fungsi handleGetar di sini sebelum setup() untuk menghindari prototipe terpisah
void IRAM_ATTR handleGetar() {
    getaranTerdeteksi = true;
}

void setup() {
    // Initialize serial communication
    Serial.begin(115200);

    // Initialize pins
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // Pastikan solenoid terkunci saat startup

    pinMode(lock, OUTPUT);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_R, OUTPUT);
    pinMode(LED_G, OUTPUT);
    pinMode(buzzer, OUTPUT);
    pinMode(pinGetar, INPUT);
    pinMode(pinBuzz, OUTPUT);
    digitalWrite(lock, HIGH);
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, HIGH);

    // Debugging status relay
    Serial.println("System initialized. Solenoid is locked.");

    // Connect to Wi-Fi
    setupWiFi();

    // Initialize SPI and RFID
    SPI.begin();
    mfrc522.PCD_Init();

    // Adjust antenna gain
    mfrc522.PCD_SetAntennaGain(MFRC522::RxGain_avg);

    if (!mfrc522.PCD_PerformSelfTest()) {
        Serial.println("RFID self-test failed.");
    } else {
        Serial.println("RFID initialized successfully.");
    }

    // Configure time
    configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

    // Interrupt setup
    attachInterrupt(digitalPinToInterrupt(pinGetar), handleGetar, RISING);
}

void loop() {
    Serial.println("Relay State: " + String(digitalRead(RELAY_PIN)));
    ReadRFID();
    ukurjarak();
    SensorGetar();
    
    // Reset pembaca RFID agar siap membaca kartu berikutnya
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    delay(100); // Reduced delay for faster response
}

void setupWiFi() {
    WiFiManager wifiManager;
    wifiManager.setDebugOutput(true);
    Serial.println("Starting WiFiManager...");

    if (!wifiManager.autoConnect("Sijaga", "sijaga123")) {
        Serial.println("Failed to connect, restarting...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("Wi-Fi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

void ukurjarak() {
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

    if (distance_cm < 55) {
        digitalWrite(LED_R, HIGH);
        digitalWrite(LED_G, LOW);
        status = "ADA BARANG";
    } else {
        digitalWrite(LED_R, LOW);
        digitalWrite(LED_G, HIGH);
        status = "TIDAK ADA BARANG";
    }

    Serial.println(status);
    Serial.println(" ");
    delay(500); // Reduced delay for faster response
}

void SensorGetar() {
    pulseDuration = pulseIn(pinGetar, HIGH);

    float membershipRendah = constrain(map(pulseDuration, 0, 900, 1, 0), 0, 1);
    float membershipSedang = constrain(map(pulseDuration, 900, 1250, 0, 1), 0, 1);
    float membershipTinggi = constrain(map(pulseDuration, 1250, 3000, 0, 1), 0, 1);

    Serial.print("Pulse Duration: ");
    Serial.print(pulseDuration);
    Serial.print(" | Rendah: ");
    Serial.print(membershipRendah);
    Serial.print(" | Sedang: ");
    Serial.print(membershipSedang);
    Serial.print(" | Tinggi: ");
    Serial.println(membershipTinggi);

    if (membershipTinggi > 0.5 || membershipSedang > 0.5) {
        buzzerLevel = 1;
    } else {
        buzzerLevel = 0;
    }

    if (buzzerLevel == 1) {
        Serial.println("Alat Bergetar (Buzzer Aktif)");
        unsigned long startTime = millis();
        while (millis() - startTime < 3000) {
            digitalWrite(pinBuzz, HIGH);
            delay(100);
            digitalWrite(pinBuzz, LOW);
            delay(100);
        }
    } else {
        digitalWrite(pinBuzz, LOW);
    }
}

void ReadRFID() {
    uid = "";

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        byte cardUID[4];
        for (byte i = 0; i < 4; i++) {
            cardUID[i] = mfrc522.uid.uidByte[i];
            uid += String(cardUID[i] < 0x10 ? "0" : "");
            uid += String(cardUID[i], HEX);
        }
        uid.toUpperCase();
        Serial.println("RFID Detected UID: " + uid);

        if (uid.length() == 0) {
            Serial.println("UID is empty, cannot proceed.");
            return;
        }

        ControlSolenoid(uid); // Langsung kontrol solenoid setelah UID terbaca
    }
}

void ControlSolenoid(String uid) {
    if (uid == authorizedUID || checkAuthorization(uid)) {
        Serial.println("UID Authorized: " + uid);
        authorizedUID = uid; // Simpan ke cache
        toggleSolenoid();
        sendUidToDatabase(uid);
        StatusBarang(status);
        historypemakaian(uid, status, solenoidStatus);
    } else {
        Serial.println("Access Denied: Unauthorized UID");
        digitalWrite(buzzer, HIGH);
        delay(1000);
        digitalWrite(buzzer, LOW);
        sendUidToDatabase(uid);
    }
}

void toggleSolenoid() {
    if (isFirstTap) {
        Serial.println("Unlocking solenoid...");
        digitalWrite(RELAY_PIN, LOW);
        tap = "UNLOCKED";
    } else {
        Serial.println("Locking solenoid...");
        digitalWrite(RELAY_PIN, HIGH);
        tap = "LOCKED";
    }
    solenoidStatus = tap;
    isFirstTap = !isFirstTap;
}

bool checkAuthorization(String uid) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected, cannot check authorization.");
        return false;
    }

    HTTPClient http;
    String query = API_URL + "/history/users?uid=eq." + uid + "&select=*";
    http.begin(query);
    http.setTimeout(2000); // Timeout 2 detik

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        String payload = http.getString();
        if (payload.indexOf(uid) > -1) {
            http.end();
            return true;
        }
    } else {
        Serial.print("Error on GET request. HTTP Response code: ");
        Serial.println(httpResponseCode);
    }

    http.end();
    return false;
}

void sendUidToDatabase(String uid) {
    if (WiFi.status() != WL_CONNECTED || uid.length() == 0) {
        Serial.println("Wi-Fi disconnected or UID is empty.");
        return;
    }

    HTTPClient http;
    String endpoint = "https://sijaga-railway-production.up.railway.app/card-id/create";
    http.begin(endpoint);
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"cardId\":\"" + uid + "\"}";
    Serial.println("Payload: " + payload);

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("POST Response:");
        Serial.println(response);
    } else {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

String getFormattedTime() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(timeStr);
}
