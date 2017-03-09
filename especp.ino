#include "config.h"
#include <ESP8266WiFi.h>
#include "vista.h"
#ifdef HTTP_SERVER
#include <FS.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WebSocketServer.h>
#endif
#ifdef OTA
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#endif

#include <time.h>
#include <Stream.h>
#include <string.h>

#ifdef TELNETSERIAL
WiFiServer TelnetServer(23);
WiFiClient serverClients;
#endif
Stream *OutputStream = &Serial;
Vista vista(RX_PIN, TX_PIN, KPADDR, OutputStream);
#define DBG_OUTPUT_PORT Serial

bool ademco_mode = false;
#ifdef HTTP_SERVER
ESP8266WebServer WebServer(80);
WebSocketServer WS;
WiFiServer WSServer(10112);
WiFiClient wsclient;
bool ws_connected = false;
//holds the current upload
File fsUploadFile;
//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(WebServer.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".mp3")) return "audio/mpeg3;audio/x-mpeg-3";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = WebServer.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload(){
  if(WebServer.uri() != "/edit") return;
  HTTPUpload& upload = WebServer.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void handleFileDelete(){
  if(WebServer.args() == 0) return WebServer.send(500, "text/plain", "BAD ARGS");
  String path = WebServer.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if(path == "/")
    return WebServer.send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return WebServer.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  WebServer.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){
  if(WebServer.args() == 0)
    return WebServer.send(500, "text/plain", "BAD ARGS");
  String path = WebServer.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if(path == "/")
    return WebServer.send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return WebServer.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return WebServer.send(500, "text/plain", "CREATE FAILED");
  WebServer.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  if(!WebServer.hasArg("dir")) {WebServer.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = WebServer.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  WebServer.send(200, "text/json", output);
}


#endif





void handleCmd(Stream *in, Stream *out) 
{
  static String inputString;
  if(in->available()) {
    char inChar = (char) in->read();
    if (ademco_mode) {
      if (!((inChar >= 0x30 && inChar <=0x39) || inChar == 0x23 || inChar == 0x2a || (inChar >= 0x41 && inChar <=0x44))){
	      ademco_mode = false;
      } else {
	OutputStream->print("ademco>");
	vista.out_wire_queue(inChar);
      }
    } else { 
      if (inChar =='\n') {
	inputString.trim();

	if(inputString == "heap") {
	  out->println(ESP.getFreeHeap());
	} else if(inputString == "adc") {
	  out->printf("ADC: %d\n\r", analogRead(A0));
	} else if(inputString == "chipId") {
	  out->printf("ChipID: %08X\n\r", ESP.getChipId());
	} else if(inputString == "reset") {
	  ESP.restart();
	} else if(inputString == "exit") {
	  if (in != &Serial) { // If we're not in Serial mode
	    ((WiFiClient *)in)->stop(); //Close connection
	    OutputStream = &Serial;
	    vista.setStream(OutputStream);
	  }
	} else if(inputString.startsWith("ademco") && inputString.length() > 6) {
	  String subcommand = inputString.substring(inputString.indexOf(' '));
	  subcommand.trim();
	  if (subcommand.startsWith("mode")) {
	    ademco_mode = true;
	    OutputStream->print("ademco>");
	  }
	  else if (subcommand.startsWith("clear")) {
	    vista.out_wire_init();
	    OutputStream->println("Clear ademco");
	  }
	  else if (subcommand.startsWith("keypad")) {
	    String keypad_number = subcommand.substring(subcommand.indexOf(' '));
	    keypad_number.trim();
	    int keypad_num = keypad_number.toInt();
	    if(keypad_num > 16 && keypad_num < 24) {
	      OutputStream->print("Set keypad to: ");
	      OutputStream->println(keypad_num);
	    }
	  }
	} else {
	  out->print("Command \'");
	  out->print(inputString);
	  out->println("\' not supported");
	}
	inputString = ""; //Clear 
      } else if ((inChar >= 32 && inChar < 128) || inChar != '\r' ) {
	inputString += inChar;
	out->write(inChar);
      }
    }
  }
}

void setup() {
  int i = 120;
  DBG_OUTPUT_PORT.begin(115200);
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }
  // Setup Wifi
  Serial.printf("Chip ID = %08X\n", ESP.getChipId());
  WiFi.hostname(espHOST);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);
  i = 120; //One minute timeout for DHCP
  DBG_OUTPUT_PORT.printf("Connecting to %s\n",WiFiSSID);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
    if (--i == 0) {
      DBG_OUTPUT_PORT.println("Connection Fail");
      ESP.restart();
      break;
    }
  }
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

#ifdef OTA
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
    vista.stop();
    });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin(); 
#endif
  configTime( 0, 0, "pool.ntp.org", "time.nist.gov"); 
  time_t now_t;
  while (!(time(&now_t))) {
    Serial.print(".");
    delay(1000);
  }

  //SERVER INIT
  //list directory
  WebServer.on("/list", HTTP_GET, handleFileList);
  //load editor
  WebServer.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) WebServer.send(404, "text/plain", "FileNotFound");
  });
  //create file
  WebServer.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  WebServer.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  WebServer.on("/edit", HTTP_POST, [](){ WebServer.send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  WebServer.onNotFound([](){
    if(!handleFileRead(WebServer.uri()))
      WebServer.send(404, "text/plain", "FileNotFound");
  });


  char buffer[80] = "ESP Reset time ";
  strcat(buffer, ctime(&now_t));
  DBG_OUTPUT_PORT.println(buffer);
  OutputStream  = &Serial;
#ifdef TELNETSERIAL
  TelnetServer.begin();
#endif
#ifdef HTTP_SERVER
  WebServer.begin();
  WSServer.begin();
#endif
  vista.begin();

}
#ifdef HTTP_SERVER
void wsHandle() {
  if (WSServer.hasClient()) {
    wsclient = WSServer.available();
    if (WS.handshake(wsclient)) {
    //We have a websocket client
       ws_connected = true;
    }
  }
}
#endif
#ifdef TELNETSERIAL
void telnetHandle() {
   if (serverClients.connected()) { //If we have client connected
       handleCmd(&serverClients, &serverClients);
   } 
   
   else if (TelnetServer.hasClient()){
      //find free/disconnected spot
      if (!serverClients || !serverClients.connected()){
	 if(serverClients) serverClients.stop();
	 serverClients = TelnetServer.available();
	 OutputStream->print("New client: "); 
         OutputStream = &serverClients;
	 vista.setStream(OutputStream);
      }
   }
}
#endif

void loop() {
   
#ifdef OTA
   ArduinoOTA.handle();
#endif

#ifdef HTTP_SERVER
   WebServer.handleClient();
   wsHandle();
#endif
#ifdef TELNETSERIAL
   telnetHandle();
#endif
   //handleCmd(&Serial, &Serial); 
   // Handle Vista
   vista.handle();
}

