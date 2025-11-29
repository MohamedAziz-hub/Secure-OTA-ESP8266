/*
 * ============================================
 * ESP8266 OTA Client - Mise à jour Over-The-Air (V3.0)
 * PROJET: AOUACHRI - VERSION PROMO 50% (PRIX 25 DT)
 * ============================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <Wire.h> // Ajout explicite pour I2C
#include <LiquidCrystal_I2C.h>

// ============================================
// CONFIGURATION WIFI
// ============================================
const char* ssid = "GLOBALNET_plus";
const char* password = "04071963";

// ============================================
// CONFIGURATION SERVEUR OTA
// ============================================
const char* serverIP = "192.168.1.14";
const int serverPort = 3000;
const bool useHTTPS = false;
const char* apiKey = "OTA_KEY_2025";

// ============================================
// CONFIGURATION FIRMWARE
// ============================================
const char* defaultVersion = "aouachri-3.0"; // ✅ Version 3 par défaut
const unsigned long checkInterval = 30000;

#define EEPROM_SIZE 512
#define VERSION_ADDRESS 0
#define VERSION_MAX_LENGTH 32
#define HASH_ADDRESS 64
#define HASH_MAX_LENGTH 64

// ============================================
// CONFIGURATION LCD 16x2 I2C — CORRIGÉE
// ============================================
#define LCD_I2C_ADDRESS 0x27  // ✅ Correction critique : 0x27 (pas 0x3F)
#define LCD_COLUMNS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// ============================================
// CONFIGURATION LED BLINK
// ============================================
#define LED_PIN LED_BUILTIN

enum SystemState { STATE_NORMAL_RUN, STATE_CHECKING_OTA, STATE_OTA_FAILED, STATE_WIFI_DISCONNECTED };
SystemState currentState = STATE_NORMAL_RUN;
unsigned long ledPreviousMillis = 0;

const long ledIntervals[] = {
    1500,  // STATE_NORMAL_RUN
    200,   // STATE_CHECKING_OTA
    50,    // STATE_OTA_FAILED
    1000   // STATE_WIFI_DISCONNECTED
};

// ============================================
// VARIABLES GLOBALES
// ============================================
unsigned long lastCheck = 0;
WiFiClient client;
WiFiClientSecure secureClient;
BearSSL::X509List cert_list;

String currentVersion = "";
String currentHash = "";
String pendingVersion = "";
String pendingHash = "";

// ============================================
// CERTIFICAT CA
// ============================================
const char* CA_CERT_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIEBTCCAu2gAwIBAgIUG1o7wMCU1APp4IeqcP/59f64jwUwDQYJKoZIhvcNAQEL
BQAwgZExCzAJBgNVBAYTAnRuMRAwDgYDVQQIDAd0dW5pc2lhMRAwDgYDVQQHDAdi
aXplcnRlMQ4wDAYDVQQKDAViaGlyYTEOMAwGA1UECwwFYmhpcmExDTALBgNVBAMM
BGF6aXoxLzAtBgkqhkiG9w0BCQEWIG1vaGFtZWRheml6LmFvdWFjaHJpQGZzYi51
Y2FyLnRuMB4XDTI1MTEyMjE5MjY1OFoXDTI2MTEyMjE5MjY1OFowgZExCzAJBgNV
BAYTAnRuMRAwDgYDVQQIDAd0dW5pc2lhMRAwDgYDVQQHDAdiaXplcnRlMQ4wDAYD
VQQKDAViaGlyYTEOMAwGA1UECwwFYmhpcmExDTALBgNVBAMMBGF6aXoxLzAtBgkq
hkiG9w0BCQEWIG1vaGFtZWRheml6LmFvdWFjaHJpQGZzYi51Y2FyLnRuMIIBIjAN
BgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA79O3OHz6mh1FOmdfE7IGD/93Rw8B
RuoHenrF6Ffxkd9H/9h95wIYGmXsz2TT90YRXax3q8nVClAY9pQjPhu6WaxB4BLK
kYeRXrVX2brvTfx7meUy7tfPH6ydRr08CRopy6YhmCmARHPMfxe5jv+R3JjxlcaJ
2C6syvB2Q66I2nzekmvHYPiIcriK4oBj5PKeq8RmnJyTJKWessVGYrQDcJOqDMdi
VgIIkpNxHtMmxk26kD02pNG0wAWT6QtmpDKnGWuyrveHeYoCZ6lSG9zJOiDTFxhj
hxEHuxohsbJTJGnjDvothqYIxJqDksZtAlNXnU1PmieFJN8GiXDT3DUQQQIDAQAB
o1MwUTAdBgNVHQ4EFgQU/duDeqIXec4xsWppho1wkSXpfsIwHwYDVR0jBBgwFoAU
/duDeqIXec4xsWppho1wkSXpfsIwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0B
AQsFAAOCAQEASfzBSwHpzb8v3sVFl2bX2BBtVz6GVfIKpH3p4wcCtMPwpuUSxwRA
w9sGh57hbsmdIdbp6MWYwQIqNqzhSYKvL/zaRbrYezLjnMGE4Gjw+vvEOC5GU+SA
q7JW8303lINvuUu4k9FNMLgUrfpXp2Wbcy92avEcDLgaqHdUXDUi16aDtZIqwQY4
NN1qILVN5SPVQqCpcF0fIkOxgie7BvZlY0cz04BSiHMpDP+ro5oYB0MC81tqKYI0
u7Gdo+wQI18tBx3KRyM6hnm5OPM7ywL7BkZbmdxWwnV1xfMtjQeK2buwcRaRehzz
4d8jzMm9rD+Ix66RJ17YGXUWJhjXEPLG1g==
-----END CERTIFICATE-----
)EOF";

// ============================================
// FONCTIONS UTILITAIRES
// ============================================
String readVersionFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    String version = "";
    for (int i = 0; i < VERSION_MAX_LENGTH; i++) {
        char c = EEPROM.read(VERSION_ADDRESS + i);
        if (c == '\0' || c == 255) break;
        version += c;
    }
    EEPROM.end();
    if (version.length() == 0) {
        version = String(defaultVersion);
        saveVersionToEEPROM(version);
    }
    return version;
}

void saveVersionToEEPROM(const String& version) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < VERSION_MAX_LENGTH; i++) {
        EEPROM.write(VERSION_ADDRESS + i, (i < version.length()) ? version.charAt(i) : '\0');
    }
    EEPROM.commit();
    EEPROM.end();
}

String readHashFromEEPROM() {
    EEPROM.begin(EEPROM_SIZE);
    String hash = "";
    for (int i = 0; i < HASH_MAX_LENGTH; i++) {
        char c = EEPROM.read(HASH_ADDRESS + i);
        if (c == '\0' || c == 255) break;
        hash += c;
    }
    EEPROM.end();
    return hash;
}

void saveHashToEEPROM(const String& hash) {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < HASH_MAX_LENGTH; i++) {
        EEPROM.write(HASH_ADDRESS + i, (i < hash.length()) ? hash.charAt(i) : '\0');
    }
    EEPROM.commit();
    EEPROM.end();
}

void logMessage(const String& message) {
    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.print("s] ");
    Serial.println(message);
}

void printWiFiInfo() {
    logMessage("=== Informations WiFi ===");
    logMessage("SSID: " + WiFi.SSID());
    logMessage("IP: " + WiFi.localIP().toString());
    logMessage("RSSI: " + String(WiFi.RSSI()) + " dBm");
    logMessage("MAC: " + WiFi.macAddress());
    logMessage("========================");
}

bool connectWiFi() {
    logMessage("Connexion au WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        logMessage("✓ WiFi connecté !");
        printWiFiInfo();
        return true;
    } else {
        logMessage("✗ Échec de la connexion WiFi");
        return false;
    }
}

void handleLedBlink() {
    if (currentState == STATE_CHECKING_OTA) return;
    unsigned long currentMillis = millis();
    if (currentMillis - ledPreviousMillis >= ledIntervals[currentState]) {
        ledPreviousMillis = currentMillis;
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
}

void updateLCDDisplay(const String& version) {
    lcd.clear();
    lcd.setCursor(0, 0);

    String versionNumber = version;
    int dashIndex = versionNumber.indexOf("-");
    if (dashIndex != -1) {
        versionNumber = versionNumber.substring(dashIndex + 1);
    }

    // ✅ AFFICHAGE POUR LA VERSION 3.0 : SOLDE 50% → PRIX 25 DT
    if (versionNumber.startsWith("3.") || versionNumber == "3.0") {
        lcd.setCursor(0, 0);
        lcd.print("SOLDE 50% !!!");
        lcd.setCursor(1, 1);
        lcd.print("PRIX 25 DT");
    }
    // Version 2.0 (optionnel, pour compatibilité)
    else if (versionNumber.startsWith("2.") || versionNumber == "2.0") {
        lcd.setCursor(0, 0);
        lcd.print("V2 EN ATTENTE");
        lcd.setCursor(0, 1);
        lcd.print("Redemarrage...");
    }
    // Version 1.0
    else if (versionNumber.startsWith("1.") || versionNumber == "1.0") {
        lcd.setCursor(2, 0);
        lcd.print("AOUACHRI V1.0");
        lcd.setCursor(3, 1);
        lcd.print("PRIX 100 DT");
    }
    // Version inconnue
    else {
        lcd.print("Version: ");
        lcd.print(versionNumber.substring(0, 14));
    }

    logMessage("LCD mis à jour pour version: " + version);
}

bool checkFirmwareUpdate();

bool performOTAUpdate(const char* firmwareURL, const String& newVersion, const String& newHash) {
    logMessage("📥 Téléchargement du firmware...");
    pendingVersion = newVersion;
    pendingHash = newHash;
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    ESPhttpUpdate.onStart([]() {
        logMessage("🔄 Début de la mise à jour OTA...");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Telechargement...");
        lcd.setCursor(0, 1);
        lcd.print("OTA en cours");
    });

    ESPhttpUpdate.onEnd([]() {
        logMessage("✓ OTA terminé !");
        saveVersionToEEPROM(pendingVersion);
        currentVersion = pendingVersion;
        if (pendingHash.length() > 0) {
            saveHashToEEPROM(pendingHash);
            currentHash = pendingHash;
        } else {
            logMessage("ℹ Aucun hash fourni.");
        }
        updateLCDDisplay(pendingVersion);
        delay(2000);
        logMessage("Redémarrage...");
    });

    ESPhttpUpdate.onProgress([](int current, int total) {
        int percent = (total > 0) ? (current * 100) / total : 0;
        static int lastPercent = -1;
        if (percent != lastPercent && (percent % 10 == 0 || percent == 100)) {
            lastPercent = percent;
            lcd.setCursor(0, 1);
            lcd.print("OTA: ");
            lcd.print(percent);
            lcd.print("%      ");
            Serial.printf("\r📥 Progression: %d%%", percent);
            yield(); // ✅ Evite les watchdog resets
        }
    });

    ESPhttpUpdate.onError([](int error) {
        logMessage("✗ Erreur OTA: " + String(error));
        currentState = STATE_OTA_FAILED;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Erreur OTA!");
        delay(3000);
        updateLCDDisplay(currentVersion);
        currentState = STATE_NORMAL_RUN;
    });

    String firmwareURLWithKey = String(firmwareURL);
    if (firmwareURLWithKey.indexOf("?") == -1) {
        firmwareURLWithKey += "?api_key=" + String(apiKey);
    } else {
        firmwareURLWithKey += "&api_key=" + String(apiKey);
    }

    t_httpUpdate_return ret;
    if (useHTTPS) {
        ret = ESPhttpUpdate.update(secureClient, firmwareURLWithKey.c_str(), "");
    } else {
        ret = ESPhttpUpdate.update(client, firmwareURLWithKey.c_str(), "");
    }

    switch (ret) {
        case HTTP_UPDATE_OK:
            logMessage("✓ Mise à jour réussie ! Redémarrage...");
            delay(1000);
            ESP.restart();
            return true;
        default:
            return false;
    }
}

bool checkFirmwareUpdate() {
    logMessage("Vérification de la disponibilité d'une mise à jour...");
    HTTPClient http;
    String protocol = useHTTPS ? "https" : "http";
    String url = protocol + "://" + String(serverIP) + ":" + String(serverPort) + "/firmware/latest";
    logMessage("URL: " + url);

    if (useHTTPS) {
        secureClient.setTimeout(10000);
        http.begin(secureClient, url);
    } else {
        http.begin(client, url);
    }

    http.addHeader("x-api-key", apiKey);
    http.setTimeout(10000);
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        logMessage("✗ Erreur HTTP: " + String(httpCode));
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();
    logMessage("Réponse serveur: " + payload);

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        logMessage("✗ Erreur JSON: " + String(error.c_str()));
        return false;
    }

    String serverVersionStr = doc["version"] ? String(doc["version"]) : "";
    String serverHash = doc["sha256"] ? String(doc["sha256"]) : "";
    logMessage("Version serveur: " + serverVersionStr);

    bool versionMatch = (serverVersionStr == currentVersion);
    bool hashMatch = (serverHash.length() > 0 && currentHash.length() > 0 && serverHash == currentHash);

    if (versionMatch && (!serverHash.length() || hashMatch)) {
        logMessage("✓ Firmware déjà à jour");
        updateLCDDisplay(currentVersion);
        return false;
    }

    if (hashMatch && !versionMatch) {
        logMessage("ℹ Hash identique, pas de flash.");
        updateLCDDisplay(currentVersion);
        return false;
    }

    logMessage("⚠ Mise à jour disponible !");
    return performOTAUpdate(doc["url"], serverVersionStr, serverHash);
}

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    // ✅ Initialisation explicite du bus I2C
    Wire.begin();
    lcd.init();
    lcd.backlight();

    currentVersion = readVersionFromEEPROM();
    currentHash = readHashFromEEPROM();

    if (useHTTPS) {
        if (String(CA_CERT_PEM).indexOf("BEGIN CERTIFICATE") != -1) {
            cert_list.append(CA_CERT_PEM);
            secureClient.setTrustAnchors(&cert_list);
            logMessage("✓ Certificat CA chargé.");
        } else {
            secureClient.setInsecure();
            logMessage("⚠ Connexion non sécurisée (setInsecure).");
        }
    }

    if (!connectWiFi()) {
        currentState = STATE_WIFI_DISCONNECTED;
        lcd.clear();
        lcd.print("WiFi Error!");
    } else {
        currentState = STATE_NORMAL_RUN;
    }

    updateLCDDisplay(currentVersion);
    lastCheck = millis() - checkInterval + 5000;
}

// ============================================
// LOOP
// ============================================
void loop() {
    handleLedBlink();

    if (WiFi.status() != WL_CONNECTED) {
        currentState = STATE_WIFI_DISCONNECTED;
        connectWiFi();
        if (WiFi.status() == WL_CONNECTED) {
            currentState = STATE_NORMAL_RUN;
        }
    }

    if (millis() - lastCheck >= checkInterval) {
        lastCheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            SystemState previousState = currentState;
            currentState = STATE_CHECKING_OTA;
            checkFirmwareUpdate();
            if (currentState == STATE_CHECKING_OTA) {
                currentState = previousState;
            }
        }
    }

    delay(10);
}