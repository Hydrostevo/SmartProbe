/*===================================================================================//
//  Source code: ChatGPT                                                             //
//  Azure code:  Lee Arbon                                                           //
//  Adapted for: Waveshare ESP32-S3-SIM7670G-4G                                      //
//  SD Card:     Added by Claude                                                     //
//===================================================================================//
//    <MODE>   | <Switch> |                  <Functions>                              |
//  Install    |   LOW    | Preview camera view, Camera settings,                     |
//             |          | WiFi settings [AP mode @ 192.168.1.1 or connected WiFi]   |
//  Timelapse  |   HIGH   | Image capture and send via SIM7670G every X minutes       |
//             |          | Images also saved to SD card with timestamp               |
//------------------------------------------------------------------------------------|
//  LED: fast blink- Install Mode, slow blink- OTA update, off- Timelapse mode        |
//===================================================================================*/

const char* stitle   = "Smart Probe Waveshare-Arbon";   // Sketch title
const char* sversion = "V1-5";                          // Sketch version

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>    // by ESP32Async
#include <AsyncTCP.h>             // by ESP32Async
#include <ESPmDNS.h>
#include <Update.h>
#include <base64.h>
#include <HardwareSerial.h>
#include <Adafruit_NeoPixel.h>    // For RGB LED
#include "time.h"
#include "FS.h"
#include "SD_MMC.h"
// Battery monitor
#include <Wire.h>
#include "Adafruit_MAX1704X.h"

// Include webpage headers
#include "main_page.h"
#include "settings_page.h"
#include "ota_page.h"
#include "sd_browser_page.h"

//=======================//
// Camera Model - OV2640 //
//=======================//
// Waveshare ESP32-S3-SIM7670G
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     34
#define SIOD_GPIO_NUM     15
#define SIOC_GPIO_NUM     16
#define Y9_GPIO_NUM       14
#define Y8_GPIO_NUM       13
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       11
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       9
#define Y3_GPIO_NUM       8
#define Y2_GPIO_NUM       7
#define VSYNC_GPIO_NUM    36
#define HREF_GPIO_NUM     35
#define PCLK_GPIO_NUM     37

//=======================//
// Hardware Pins         //
//=======================//
#define indicatorLED 2             // Built-in LED on Waveshare board
#define MODE_PIN 21                // Mode select switch (LOW = Setup mode)
#define SIM_PWR_PIN 41             // SIM7670G power control pin
#define SIM_RST_PIN 42             // SIM7670G reset pin
#define RGB_LED_PIN 38             // RGB LED (WS2812)
#define MAX17048_I2C_ADDRESS 0x36  // Battery monitor
#define I2C_SDA 8                  // I2C - SDA
#define I2C_SCL 9                  // I2C - SCL 

// SD Card pins for Waveshare ESP32-S3-SIM7670G-4G (actual hardware pins)
#define SDMMC_CLK   5   // CLK          ~ SD_SCK 
#define SDMMC_CMD   4   // CMD          ~ SD_MOSI
#define SDMMC_DATA  6   // DATA0        ~ SD_MISO
#define SD_CD_PIN   46  // Card Detect  ~ SD_CS (optional)

//  MAX17048G Battery monitor
Adafruit_MAX17048 maxlipo;
#define BATTERY_MAX_VOLTAGE 4.2  // LiPo/Li-ion fully charged
#define BATTERY_MIN_VOLTAGE 3.0  // LiPo/Li-ion minimum safe voltage
#define CHARGING_THRESHOLD 4.15  // Consider charging if voltage is rising above this

// Battery state tracking
struct BatteryState {
  float lastVoltage;
  unsigned long lastReadTime;
  bool initialized;
} batteryState = {0, 0, false};

// Battery data structure for readings
struct BatteryData {
  float voltage;
  float percentage;
  bool charging;
  bool valid;
};

const int MAX_RETRIES = 3;

bool installModeActive = false;
unsigned long lastBlink = 0;
bool ledState = false;
bool sdCardAvailable = false;

// RGB LED Breathing variables
unsigned long lastBreathUpdate = 0;
int breathBrightness = 0;
int breathDirection = 1;
const int breathSpeed = 5;

//=======================//
// System Time           //
//=======================//
time_t systemTime = 0;
struct tm timeinfo;
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

//=======================//
// AP Settings           //
//=======================//
const char* apSSID     = "SmartProbe3000";
const char* apPassword = "smartprobe";
const char* hostName   = "smartprobe";
IPAddress apIP(192,168,1,1);
IPAddress apGW(192,168,1,1);

//=======================//
// SIM7670G              //
//=======================//
HardwareSerial sim7670(1);
#define SIM_RX 18
#define SIM_TX 17

//=======================//
// Azure                 //
//=======================//
const char* serverURL = "*******************************";
const char* deviceId  = "*******************************";
const char* apiKey    = "*******************************";

//=======================//
// Preferences           //
//=======================//
Preferences preferences;
String ssidList[5], passList[5];
int storedInterval;
int camFrameSize, camQuality, camBrightness, camContrast;
bool showCrosshair = true;
bool saveToSD = true;  // Enable/disable SD card saving

//=======================//
// Server                //
//=======================//
AsyncWebServer server(80);

