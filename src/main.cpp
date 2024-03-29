#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

//---------- AP vars ----------
#define AP_SSID "KMS Temp logger"
IPAddress AP_IP = IPAddress(10, 10, 10, 1);
IPAddress AP_gateway = IPAddress(10, 10, 10, 1);
IPAddress AP_nmask = IPAddress(255, 255, 255, 0);
//-------- Program vars -------
// this value is given when sensor is initializing
#define SENSOR_INIT 85.00
#define SENSOR_1_PIN D6
#define SENSOR_2_PIN D5
#define AP_MODE_PIN D8
#define LONG_PRESS_MILLIS 4000
#define RECONNECT_MILLIS 10000
float sensor1_temp;
float sensor2_temp;
float sensor1_comp;
float sensor2_comp;
unsigned long sensor1_millis;
unsigned long sensor2_millis;
unsigned long pressedTime;
unsigned long releasedTime;
unsigned long previousMillis;
int lastState;
int currentState;
bool isInApMode;
WiFiEventHandler onGotIpHandler;
WiFiEventHandler onFailedToConnectHandler;
AsyncWebServer server(80);
OneWire sensor1_OW(SENSOR_1_PIN);
OneWire sensor2_OW(SENSOR_2_PIN);
DallasTemperature sensor1_DT(&sensor1_OW);
DallasTemperature sensor2_DT(&sensor2_OW);
LiquidCrystal_I2C lcd(0x27, 16, 2);
//-----------------------------
String readFile(fs::FS &fs, const char * path) {
  Serial.println();
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.print("- read from file: ");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  file.close();
  Serial.println(fileContent);
  return fileContent;
}
//-----------------------------
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
  file.close();
}
//-----------------------------
String processor(const String& var) {
  if (var == "ssid") {
    return readFile(SPIFFS, "/ssid.txt");
  } else if (var == "passwd") {
    return readFile(SPIFFS, "/passwd.txt");
  } else if (var == "sens1") {
    return readFile(SPIFFS, "/sensor1.txt");
  } else if (var == "sens2") {
    return readFile(SPIFFS, "/sensor2.txt");
  }
  return String();
}
//-----------------------------
void readTemp_s1() {
  float tmp = sensor1_DT.getTempCByIndex(0);
  sensor1_temp = tmp;
  if ((sensor1_temp != DEVICE_DISCONNECTED_C) && (sensor1_temp != SENSOR_INIT)) sensor1_temp += sensor1_comp;
  Serial.print("S1: ");
  Serial.println(sensor1_temp);
  sensor1_DT.requestTemperatures();
  sensor1_millis = millis();
}
//-----------------------------
void readTemp_s2() {
  float tmp = sensor2_DT.getTempCByIndex(0);
  sensor2_temp = tmp;
  if ((sensor2_temp != DEVICE_DISCONNECTED_C) && (sensor2_temp != SENSOR_INIT)) sensor2_temp += sensor2_comp;
  Serial.print("S2: ");
  Serial.println(sensor2_temp);
  sensor2_DT.requestTemperatures();
  sensor2_millis = millis();
}
//-----------------------------
void updateS1OnScreen() {
  String res;
  if (sensor1_temp != DEVICE_DISCONNECTED_C) res += sensor1_temp;
  else res += "XX.XX";
  res += (char)223;
  res += "C";
  if (res.length() < 8) res += " ";
  lcd.setCursor(0, 0);
  lcd.print(res.c_str());
}
//-----------------------------
void updateS2OnScreen() {
  String res;
  if (sensor2_temp != DEVICE_DISCONNECTED_C) res += sensor2_temp;
  else res += "XX.XX";
  res += (char)223;
  res += "C";
  if (res.length() < 8) res += " ";
  lcd.setCursor(8, 0);
  lcd.print(res.c_str());
}
//-----------------------------
void initApMode() {
  isInApMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID);
  delay(100); // crude hack
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
  lcd.setCursor(0, 1);
  lcd.print("AP! ");
  lcd.print(WiFi.softAPIP());
}
//-----------------------------
void onGotIp(const WiFiEventStationModeGotIP& event) {
  isInApMode = false;
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(ip);
}
//-----------------------------
void onDisconnected(const WiFiEventStationModeDisconnected& event) {
  lcd.setCursor(0, 1);
  lcd.print("DISCONNECTED     ");
  WiFi.disconnect();
}
//-----------------------------
void connectToWifi() {
  isInApMode = false;
  WiFi.mode(WIFI_STA);
  onGotIpHandler = WiFi.onStationModeGotIP(onGotIp);
  onFailedToConnectHandler = WiFi.onStationModeDisconnected(onDisconnected);
  String ssid = readFile(SPIFFS, "/ssid.txt");
  if (ssid.isEmpty()) initApMode();
  else {
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(ssid.c_str(), readFile(SPIFFS, "/passwd.txt").c_str());
  } 
}
//-----------------------------
void setup() {
  Serial.begin(115200);

  pinMode(AP_MODE_PIN, INPUT_PULLUP);

  Wire.begin(4, 5);
  lcd.init();
  lcd.clear();
  lcd.backlight();

  sensor1_DT.setWaitForConversion(false);
  sensor1_DT.setResolution(0x1F);
  sensor1_DT.begin();
  sensor1_DT.requestTemperatures();
  sensor1_millis = millis();
  sensor1_temp = DEVICE_DISCONNECTED_C;
  updateS1OnScreen();

  sensor2_DT.setWaitForConversion(false);
  sensor2_DT.setResolution(0x1F);
  sensor2_DT.begin();
  sensor2_DT.requestTemperatures();
  sensor2_millis = millis();
  sensor2_temp = DEVICE_DISCONNECTED_C;
  updateS2OnScreen();

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Failed!");
    lcd.setCursor(0, 1);
    lcd.print("SPIFFS error");
    while (true) {}
  }

  lcd.setCursor(0, 1);
  lcd.print("CONNECTING");

  connectToWifi();

  sensor1_comp = readFile(SPIFFS, "/sensor1.txt").toFloat();
  sensor2_comp = readFile(SPIFFS, "/sensor2.txt").toFloat();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html", false, processor);
  });

  server.on("/network", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("save")) {
      String reqData;
      if (request->hasParam("ssid")) {
        reqData = request->getParam("ssid")->value();
        writeFile(SPIFFS, "/ssid.txt", reqData.c_str());
        Serial.print("Received ssid: ");
        Serial.println(reqData);
      }
      if (request->hasParam("passwd")) {
        reqData = request->getParam("passwd")->value();
        writeFile(SPIFFS, "/passwd.txt", reqData.c_str());
        Serial.print("Received passwd: ");
        Serial.println(reqData);
      }
      request->redirect("/");
    } else if (request->hasParam("connect")) {
      Serial.println("CONNECTTING");
      lcd.setCursor(0, 1);
      lcd.print("CONNECTING      ");
      connectToWifi();
    }
  });

  server.on("/savecalib", HTTP_GET, [](AsyncWebServerRequest *request) {
    String reqData;
    if (request->hasParam("sens1")) {
      reqData = request->getParam("sens1")->value();
      writeFile(SPIFFS, "/sensor1.txt", reqData.c_str());
      Serial.print("Received sensor 1: ");
      Serial.println(reqData);
    }
    if (request->hasParam("sens2")) {
      reqData = request->getParam("sens2")->value();
      writeFile(SPIFFS, "/sensor2.txt", reqData.c_str());
      Serial.print("Received sensor 2: ");
      Serial.println(reqData);
    }
    request->redirect("/");
  });

  server.on("/gettemp", HTTP_GET, [](AsyncWebServerRequest *request) {
    String temps = String(sensor1_temp)+" "+String(sensor2_temp);
    request->send(200, "text/html", temps.c_str());
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server.begin();
}

void loop() {
  if (!isInApMode) {
    currentState = digitalRead(AP_MODE_PIN);
    if ((lastState == LOW) && (currentState == HIGH)) {
      pressedTime = millis();
    } else if ((lastState == HIGH) && (currentState == LOW)) {
      releasedTime = millis();
    }
    long pressDuration = releasedTime - pressedTime;

    if (pressDuration >= LONG_PRESS_MILLIS) {
      initApMode();
      pressedTime = 0;
      releasedTime = 0;
    }

    lastState = currentState;
  }

  if (!isInApMode && (WiFi.status() != WL_CONNECTED)) {
    unsigned long currentMillis = millis();
    if ((currentMillis - previousMillis) >= RECONNECT_MILLIS) {
      Serial.println("RECONNECTING TO WIFI");
      connectToWifi();
      previousMillis = currentMillis;
    }
  }

  if ((millis() - sensor1_millis) > 1000) {
    readTemp_s1();
    updateS1OnScreen();
  }

  if ((millis() - sensor2_millis) > 1000) {
    readTemp_s2();
    updateS2OnScreen();
  }
}