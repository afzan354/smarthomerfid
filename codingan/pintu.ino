#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <MFRC522.h>
#include <SPI.h>

#define BUZZER D3   // Buzzer pin D3
#define RELAY_PIN D4 // Relay pin D4
#define SS_PIN D2    // D2
#define RST_PIN D1   // D1

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

ESP8266WiFiMulti WiFiMulti;
bool debugMode = false; // Tandai apakah kita dalam mode debug atau tidak

String lastPayload = ""; // Variabel untuk menyimpan nilai terakhir dari URL

void readRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String ID_TAG = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    ID_TAG += String(mfrc522.uid.uidByte[i], HEX);
  }

  Serial.print("ID : ");
  Serial.println(ID_TAG);

  // Ganti dengan UID kartu RFID yang diizinkan
  String authorizedCardUIDs[] = {"93df9ba9", "933e2eab", "1391ae"};
  bool accessGranted = false;

  for (int i = 0; i < sizeof(authorizedCardUIDs) / sizeof(authorizedCardUIDs[0]); i++) {
    if (ID_TAG == authorizedCardUIDs[i]) {
      accessGranted = true;
      break;
    }
  }

  if (accessGranted) {
    digitalWrite(BUZZER, HIGH);
    delay(500);
    digitalWrite(BUZZER, LOW);

    // Membuka pintu
    digitalWrite(RELAY_PIN, LOW);
    delay(1000); // Tahan pintu terbuka selama 1 detik
    digitalWrite(RELAY_PIN, HIGH); // Kunci pintu kembali
  } else {
    // Kartu RFID tidak diizinkan
    Serial.println("Access denied.");
    digitalWrite(BUZZER, HIGH);
    delay(1000);
    digitalWrite(BUZZER, LOW);
  }

  Serial.println("Tap kartu RFID ...");
  delay(2000);
}

void controlAccess(bool granted) {
  if (granted) {
    Serial.println("Access granted. Opening door...");

    // Membuka pintu
    digitalWrite(RELAY_PIN, LOW);
    delay(1000); // Tahan pintu terbuka selama 1 detik
    digitalWrite(RELAY_PIN, HIGH); // Kunci pintu kembali
  } else {
    // Kartu RFID tidak diizinkan
    Serial.println("Access denied.");
    digitalWrite(BUZZER, HIGH);
    delay(1000);
    digitalWrite(BUZZER, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Kunci pintu awalnya terkunci
  delay(200);
  SPI.begin();
  mfrc522.PCD_Init();
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("iphone", "123iphone"); // Ganti dengan SSID dan password WiFi Anda
  Serial.println("Connecting to WiFi...");
}

void loop() {
  if ((WiFiMulti.run() == WL_CONNECTED)) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    HTTPClient https;

    if (debugMode) {
      Serial.println("[HTTPS] begin...");
      Serial.println("[HTTPS] GET...");
    }

    // Update URL
    const char *url = "https://arduino-b306c-default-rtdb.asia-southeast1.firebasedatabase.app/shome/relay2.json";

    if (https.begin(*client, url)) {  // HTTPS
      int httpCode = https.GET();

      if (httpCode > 0) {
        if (debugMode) {
          Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        }

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();

          // Hilangkan tanda petik ganda di awal dan akhir String
          payload.remove(0, 1);
          payload.remove(payload.length() - 1);

          // Periksa apakah nilai payload berubah sejak data terakhir
          if (payload != lastPayload) {
            lastPayload = payload;

            // Tampilkan teks hanya jika ada perubahan pada URL
            Serial.println(payload);

            // Periksa nilai payload
            if (payload == "on") {
              controlAccess(true); // Buka pintu
            } else if (payload == "off") {
              controlAccess(false); // Tutup pintu
            }
          }
        }
      } else {
        if (debugMode) {
          Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
        }
      }

      https.end();
    } else {
      if (debugMode) {
        Serial.printf("[HTTPS] Unable to connect\n");
      }
    }

    // Tunggu sebelum pengambilan data berikutnya
    delay(1000);

    // Membaca kartu RFID setelah menunggu
    readRFID();
  }
}