//=======================//
// RGB LED               //
//=======================//
Adafruit_NeoPixel rgbLED(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

//=======================//
// Function Prototypes   //
//=======================//
bool setupCamera();
long getSleepUS();
bool uploadToAzureSIM(const uint8_t* imageData, size_t imageSize);
bool uploadToAzureWiFi(const uint8_t* imageData, size_t imageSize);
void installMode();
void timelapseMode();
void blinkLED(int delayMs);
bool setupWiFi();
void applyCameraSettings();
void updateRGBBreathing(uint8_t r, uint8_t g, uint8_t b);
void setRGBColor(uint8_t r, uint8_t g, uint8_t b);
bool waitFor(const char* keyword, uint16_t timeout);
bool sendAT(const char* cmd, const char* expected, uint32_t timeout);
void setupSIM7670G();
bool initSDCard();
bool saveImageToSD(const uint8_t* imageData, size_t imageSize);
String getTimestamp();
BatteryData readBatteryData();
bool setupBatteryMonitor();
void syncTimeFromSIM();
void cleanupInstallMode();
bool readModePin();
void checkModeSwitch();

//========================//
// SD Card Functions      //
//========================//
bool initSDCard() {
  Serial.println("========================================");
      Serial.println("Initializing SD Card...");
  SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_DATA);
  
// Try 1-bit mode first (more compatible)
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("1-bit mode failed, trying 4-bit mode...");
    if (!SD_MMC.begin("/sdcard", false)) {
      Serial.println("Both modes failed, trying default pins...");
      SD_MMC.end();
      if (!SD_MMC.begin()) {
    Serial.println("✗ SD Card Mount Failed!");
    sdCardAvailable = false;
    return false;
        }
    }
  }
 
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("✗ No SD card attached or card type unknown");
    Serial.println("========================================");
    sdCardAvailable = false;
    SD_MMC.end();
    return false;
  }

  Serial.println("✓ SD Card mounted successfully!");
  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  Serial.printf("Total space: %lluMB\n", SD_MMC.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD_MMC.usedBytes() / (1024 * 1024));
  Serial.printf("Free space: %lluMB\n", (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024));

  // Create images directory if it doesn't exist
  if (!SD_MMC.exists("/images")) {
    if (SD_MMC.mkdir("/images")) {
      Serial.println("✓ Created /images directory");
    } else {
      Serial.println("⚠ Warning: Failed to create /images directory");
    }
  } else {
    Serial.println("✓ /images directory exists");
  }

  sdCardAvailable = true;
  Serial.println("✓ SD Card ready for use!");
  Serial.println("========================================");
  return true;
}

String getTimestamp() {
  // Try to get time from system
  if (getLocalTime(&timeinfo)) {
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y%m%d_%H%M%S", &timeinfo);
    return String(timeStr);
  }
  
  // Fallback to simple counter if time not available
  static int imageCounter = 0;
  imageCounter++;
  return String("img_") + String(imageCounter);
}

void syncTimeFromSIM() {
  // Get network time from SIM7670G
  sim7670.println("AT+CCLK?");
  delay(500);
  
  String response = "";
  while (sim7670.available()) {
    response += char(sim7670.read());
  }
  
  // Parse response: +CCLK: "24/01/28,12:34:56+00"
  if (response.indexOf("+CCLK:") > 0) {
    Serial.println("Time synced from SIM module: " + response);
  }
}

bool saveImageToSD(const uint8_t* imageData, size_t imageSize) {
  if (!sdCardAvailable) {
    Serial.println("SD card not available");
    return false;
  }

  String filename = "/images/" + getTimestamp() + ".jpg";
  Serial.printf("Saving image to SD: %s (%u bytes)\n", filename.c_str(), imageSize);

  File file = SD_MMC.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }

  size_t bytesWritten = file.write(imageData, imageSize);
  file.close();

  if (bytesWritten == imageSize) {
    Serial.printf("✓ Image saved successfully: %s\n", filename.c_str());
    
    // Update preferences with last saved image info
    preferences.putString("lastImage", filename);
    preferences.putInt("imageCount", preferences.getInt("imageCount", 0) + 1);
    
    return true;
  } else {
    Serial.printf("✗ Write error: only %u/%u bytes written\n", bytesWritten, imageSize);
    return false;
  }
}

//========================//
// RGB LED Functions      //
//========================//
void setRGBColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLED.setPixelColor(0, rgbLED.Color(r, g, b));
  rgbLED.show();
}

void updateRGBBreathing(uint8_t r, uint8_t g, uint8_t b) {
  if (millis() - lastBreathUpdate > 20) {
    lastBreathUpdate = millis();
    breathBrightness += breathDirection * breathSpeed;
    if (breathBrightness >= 255) {
      breathBrightness = 255;
      breathDirection = -1;
    } else if (breathBrightness <= 0) {
      breathBrightness = 0;
      breathDirection = 1;
    }
    uint8_t scaledR = (r * breathBrightness) / 255;
    uint8_t scaledG = (g * breathBrightness) / 255;
    uint8_t scaledB = (b * breathBrightness) / 255;
    setRGBColor(scaledR, scaledG, scaledB);
  }
}

