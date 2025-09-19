#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SD.h>

// --- KREDENSIAL & KONFIGURASI ---
char ssid[] = "NAMA_WIFI_ANDA";
char pass[] = "PASSWORD_WIFI_ANDA";
String googleScriptURL = "URL_GOOGLE_APPS_SCRIPT_ANDA";

// --- KONFIGURASI PIN ---
#define SS_PIN    5  // RFID SDA/CS
#define RST_PIN   4  // RFID RST
#define SD_CS_PIN 15 // SD Card CS
#define BUZZER_PIN 13

// --- INISIALISASI OBJEK ---
MFRC522 mfrc522(SS_PIN, RST_PIN);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// --- DATABASE KARTU (CONTOH) ---
// Di dunia nyata, ini bisa disimpan di SD Card atau database
String knownUIDs[] = {"AB CD EF 12", "34 56 78 90"};
String knownNames[] = {"Budi", "Ani"};

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  pinMode(BUZZER_PIN, OUTPUT);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Connecting to WiFi...");
  display.display();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" WiFi Connected!");

  // Inisialisasi SD Card
  if(!SD.begin(SD_CS_PIN)){
    display.println("SD Card Error!");
    display.display();
  }

  displayMessage("Silakan Tap Kartu", 2000);
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String uid = getCardUID();
  int userIndex = findUser(uid);

  if (userIndex != -1) { // Kartu terdaftar
    String name = knownNames[userIndex];
    displayMessage("Selamat Datang,\n" + name, 2000);
    playTone(true);
    logAttendance(uid, name, "Hadir");
  } else { // Kartu tidak terdaftar
    displayMessage("Kartu Tidak\nTerdaftar!", 2000);
    playTone(false);
    logAttendance(uid, "Unknown", "Ditolak");
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1000);
  displayMessage("Silakan Tap Kartu", 0);
}

// --- FUNGSI-FUNGSI BANTUAN ---

String getCardUID() {
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
    content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  content.toUpperCase();
  return content.substring(1);
}

int findUser(String uid) {
  for (int i = 0; i < sizeof(knownUIDs) / sizeof(String); i++) {
    if (uid.equals(knownUIDs[i])) {
      return i;
    }
  }
  return -1;
}

void logAttendance(String uid, String name, String status) {
  String data = String(millis()) + "," + uid + "," + name + "," + status;
  
  // Log ke SD Card sebagai backup
  File dataFile = SD.open("/absensi.csv", FILE_APPEND);
  if (dataFile) {
    dataFile.println(data);
    dataFile.close();
    Serial.println("Logged to SD Card");
  }

  // Kirim ke Google Sheets jika WiFi terhubung
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = googleScriptURL + "?uid=" + uid + "&nama=" + name + "&status=" + status;
    http.begin(url.c_str());
    int httpCode = http.GET();
    if (httpCode > 0) Serial.println("Logged to Google Sheets");
    http.end();
  }
}

void displayMessage(String text, int delayTime) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.println(text);
  display.display();
  if (delayTime > 0) delay(delayTime);
}

void playTone(bool success) {
  if (success) {
    digitalWrite(BUZZER_PIN, HIGH); delay(100); digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW); delay(50);
    digitalWrite(BUZZER_PIN, HIGH); delay(50); digitalWrite(BUZZER_PIN, LOW);
  }
}
