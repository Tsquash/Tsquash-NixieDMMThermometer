#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include "X9C10X.h"

// Constants
#define EEPROM_SIZE 512
#define OFFSET 15
const char *PLACEHOLDER_API_KEY = "/*YourKeyHere*/";
const char* SSID = "/*YourSSIDHere*/";
const char* PASSWORD = "/*YourPasswordHere*/";
String location = "/*YourCityHere*/";

// Global Variables
X9C10X pot(100000);
unsigned int consecutiveFailures = 0;
uint8_t currentPosition = 0; // Current position of the potentiometer
int tempCache = 0;

// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000); // UTC, updates every 60 seconds

void connectToWiFi();
int fetchTemperature();
void initDigipot();
void updateDigipot(int temperature);
bool isDeadHours();
void rollDigits();

void setup() {
  pinMode(2, OUTPUT); // INC
  pinMode(1, OUTPUT); // U/D
  pinMode(7, OUTPUT); // CS

  Serial.begin(115200);

  // Initialize Pot & Set to 0
  pot.begin(2,1,7);

  //ensures it is at 0
  updateDigipot(117);
  updateDigipot(15);
  pot.setPosition(0);

  Serial.printf("SSID:%s\nPassword:%s\nLocation:%s\n",SSID,PASSWORD,location);
  connectToWiFi();
}

void loop() {
  static unsigned long lastUpdate = 15*60*1000;
  static unsigned long lastRollTime = 0;
  const unsigned long ROLL_INTERVAL = 60 * 1000; // Roll every minute during dead hours

  // Update temperature every minute
  if (millis() - lastUpdate > 1 * 60 * 1000) {
    Serial.println("Fetching Temp");
    lastUpdate = millis();
    int temperature = fetchTemperature();
    if (temperature != -1) {
      updateDigipot(temperature);
      Serial.printf("Temp: %d\n", temperature);
      consecutiveFailures = 0;
    } else {
      consecutiveFailures++;
      if (consecutiveFailures >= 50) {
        Serial.println("Too many failures. Setting temperature to 0.");
        updateDigipot(0);
      }
    }
  }
  // Perform digit rolling during dead hours
  if (millis() - lastRollTime > ROLL_INTERVAL && isDeadHours()) {
    rollDigits();
    lastRollTime = millis();
  }
}

void connectToWiFi() {
  WiFi.begin(SSID, PASSWORD);
  Serial.println("Connecting to Wi-Fi...");
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 40) { // Increase retries
      delay(500); // Increase delay
      Serial.print(".");
      retryCount++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to Wi-Fi!");
  } else {
    Serial.println("Failed to connect to Wi-Fi.");
  }
}

int fetchTemperature() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = "http://api.openweathermap.org/data/2.5/weather?q=" + location + "&appid=" + PLACEHOLDER_API_KEY + "&units=imperial";
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      int tempIndex = payload.indexOf("\"temp\":") + 7;
      int tempEnd = payload.indexOf(',', tempIndex);
      tempCache = payload.substring(tempIndex, tempEnd).toInt();
      return tempCache;
    } else {
      Serial.println("Failed to fetch temperature.");
      return -1;
    }

    http.end();
  } else {
    Serial.println("Wi-Fi not connected.");
    return -1;
  }
}

void updateDigipot(int temperature) {
  // Calculate target position based on temperature
  int targetPosition = temperature;
  // calculate offset
  if(targetPosition < 39) targetPosition-=15;
  else if (targetPosition >= 39 && targetPosition < 77) targetPosition-=16;
  else if (targetPosition >= 77 && targetPosition < 107) targetPosition-=17;
  else targetPosition-=18;
  // Ensure targetPosition is within valid bounds
  if (targetPosition < 0) {
    targetPosition = 0;
  } else if (targetPosition > 99) {
    targetPosition = 99;
  }
  if (targetPosition == currentPosition) return;
  else {
    Serial.print("Current position: ");
    Serial.println(currentPosition);
    Serial.print("Target position: ");
    Serial.println(targetPosition);
    if (targetPosition > currentPosition){
      for (uint8_t i = currentPosition; i < targetPosition; i++){
        pot.incr(); // Increment position
        delay(10);  // Small delay for stabilization
      }
    }
    else if (targetPosition < currentPosition){
      for (uint8_t i = currentPosition; i > targetPosition; i--){
        pot.decr(); // Decrement position
        delay(10);  // Small delay for stabilization
      }
    }
    // Update currentPosition after adjustments
    currentPosition = targetPosition;
    pot.setPosition(currentPosition);
  }
}

bool isDeadHours() {
  static bool initialized = false;
  static unsigned long lastNTPAttempt = 0;
  const unsigned long NTP_RETRY_INTERVAL = 10000; // Retry every 10 seconds if NTP fails

  // Initialize NTP client once
  if (!initialized) {
    timeClient.begin();
    initialized = true;
  }

  // Update NTP time
  if (!timeClient.update()) {
    // Retry if enough time has passed since the last attempt
    if (millis() - lastNTPAttempt > NTP_RETRY_INTERVAL) {
      timeClient.forceUpdate(); // Force update attempt
      lastNTPAttempt = millis();
    }
    // If unable to get time, assume it's not dead hours to avoid disruptions
    return false;
  }

  // Get the current hour (UTC) and convert to local time if needed
  int currentHour = (timeClient.getHours() - 6 + 24) % 24; // Example: UTC-6 for local time adjustment
  return currentHour >= 0 && currentHour < 7; // Dead hours: 12 AM to 7 AM
}

void rollDigits() {
  Serial.println("Rolling digits to prevent burnout...");
  updateDigipot(15);
  delay(100); 
  updateDigipot(22);
  delay(100); 
  updateDigipot(33);
  delay(100); 
  updateDigipot(44);
  delay(100); 
  updateDigipot(51);
  delay(100); 
  updateDigipot(66);
  delay(100); 
  updateDigipot(77);
  delay(100); 
  updateDigipot(88);
  delay(100); 
  updateDigipot(99);
  delay(100); 
  updateDigipot(100);
  delay(100); 
  updateDigipot(tempCache); // restore back to the temperature before the rolling
}