//========================//
// SIM7670G Functions     //
//========================//
void setupSIM7670G() {
  Serial.println("Setting up SIM7670G...");
  sim7670.begin(115200, SERIAL_8N1, SIM_RX, SIM_TX);
  
  pinMode(SIM_PWR_PIN, OUTPUT);
  pinMode(SIM_RST_PIN, OUTPUT);
  
  digitalWrite(SIM_PWR_PIN, HIGH);
  digitalWrite(SIM_RST_PIN, HIGH);
  delay(1000);
  
  Serial.println("SIM7670G initialized");
}

bool waitFor(const char* keyword, uint16_t timeout) {
  unsigned long start = millis();
  String response = "";
  
  while (millis() - start < timeout) {
    if (sim7670.available()) {
      char c = sim7670.read();
      response += c;
      if (response.indexOf(keyword) >= 0) {
        return true;
      }
    }
  }
  return false;
}

bool sendAT(const char* cmd, const char* expected, uint32_t timeout) {
  sim7670.println(cmd);
  return waitFor(expected, timeout);
}

//========================//
// WiFi Functions         //
//========================//
bool setupWiFi() {
  Serial.println("Setting up WiFi...");
  
  for (int i = 0; i < 5; i++) {
    if (ssidList[i].length() == 0) continue;
    
    Serial.printf("Trying to connect to: %s\n", ssidList[i].c_str());
    WiFi.begin(ssidList[i].c_str(), passList[i].c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\nConnected to %s\n", ssidList[i].c_str());
      Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
      
      // Configure time from NTP
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      
      return true;
    }
    
    Serial.println("\nFailed to connect");
  }
  
  return false;
}

//========================//
// Camera Functions       //
//========================//
void applyCameraSettings() {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("Failed to get camera sensor");
    return;
  }
  
  s->set_framesize(s, (framesize_t)camFrameSize);
  s->set_quality(s, camQuality);
  s->set_brightness(s, camBrightness);
  s->set_contrast(s, camContrast);
  
  Serial.println("Camera settings applied");
}

bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("WARNING: No PSRAM found - using minimal settings");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  } else {
    Serial.printf("PSRAM found: %d bytes free\n", ESP.getFreePsram());
    config.fb_location = CAMERA_FB_IN_PSRAM;
    
    if (installModeActive) {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 15;
      config.fb_count = 2;
      Serial.println("Camera config: Install mode (SVGA, Q15, 2 buffers)");
    } else {
      config.frame_size = FRAMESIZE_UXGA;
      config.jpeg_quality = 10;
      config.fb_count = 1;
      Serial.println("Camera config: Timelapse mode (UXGA, Q10, 1 buffer)");
    }
  }

  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    
    if (!installModeActive && config.frame_size == FRAMESIZE_UXGA) {
      Serial.println("Retrying with SVGA...");
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      err = esp_camera_init(&config);
      
      if (err == ESP_OK) {
        Serial.println("Camera initialized with fallback settings");
        sensor_t *s = esp_camera_sensor_get();
        s->set_brightness(s, constrain(camBrightness, -2, 2));
        s->set_contrast(s, constrain(camContrast, -2, 2));
        return true;
      }
    }
    
    return false;
  }

  Serial.println("Camera initialized successfully");
  
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, constrain(camBrightness, -2, 2));
    s->set_contrast(s, constrain(camContrast, -2, 2));
    
    if (!installModeActive) {
      s->set_whitebal(s, 1);
      s->set_awb_gain(s, 1);
      s->set_exposure_ctrl(s, 1);
      s->set_aec2(s, 1);
      s->set_ae_level(s, 0);
      s->set_aec_value(s, 300);
      s->set_gain_ctrl(s, 1);
      s->set_agc_gain(s, 0);
      s->set_gainceiling(s, (gainceiling_t)0);
      s->set_bpc(s, 0);
      s->set_wpc(s, 1);
      s->set_raw_gma(s, 1);
      s->set_lenc(s, 1);
      s->set_hmirror(s, 0);
      s->set_vflip(s, 0);
      s->set_dcw(s, 1);
      s->set_colorbar(s, 0);
    }
  }
  
  return true;
}

//========================//
// Utility Functions      //
//========================//
void blinkLED(int delayMs){
  digitalWrite(indicatorLED, !digitalRead(indicatorLED));
  delay(delayMs);
}

long getSleepUS(){ 
  return (long)storedInterval * 60 * 1000000L; 
}

void cleanupInstallMode() {
  server.end();
  MDNS.end();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  esp_camera_deinit();
  delay(100);
}

bool readModePin() {
  int lowCount = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(MODE_PIN) == LOW) lowCount++;
    delay(10);
  }
  return (lowCount >= 3);
}

void checkModeSwitch() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();
    bool currentMode = readModePin();
    if (currentMode != installModeActive) {
      Serial.println("Mode switch detected - restarting...");
      cleanupInstallMode();
      ESP.restart();
    }
  }
}

