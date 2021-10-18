#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

//---------- AP vars ----------
#define AP_SSID "KMS Temp logger"
IPAddress AP_IP = IPAddress(10, 10, 10, 1);
IPAddress AP_gateway = IPAddress(10, 10, 10, 1);
IPAddress AP_nmask = IPAddress(255, 255, 255, 0);
//-------- Program vars -------
bool updateStatus = true;
String responseMsg = String("23 25");
AsyncWebServer server(80);
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
  } else if (var == "passwd"){
    return readFile(SPIFFS, "/passwd.txt");
  }
  return String();
}
//-----------------------------

void setup() {
  Serial.begin(115200);
  if (!SPIFFS.begin()) {
    updateStatus = false;
    Serial.println("SPIFFS Failed!");
    // write on screen error msg here
  } else {
    // try to connect to wifi
    WiFi.mode(WIFI_STA);
    WiFi.begin(readFile(SPIFFS, "/ssid.txt").c_str(), readFile(SPIFFS, "/passwd.txt").c_str());
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("Cannot connect to network!");
      updateStatus = false;
      // write on screen error msg here
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID);
      WiFi.softAPConfig(AP_IP, AP_gateway, AP_nmask);
      Serial.print("IP Address: ");
      Serial.println(WiFi.softAPIP());

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false, processor);
      });

      server.on("/save", HTTP_GET, [](AsyncWebServerRequest *request) {
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
      });

      server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
      });

      server.begin();
    } else {
      updateStatus = true;
      IPAddress ip = WiFi.localIP();
      Serial.print("IP Address: ");
      Serial.println(ip);
      // write on screen info here

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", responseMsg.c_str());
      });

      server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
      });

      server.begin();
    }
  }
}

void loop() {
  
}