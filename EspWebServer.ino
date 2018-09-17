#include <ESP8266WiFi.h>
#include <WiFiClient.h>
//#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson
#include <FS.h>   // Include the SPIFFS library
#include <DHT.h>


HTTPClient http;
#define DHTTYPE DHT11
#define dht_dpin 0

#define RedPin 12
#define GreenPin 13
#define BluePin 14

DHT dht(dht_dpin,DHTTYPE);

//ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80
// Set web server port number to 80
//WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String outputState = "off";

// Assign output variables to GPIO pins
char output[2] = "5";

//flag for saving data
bool shouldSaveConfig = true;

void handleRoot();


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

#ifndef pinHandler_h
#define pinHandler_h
class pinHandler {
    public:
      pinHandler(int pin);
      void toggle();
      void status();
    private:
      int _pin;
      int _status;
  };
#endif
pinHandler::pinHandler(int pin) {
  pinMode(pin,OUTPUT);
  _pin = pin;
};
void pinHandler::toggle() {
      digitalWrite(_pin,!digitalRead(_pin));      // Change the state of the LED
      server.send(200, "application/json", "{'status' = 'success'}");                         
      server.sendHeader("Access-Control-Max-Age", "10000");
      server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      Serial.println(WiFi.localIP());
      Serial.println(WiFi.macAddress());     
};
void pinHandler::status(){
      bool roomStat = digitalRead(_pin);
      if (roomStat == 1) {
        server.sendHeader("Access-Control-Max-Age", "10000");
        server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", "{'status': 1}");                         
        }else{
        server.sendHeader("Access-Control-Max-Age", "10000");
        server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", "{'status': 0}");     
      }      
};

pinHandler port1(4);
pinHandler port2(5);

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)
void handleRoom1() {                          // If a POST request is made to URI /LED
      port1.toggle();         
   }
void handleRoom2() {                          // If a POST request is made to URI /LED
      port2.toggle();                      
   }
bool statusRoom1(){
      port1.status();
   }
bool statusRoom2(){
      port2.status();
   }   
bool ColorPicker(){      
      int red = server.arg("red").toInt();      
      int green = server.arg("green").toInt();      
      int blue = server.arg("blue").toInt();      
      setColor(red,green,blue);
  }
void readSensorData(){
    StaticJsonBuffer<256> jsonBuffer;
    // Create the root object    
    JsonObject& root = jsonBuffer.createObject();    
    root["tempreture"] = dht.readTemperature();
    root["humidity"] = dht.readHumidity();
    String json;
    root.prettyPrintTo(json);
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200,"application/json",json);      
  }  
void setColor(int red, int green, int blue){
      analogWrite(RedPin,red);
      analogWrite(GreenPin,green);
      analogWrite(BluePin,blue);
      server.sendHeader("Access-Control-Max-Age", "10000");
      server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "application/json", "{'status': 'Success'}");                         
  }
 void pingDevice(){
      server.sendHeader("Access-Control-Max-Age", "10000");
      server.sendHeader("Access-Control-Allow-Methods", "POST,GET,OPTIONS");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "application/json", "{'status': 'connected'}");                         
    }
void setup() {
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  dht.begin();
  delay(10);
  Serial.println('\n');
  pinMode(4,OUTPUT);
  pinMode(5,OUTPUT);
  pinMode(RedPin,OUTPUT);
  pinMode(GreenPin,OUTPUT);
  pinMode(BluePin,OUTPUT);
  Serial.println("Connecting ...");
  int i = 0;

  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("upitsIOT")) {              // Start the mDNS responder for esp8266.local
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  //read configuration from FS json
  Serial.println("mounting FS...");
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(output, json["output"]);
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
  
  WiFiManagerParameter custom_output("output", "output", output, 2);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  wifiManager.autoConnect("upitsIOT");
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  
  SPIFFS.begin();                           // Start the SPI Flash Files System
  
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

server.on("/port1", HTTP_GET, handleRoom1);
server.on("/port1/status", HTTP_GET, statusRoom1);
server.on("/port2", HTTP_GET, handleRoom2);
server.on("/port2/status", HTTP_GET, statusRoom2);
server.on("/RGB", HTTP_POST, ColorPicker);
server.on("/sensor/1", HTTP_GET, readSensorData);
server.on("/ping", HTTP_GET, pingDevice);


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["output"] = output;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }
  
  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  if (WiFi.status() == WL_CONNECTED) {  //Wait for the WiFI connection completion     
    Serial.println("Loggin Device Into Cloud");
    http.begin("http://192.168.2.111:8080/api/v1/deviceLog");
    http.addHeader("Content-Type", "application/json");
    StaticJsonBuffer<256> jsonBuffer;
    // Create the root object    
    JsonObject& logPost = jsonBuffer.createObject();    
    logPost["local_ip"] = WiFi.localIP().toString() ;     
    logPost["mac_address"] = TheMac();
    String json;
    logPost.prettyPrintTo(json); 
    int httpCode = http.POST(json);
    String payload = http.getString();
    http.end();
    Serial.println(json);
  }
}


//String getMacAddress() {
//  byte mac[6];
//  WiFi.macAddress(mac);
//  String cMac = "";
//  for (int i = 0; i < 6; ++i) {
//  cMac += String(mac[i],HEX);
//  if(i<5)
//  cMac += ":";
//  }
//  cMac.toUpperCase();
//  return cMac;
//}

String TheMac() {
  String s = WiFi.macAddress();
  return s;
  }

void loop(void) {
    
  server.handleClient();
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false;                                         // If the file doesn't exist, return false
}