// ====== SETUP BATTERY MONITOR ======
bool setupBatteryMonitor() {
  Serial.println("Initializing MAX17048 Battery Monitor...");
  
  // Initialize I2C with custom pins (if needed)
  // Wire.begin(I2C_SDA, I2C_SCL);
  
  // Try to initialize MAX17048
  if (!maxlipo.begin()) {
    Serial.println("ERROR: Could not find MAX17048!");
    Serial.println("Check:");
    Serial.println("  1. Battery is connected");
    Serial.println("  2. I2C wiring is correct");
    Serial.println("  3. Battery fuel gauge IC is functioning");
    return false;
  }
  
  Serial.print("Found MAX17048 with Chip ID: 0x");
  Serial.println(maxlipo.getChipID(), HEX);
  
  // Optional: Configure alert threshold (battery low warning)
  // maxlipo.setActivityThreshold(5.0);  // Alert at 5%
  
  batteryState.initialized = true;
  
  Serial.println("✓ MAX17048 Battery Monitor initialized successfully!");
  return true;
}

// ====== READ BATTERY DATA ======
BatteryData readBatteryData() {
  BatteryData data = {0, 0, false, false};
 
  if (!batteryState.initialized) {
    Serial.println("Battery monitor not initialized");
    return data;
  }
  
  data.voltage = maxlipo.cellVoltage();
  data.percentage = maxlipo.cellPercent();
  data.valid = true;
  
  // Simple charging detection based on voltage trend
  data.charging = (data.voltage > batteryState.lastVoltage + 0.05);
  batteryState.lastVoltage = data.voltage;
  
  return data;
}

// ====== BATTERY STATUS WEB HANDLER ======
void handleBatteryStatus(AsyncWebServerRequest *request) {
  BatteryData battery = readBatteryData();
  
  if (!battery.valid) {
    // Return error status
    String json = "{";
    json += "\"error\":\"Failed to read battery data\",";
    json += "\"voltage\":0,";
    json += "\"percentage\":0,";
    json += "\"charging\":false";
    json += "}";
    
    request->send(500, "application/json", json);
    return;
  }
  
  // Create JSON response
  String json = "{";
  json += "\"voltage\":" + String(battery.voltage, 2) + ",";
  json += "\"percentage\":" + String((int)battery.percentage) + ",";
  json += "\"charging\":" + String(battery.charging ? "true" : "false");
  json += "}";
  
  // Log to serial
  Serial.printf("Battery Status: %.2fV (%d%%) %s\n", 
                battery.voltage, 
                (int)battery.percentage, 
                battery.charging ? "[CHARGING]" : "");
  
  request->send(200, "application/json", json);
}

// ====== REGISTER BATTERY ENDPOINT ======
void registerBatteryEndpoint() {
  server.on("/battery_status", HTTP_GET, handleBatteryStatus);
  Serial.println("Battery status endpoint registered at /battery_status");
}

//=======================//
// Upload Functions      //
//=======================//
bool uploadToAzureSIM(const uint8_t* imgData, size_t len) {
  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    Serial.printf("[SIM7670G] Upload attempt %d/%d\n", attempt, MAX_RETRIES);

    if (!sendAT("AT+HTTPINIT", "OK", 2000)) {
      Serial.println("[SIM7670G] HTTPINIT failed");
      continue;
    }

    if (!sendAT("AT+HTTPPARA=\"CID\",1", "OK", 2000)) continue;

    String urlCmd = "AT+HTTPPARA=\"URL\",\"" + String(serverURL) + "\"";
    if (!sendAT(urlCmd.c_str(), "OK", 2000)) continue;

    if (!sendAT("AT+HTTPPARA=\"CONTENT\",\"image/jpeg\"", "OK", 2000)) continue;

    String headerCmd = "AT+HTTPPARA=\"USERDATA\",\"deviceId: " + String(deviceId) + 
                       "\\r\\nX-API-Key: " + String(apiKey) + "\"";
    if (!sendAT(headerCmd.c_str(), "OK", 2000)) continue;

    sim7670.printf("AT+HTTPDATA=%d,10000\r\n", len);
    if (!waitFor("DOWNLOAD", 5000)) {
      Serial.println("[SIM7670G] No DOWNLOAD prompt");
      sendAT("AT+HTTPTERM", "OK", 2000);
      continue;
    }

    sim7670.write(imgData, len);
    delay(1000);

    if (!sendAT("AT+HTTPACTION=1", "OK", 2000)) {
      Serial.println("[SIM7670G] HTTPACTION send failed");
      sendAT("AT+HTTPTERM", "OK", 2000);
      continue;
    }

    if (waitFor("+HTTPACTION: 1,200", 30000)) {
      Serial.println("[SIM7670G] Upload successful");
      sendAT("AT+HTTPTERM", "OK", 2000);
      return true;
    }

    sendAT("AT+HTTPTERM", "OK", 2000);
  }

  Serial.println("[SIM7670G] All attempts failed");
  return false;
}

bool uploadToAzureWiFi(const uint8_t* imageData, size_t imageSize) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Not connected");
    return false;
  }

  String credentials = String(deviceId) + ":" + String(apiKey);
  String encoded = base64::encode(credentials);

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    Serial.printf("[WiFi] Upload attempt %d/%d\n", attempt, MAX_RETRIES);
  
    HTTPClient http;
    http.begin(serverURL);
    
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("Authorization", "Basic " + encoded);

    int code = http.POST((uint8_t*)imageData, imageSize);
    Serial.printf("[WiFi] HTTP code: %d\n", code);

    String response = http.getString();
    Serial.println("[WiFi] Response: " + response);
    
    http.end();
    
    if (code == 200) { 
      Serial.println("WiFi upload successful!");
      return true;
    }
    
    delay(2000);
  }
  
  Serial.println("WiFi upload failed!");
  return false;
}

