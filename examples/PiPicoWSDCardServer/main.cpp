#include <Arduino.h>
#include <WiFi.h>
#include <SdFat.h>
#include <SDServer.h>

#include <array>

const char* ssid = "wifi_ssid_here";
const char* password = "wifi_password_here";

SdFs sd;

WiFiServer server(80);

// Needs to be large enough to hold a current file path and
// a multipart/form-data part's headers at the same time.
// Experiment depending on your particular use case.
std::array<char, 512> workingBuffer;

// Buffer used when streaming uploaded files to the SD card.
// A larger buffer will upload files faster at the expense
// of increased memory usage.
std::array<char, 64> uploadStreamingBuffer;

SDServer sdServer;

void halt() {
  while (true);
}

void printWiFiStatus() {
  Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
  Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
}

void setup() {
  Serial.begin(115200);

  // Wait for USB serial
  while (!Serial) {
    yield();
  }

  int mhz = 50;
  bool sdInitialized = true;
  while (!sd.begin(SdSpiConfig(PIN_SPI0_SS, DEDICATED_SPI, SD_SCK_MHZ(mhz), &SPI))) { // MISO=GPIO16, CS=GPIO17, SCK=GPIO18, MOSI=GPIO19
    if (0 == --mhz) {
      Serial.println("SDFS::begin() failed");
      halt();
    }
  }

  Serial.printf("SPI MHz: %d\n", mhz);

  cid_t sdCID;
  if (sd.card()->readCID(&sdCID)) {
    Serial.printf("SD card serial: %d\n", sdCID.psn());
  } else {
    Serial.println("Failed to read CID");
    halt();
  }
  
  if (WL_NO_SHIELD == WiFi.status()) {
    Serial.println("No WiFi support found");
    halt();
  }

  int wifiStatus = WL_IDLE_STATUS;
  while (WL_CONNECTED != wifiStatus) {
    Serial.printf("Attempting to connect to WiFi network \"%s\"\n", ssid);
    wifiStatus = WiFi.begin(ssid, password);
    delay(10000); // retry every 10 seconds
  }

  server.begin();
  printWiFiStatus();

  sdServer.begin(
    &server,
    &sd,
    workingBuffer.begin(),
    workingBuffer.size(),
    uploadStreamingBuffer.begin(),
    uploadStreamingBuffer.size()
  );
}

void loop() {
  sdServer.handleClient();
}