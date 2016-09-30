/**
 * This workshop contains a button and an LED.
 * When the button is pressed, a Salesforce Case will be created.
 * The LED will be controlled by the number of open cases.
 * If there are any open cases, the LED will be lit.
 *
 * Visit https://www.losant.com/getting-started/losant-iot-dev-kits/builder-kit-salesforce/ for full instructions.
 *
 * Copyright (c) 2016 Losant IoT. All rights reserved.
 * https://www.losant.com
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Losant.h>

// WiFi credentials.
const char* WIFI_SSID = "my-wifi-ssid";
const char* WIFI_PASS = "my-wifi-pass";

// Losant credentials.
const char* LOSANT_DEVICE_ID = "my-device-id";
const char* LOSANT_ACCESS_KEY = "my-access-key";
const char* LOSANT_ACCESS_SECRET = "my-access-secret";

const int BUTTON_PIN = 5;
const int LED_PIN = 4;

bool ledState = false;
int buttonState = 0;

// For an unsecure connection to Losant.
WiFiClientSecure wifiClient;

LosantDevice device(LOSANT_DEVICE_ID);

// The set up function will run first and only one time.
void setup() {
  Serial.begin(115200);

  // Giving it a little time because the serial monitor doesn't
  // immediately attach. Want the workshop that's running to
  // appear on each upload.
  delay(2000);

  Serial.println();
  Serial.println("Running Salesforce Workshop Firmware.");

  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Register the command handler to be called when a command is received
  // from the Losant platform.
  device.onCommand(&handleLosantCommand);

  connect();
}

// The loop function will run repeatedly as fast as the microcontroller will
// allow.
void loop() {
  bool toReconnect = false;

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from WiFi");
    toReconnect = true;
  }

  if(!device.connected()) {
    Serial.println("Disconnected from Losant");
    Serial.println(device.mqttClient.state());
    toReconnect = true;
  }

  if(toReconnect) {
    connect();
  }

  // This allows the Losant Client to maintain the connection and check for any incoming messages
  device.loop();

  int currentRead = digitalRead(BUTTON_PIN);

  if(currentRead != buttonState) {
    buttonState = currentRead;
    if(buttonState) {
      buttonPressed();
    }
  }

  delay(100);
}

// Triggered when button is pressed
void buttonPressed() {
  Serial.println("Button Pressed!");

  // Losant uses a JSON protocol. Construct the simple state object.
  // { "button" : true }
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["button"] = true;

  // Send the state to Losant.
  device.sendState(root);
}

// Toggle LED on/off
void toggleLed() {
  Serial.println("Toggling LED.");
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
}

// An Infinite loop to stop the program when an error occurs.
void halt(){
  while(true) {
    delay(500);
  }
}

// Called whenever the device receives a command from the Losant platform.
void handleLosantCommand(LosantCommand *command) {
  Serial.print("Command received: ");
  Serial.println(command->name);

  if(strcmp(command->name, "toggle") == 0) {
    toggleLed();
  }
}

// Connect to WiFI and Losant
void connect() {

  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // Workaround to fix wifi connection problems.
  // See: https://github.com/esp8266/Arduino/issues/2186#issuecomment-228581052
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  } else {
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  while (WiFi.status() != WL_CONNECTED) {
    // Failed to find SSID
    if(WiFi.status() == WL_NO_SSID_AVAIL){
      Serial.println();
      Serial.println("Failed to find SSID.");
      Serial.print("SSID: ");
      Serial.println(WIFI_SSID);
      halt();
    }

    // Failed to connect to WIfi
    if (WiFi.status() == WL_CONNECT_FAILED) {
      Serial.println();
      Serial.println("Failed to connect to WIFI. Please verify credentials: ");
      Serial.println();
      Serial.print("SSID: ");
      Serial.println(WIFI_SSID);
      Serial.print("Password: ");
      Serial.println(WIFI_PASS);
      Serial.println();
      Serial.println("Trying again...");
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      delay(10000);
    }

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.print("Authenticating Device...");

  // Use the Losant Auth API to verify device
  // See: https://docs.losant.com/rest-api/overview/#device-based-authentication

  HTTPClient http;
  http.begin("http://api.losant.com/auth/device");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");

  // Create JSON payload to sent to Losant API
  //  {
  //     "deviceId": "575ecf887ae143cd83dc4aa2",
  //     "key": "this_would_be_the_key",
  //     "secret": "this_would_be_the_secret"
  //   }
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["deviceId"] = LOSANT_DEVICE_ID;
  root["key"] = LOSANT_ACCESS_KEY;
  root["secret"] = LOSANT_ACCESS_SECRET;
  String buffer;
  root.printTo(buffer);

  int httpCode = http.POST(buffer);

  if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
          Serial.println("This device is authorized!");
      } else {
        Serial.println("Failed to authorize device to Losant.");
        if(httpCode == 400) {
          Serial.println("Validation error: The device ID, access key, or access secret is not in the proper format.");
        } else if(httpCode == 401) {
          Serial.println("Invalid credentials to Losant: Please double-check the device ID, access key, and access secret.");
        } else {
           Serial.println("Unknown response from API");
        }
        halt();
      }
    } else {
        Serial.println("Failed to connect to Losant API.");
        Serial.printf("Error: %s\n", http.errorToString(httpCode).c_str());
        halt();
   }

  http.end();

  // Connect to Losant.
  Serial.println();
  Serial.print("Connecting to Losant...");

  device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);

  while(!device.connected()) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected!");
  Serial.println();
  Serial.println("This device is now ready for use!");
}