//=======================//
// Timelapse Mode        //
//=======================//
void timelapseMode() {
  Serial.println("=== TIMELAPSE MODE ===");
  setRGBColor(0, 128, 0);
  
  delay(500);
  
  if (saveToSD) {
    initSDCard();
  }
  
  syncTimeFromSIM();
  
  if (psramFound()) {
    Serial.printf("PSRAM available: %d bytes free\n", ESP.getFreePsram());
  } else {
    Serial.println("WARNING: No PSRAM detected!");
  }

  Serial.println("Initializing camera...");
  if (!setupCamera()) {
    Serial.println("ERROR: Camera initialization failed!");
    setRGBColor(255, 0, 0);
    delay(2000);
    setRGBColor(0, 0, 0);
    
    esp_sleep_enable_timer_wakeup(getSleepUS());
    esp_deep_sleep_start();
  }

  Serial.println("Camera warming up...");
  delay(2000);
  
  camera_fb_t *fb_discard = esp_camera_fb_get();
  if (fb_discard) {
    Serial.println("Discarding first frame");
    esp_camera_fb_return(fb_discard);
    delay(500);
  }

  Serial.println("Capturing image...");
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("ERROR: Image capture failed!");
    setRGBColor(255, 0, 0);
    delay(2000);
    
    esp_camera_deinit();
    setRGBColor(0, 0, 0);
    esp_sleep_enable_timer_wakeup(getSleepUS());
    esp_deep_sleep_start();
  }

  Serial.printf("Image captured: %u bytes, %dx%d\n", 
                fb->len, fb->width, fb->height);

  bool savedToSD = false;
  if (saveToSD && sdCardAvailable) {
    Serial.println("Saving to SD card...");
    savedToSD = saveImageToSD(fb->buf, fb->len);
    if (savedToSD) {
      Serial.println("✓ Image saved to SD card");
    } else {
      Serial.println("✗ Failed to save to SD card");
    }
  }

  bool uploaded = false;

  Serial.println("Attempting SIM7670G upload...");
  for (int i = 0; i < 3 && !uploaded; i++) {
    setRGBColor(0, 255, 0);
    delay(100);
    setRGBColor(0, 64, 0);
    delay(100);
    
    if (uploadToAzureSIM(fb->buf, fb->len)) {
      uploaded = true;
      Serial.println("SIM7670G upload successful!");
      for (int j = 0; j < 3; j++) {
        setRGBColor(0, 255, 0);
        delay(200);
        setRGBColor(0, 0, 0);
        delay(200);
      }
    } else {
      Serial.printf("SIM7670G upload failed (attempt %d/3)\n", i + 1);
      delay(3000);
    }
  }

  if (!uploaded) {
    Serial.println("SIM7670G failed, trying WiFi...");
    if (setupWiFi()) {
      for (int i = 0; i < 3 && !uploaded; i++) {
        setRGBColor(0, 255, 255);
        delay(100);
        setRGBColor(0, 64, 64);
        delay(100);
        
        if (uploadToAzureWiFi(fb->buf, fb->len)) {
          uploaded = true;
          Serial.println("WiFi upload successful!");
          for (int j = 0; j < 3; j++) {
            setRGBColor(0, 255, 255);
            delay(200);
            setRGBColor(0, 0, 0);
            delay(200);
          }
        } else {
          Serial.printf("WiFi upload failed (attempt %d/3)\n", i + 1);
          delay(3000);
        }
      }
    } else {
      Serial.println("WiFi unavailable.");
    }
  }

  esp_camera_fb_return(fb);

  if (!uploaded) {
    for (int j = 0; j < 3; j++) {
      setRGBColor(255, 0, 0);
      delay(300);
      setRGBColor(0, 0, 0);
      delay(300);
    }
  }

  Serial.println("=== Summary ===");
  Serial.printf("SD Card Save: %s\n", savedToSD ? "SUCCESS" : "FAILED");
  Serial.printf("Cloud Upload: %s\n", uploaded ? "SUCCESS" : "FAILED");
  
  if (savedToSD && !uploaded) {
    Serial.println("Image backed up on SD card - will retry upload on next cycle");
  }

  esp_camera_deinit();
  setRGBColor(0, 0, 0);
  
  esp_sleep_enable_timer_wakeup(getSleepUS());
  esp_sleep_enable_ext0_wakeup((gpio_num_t)MODE_PIN, 0);

  Serial.println("Entering deep sleep...");
  Serial.flush();
  delay(100);
  esp_deep_sleep_start();
}

//=======================//
// Install Mode          //
//=======================//
void installMode() {
  installModeActive = true;
  Serial.println("=== INSTALL MODE ===");
  
  initSDCard();
  
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_AP);
  delay(200);
  WiFi.softAPdisconnect(true);
  delay(200);

  if (!WiFi.softAPConfig(apIP, apGW, IPAddress(255,255,255,0))) {
    Serial.println("AP config failed!");
  } else {
    Serial.println("AP configured successfully");
  }
  
  if (!WiFi.softAP(apSSID, apPassword)) {
    Serial.println("Failed to start Access Point!");
  } else {
    Serial.printf("Access Point started: %s\n", WiFi.softAPIP().toString().c_str());
  }
  
  WiFi.mode(WIFI_AP_STA);
  delay(500);
  setupWiFi();

  Serial.print("Install mode active, connect to: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.print(apSSID);
  Serial.print(", Password: ");
  Serial.println(apPassword);

  if (!setupCamera()) {
    Serial.println("Camera init failed in install mode");
  } else {
    applyCameraSettings();
  }

  if (!MDNS.begin("smartprobe")) {
    Serial.println("mDNS setup failed");
  }

  // Root page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = FPSTR(ROOT_HTML);
    html.replace("{{title}}", String(stitle) + " " + String(sversion));
    String staIP = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Not connected";
    String apIP = WiFi.softAPIP().toString();
    
    String sdInfo = "";
    if (sdCardAvailable) {
      int imageCount = preferences.getInt("imageCount", 0);
      uint64_t freeSpace = (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024);
      sdInfo = "<br><b>SD Card:</b> Available (" + String(imageCount) + " images, " + 
               String(freeSpace) + "MB free)";
    } else {
      sdInfo = "<br><b>SD Card:</b> Not available";
    }
    
    String ipInfo = "<div style='margin-top:10px; padding:10px; background:#eee; border-radius:8px;'>"
                    "<b>Access Point IP:</b> " + apIP + "<br>"
                    "<b>Station IP:</b> " + staIP + sdInfo + "</div>";
    html.replace("{{ip_info}}", ipInfo);
    request->send(200, "text/html", html);
  });

  // Settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = FPSTR(SETTINGS_HTML);
    html.replace("{{title}}", String(stitle) + " " + String(sversion));
    request->send(200, "text/html", html);
  });

  // SD Card Gallery page
  server.on("/gallery", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", FPSTR(SD_BROWSER_HTML));
  });

  // SD Card API: List files
  server.on("/sd_list", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"files\":[";
    if (sdCardAvailable) {
      File root = SD_MMC.open("/images");
      if (root) {
        File file = root.openNextFile();
        bool first = true;
        while (file) {
          if (!file.isDirectory()) {
            String filename = String(file.name());
            // Only include image files
            if (filename.endsWith(".jpg") || filename.endsWith(".jpeg") || 
                filename.endsWith(".JPG") || filename.endsWith(".JPEG")) {
              if (!first) json += ",";
              first = false;
              json += "{\"name\":\"" + filename + "\",\"size\":" + String(file.size()) + "}";
            }
          }
          file = root.openNextFile();
        }
        root.close();
      }
      json += "],\"freeMB\":" + String((SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024));
    } else {
      json += "],\"freeMB\":0";
    }
    json += "}";
    request->send(200, "application/json", json);
  });

  // SD Card API: Serve image
  server.on("/sd_image", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
      request->send(404, "text/plain", "SD card not available");
      return;
    }
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String filename = "/images/" + request->getParam("file")->value();
    
    if (!SD_MMC.exists(filename)) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    
    request->send(SD_MMC, filename, "image/jpeg");
  });

  // SD Card API: Download image
  server.on("/sd_download", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
      request->send(404, "text/plain", "SD card not available");
      return;
    }
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }
    String filename = "/images/" + request->getParam("file")->value();
    
    if (!SD_MMC.exists(filename)) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    
    AsyncWebServerResponse *response = request->beginResponse(SD_MMC, filename, "image/jpeg", true);
    response->addHeader("Content-Disposition", "attachment; filename=" + request->getParam("file")->value());
    request->send(response);
  });

  // SD Card API: Delete single file
  server.on("/sd_delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
      return;
    }
    if (!request->hasParam("file")) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"Missing file parameter\"}");
      return;
    }
    
    String filename = "/images/" + request->getParam("file")->value();
    
    if (SD_MMC.remove(filename)) {
      // Update image count
      int count = preferences.getInt("imageCount", 0);
      if (count > 0) {
        preferences.putInt("imageCount", count - 1);
      }
      request->send(200, "application/json", "{\"success\":true}");
      Serial.println("Deleted: " + filename);
    } else {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"Failed to delete file\"}");
    }
  });

  // SD Card API: Delete all files
  server.on("/sd_delete_all", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!sdCardAvailable) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
      return;
    }
    
    int deletedCount = 0;
    File root = SD_MMC.open("/images");
    if (root) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String filename = String(file.name());
          String fullPath = "/images/" + filename;
          file.close();
          if (SD_MMC.remove(fullPath)) {
            deletedCount++;
          }
          file = root.openNextFile();
        } else {
          file = root.openNextFile();
        }
      }
      root.close();
    }
    
    // Reset image count
    preferences.putInt("imageCount", 0);
    
    String json = "{\"success\":true,\"deleted\":" + String(deletedCount) + "}";
    request->send(200, "application/json", json);
    Serial.printf("Deleted %d files from SD card\n", deletedCount);
  });

  // Serve still image
  auto handleStill = [](AsyncWebServerRequest *request) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      request->send(500, "text/plain", "Capture failed");
      return;
    }
    
    if (saveToSD && sdCardAvailable && request->hasParam("save")) {
      saveImageToSD(fb->buf, fb->len);
    }
    
    request->send_P(200, "image/jpeg", fb->buf, fb->len);
    esp_camera_fb_return(fb);
  };
  server.on("/still", HTTP_GET, handleStill);
  server.on("/stream", HTTP_GET, handleStill);
  server.on("/capture", HTTP_GET, handleStill);

  // Camera settings
  auto handleSaveCam = [](AsyncWebServerRequest *request) {
    if (request->hasParam("framesize", true)) camFrameSize = request->getParam("framesize", true)->value().toInt();
    if (request->hasParam("quality", true)) camQuality = request->getParam("quality", true)->value().toInt();
    if (request->hasParam("brightness", true)) camBrightness = request->getParam("brightness", true)->value().toInt();
    if (request->hasParam("contrast", true)) camContrast = request->getParam("contrast", true)->value().toInt();
    preferences.putInt("framesize", camFrameSize);
    preferences.putInt("quality", camQuality);
    preferences.putInt("brightness", camBrightness);
    preferences.putInt("contrast", camContrast);
    applyCameraSettings();
    request->send(200, "text/plain", "OK");
  };
  server.on("/save_camera", HTTP_POST, handleSaveCam);
  server.on("/save-camera-settings", HTTP_POST, handleSaveCam);

  // Crosshair toggle
  server.on("/set_crosshair", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("state")) {
      showCrosshair = request->getParam("state")->value().toInt();
      preferences.putBool("crosshair", showCrosshair);
    }
    request->send(200, "text/plain", "OK");
  });

  // SD card enable/disable
  server.on("/set_sd", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("enabled", true)) {
      saveToSD = request->getParam("enabled", true)->value().toInt();
      preferences.putBool("saveSD", saveToSD);
      Serial.printf("SD card saving %s\n", saveToSD ? "enabled" : "disabled");
    }
    request->send(200, "text/plain", "OK");
  });

  // Get SD card status
  server.on("/sd_status", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"available\":" + String(sdCardAvailable ? "true" : "false") + ",";
    json += "\"enabled\":" + String(saveToSD ? "true" : "false") + ",";
    json += "\"imageCount\":" + String(preferences.getInt("imageCount", 0)) + ",";
    if (sdCardAvailable) {
      json += "\"totalMB\":" + String(SD_MMC.totalBytes() / (1024 * 1024)) + ",";
      json += "\"usedMB\":" + String(SD_MMC.usedBytes() / (1024 * 1024)) + ",";
      json += "\"freeMB\":" + String((SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024));
    } else {
      json += "\"totalMB\":0,\"usedMB\":0,\"freeMB\":0";
    }
    json += "}";
    request->send(200, "application/json", json);
  });

  // SD Browser page
  server.on("/sd_browser", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", FPSTR(SD_BROWSER_HTML));
  });

  // List files on SD card
  server.on("/sd_list", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!sdCardAvailable) {
      request->send(200, "application/json", "{\"files\":[]}");
      return;
    }

    File root = SD_MMC.open("/images");
    if (!root || !root.isDirectory()) {
      request->send(200, "application/json", "{\"files\":[]}");
      return;
    }

    String json = "{\"files\":[";
    bool first = true;
    
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String filename = String(file.name());
        // Only include .jpg files
        if (filename.endsWith(".jpg") || filename.endsWith(".JPG")) {
          if (!first) json += ",";
          first = false;
          
          json += "{";
          json += "\"name\":\"" + filename + "\",";
          json += "\"size\":" + String(file.size()) + ",";
          json += "\"date\":\"" + String(file.getLastWrite()) + "\"";
          json += "}";
        }
      }
      file = root.openNextFile();
    }
    
    json += "]}";
    request->send(200, "application/json", json);
  });

  // Download file from SD card
  server.on("/sd_download", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!sdCardAvailable) {
      request->send(404, "text/plain", "SD card not available");
      return;
    }

    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }

    String filename = request->getParam("file")->value();
    String filepath = "/images/" + filename;
    
    // Security: prevent directory traversal
    if (filename.indexOf("..") >= 0 || filename.indexOf("/") >= 0) {
      request->send(403, "text/plain", "Invalid filename");
      return;
    }

    if (!SD_MMC.exists(filepath)) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    File file = SD_MMC.open(filepath, FILE_READ);
    if (!file) {
      request->send(500, "text/plain", "Failed to open file");
      return;
    }

    request->send(SD_MMC, filepath, "image/jpeg", true);
    file.close();
  });

  // Delete file from SD card
  server.on("/sd_delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!sdCardAvailable) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"SD card not available\"}");
      return;
    }

    if (!request->hasParam("file", true)) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"Missing file parameter\"}");
      return;
    }

    String filename = request->getParam("file", true)->value();
    String filepath = "/images/" + filename;
    
    // Security: prevent directory traversal
    if (filename.indexOf("..") >= 0 || filename.indexOf("/") >= 0) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"Invalid filename\"}");
      return;
    }

    if (!SD_MMC.exists(filepath)) {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"File not found\"}");
      return;
    }

    if (SD_MMC.remove(filepath)) {
      // Update image count
      int currentCount = preferences.getInt("imageCount", 0);
      if (currentCount > 0) {
        preferences.putInt("imageCount", currentCount - 1);
      }
      Serial.printf("Deleted file: %s\n", filepath.c_str());
      request->send(200, "application/json", "{\"success\":true}");
    } else {
      request->send(200, "application/json", "{\"success\":false,\"error\":\"Failed to delete file\"}");
    }
  });

  // Timelapse interval
  server.on("/set_interval", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("interval", true)) {
      storedInterval = request->getParam("interval", true)->value().toInt();
      if (storedInterval < 1) storedInterval = 1;
      preferences.putInt("interval", storedInterval);
    }
    request->send(200, "text/plain", "OK");
  });

  // WiFi handlers (simplified - add your existing handlers here)
  server.on("/wifi_add", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String pass = request->getParam("password", true)->value();
      for (int i = 0; i < 5; i++) {
        if (ssidList[i] == "" || ssidList[i] == ssid) {
          ssidList[i] = ssid;
          passList[i] = pass;
          preferences.putString(("ssid" + String(i)).c_str(), ssid);
          preferences.putString(("pass" + String(i)).c_str(), pass);
          break;
        }
      }
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/wifi_scan", HTTP_GET, [](AsyncWebServerRequest *request){
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n && i < 5; i++) {
      if (i) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
              ",\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  // OTA
  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", FPSTR(UPDATE_FORM_HTML));
  });

  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request){
      bool ok = !Update.hasError();
      request->send(200, "text/plain", ok ? "OK" : "FAIL");
      if (ok) {
        delay(200);
        ESP.restart();
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!index) {
        Serial.printf("OTA Start: %s\n", filename.c_str());
        if (!Update.begin()) Update.printError(Serial);
      }
      if (Update.write(data, len) != len) Update.printError(Serial);
      if (final) {
        if (Update.end(true)) Serial.printf("OTA Success: %u bytes\n", index + len);
        else Update.printError(Serial);
      }
    }
  );

  server.begin();
  Serial.println("HTTP server started");
}

//======================//
// Setup                //
//======================//
void setup() {
  Serial.begin(115200);
  delay(1000);

  rgbLED.begin();
  rgbLED.setBrightness(255);
  setRGBColor(0, 0, 0);

  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
  Serial.println(String("Wakeup reason: ") + (int)wakeReason);

  setupSIM7670G();
  
  pinMode(indicatorLED, OUTPUT);
  pinMode(MODE_PIN, INPUT_PULLUP);
  digitalWrite(indicatorLED, LOW);

  Serial.printf("MODE_PIN (GPIO%d) state: %s\n", MODE_PIN, digitalRead(MODE_PIN) == LOW ? "LOW" : "HIGH");
  
  preferences.begin("smartprobe3000", false);
  camFrameSize = preferences.getInt("framesize", 5);
  camQuality = preferences.getInt("quality", 12);
  camBrightness = preferences.getInt("brightness", 0);
  camContrast = preferences.getInt("contrast", 0);
  showCrosshair = preferences.getBool("crosshair", true);
  storedInterval = preferences.getInt("interval", 3);
  saveToSD = preferences.getBool("saveSD", true);

  for(int i = 0; i < 5; i++){
    String ssidKey = "ssid" + String(i);
    ssidList[i] = preferences.getString(ssidKey.c_str(), "");
    passList[i] = preferences.getString(("pass" + String(i)).c_str(), "");
  }

 // Initialize I2C (if not already done)
  Wire.begin();  // Uses default pins
  // Or specify custom pins:
  // Wire.begin(I2C_SDA, I2C_SCL);
  
  // Initialize battery monitoring
  if (setupBatteryMonitor()) {
    Serial.println("Battery monitor ready!");
    
    // Print initial reading
    BatteryData battery = readBatteryData();
    if (battery.valid) {
      Serial.printf("Initial Battery: %.2fV (%d%%)\n", 
                    battery.voltage, (int)battery.percentage);
    }
  } else {
    Serial.println("WARNING: Battery monitor not available");
  }
  
  // ... setup web server ...
  server.begin();
  
  // Register battery endpoint
  registerBatteryEndpoint();

  
  delay(100);

  if (readModePin()) {
    Serial.println("MODE_PIN LOW → Starting Install Mode");
    installMode();
    return;
  } else {
    Serial.println("MODE_PIN HIGH → Starting Timelapse Mode");
    timelapseMode();
    return;
  }
}

//========================//
//      LOOP              //
//========================//
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.equalsIgnoreCase("I") || cmd.equalsIgnoreCase("INSTALL")) {
      Serial.println("Serial override: Starting Install Mode...");
      installMode();
      return;
    }
    else if (cmd.equalsIgnoreCase("T") || cmd.equalsIgnoreCase("TIMELAPSE")) {
      Serial.println("Serial override: Starting Timelapse Mode...");
      timelapseMode();
      return;
    }
  }
  
  if (installModeActive) {
    updateRGBBreathing(204, 32, 240);
    checkModeSwitch();
    delay(10);
  }
}
