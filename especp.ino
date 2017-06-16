#include "config.h"
#include <ESP8266WiFi.h>
#include "vista.h"
#ifdef HTTP_SERVER
#include <FS.h>
#include <ESP8266mDNS.h>
#include <ESP8266LLMNR.h>
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include <SPIFFSEditor.h>
#endif
#ifdef IFTTT
#include <IFTTTMaker.h>
#endif
#ifdef OTA
#include <ArduinoOTA.h>
#endif

#include <time.h>
#include <Stream.h>
//#include <ArduinoJson.h>

#ifdef TELNETSERIAL
WiFiServer TelnetServer(23);
WiFiClient serverClients;
#endif
#ifdef IFTTT
IFTTTMaker ifttt(maker_webhook);
#endif
#ifdef SMTP
WiFiServer smtp(587);
WiFiClient smtpClients;
enum smtp_state_enum { smtp_init,wait_helo , wait_mail, wait_enddata, wait_quit};
enum smtp_state_enum smtp_state; 
String smtp_str;
#endif
Stream *OutputStream = &Serial;
Vista vista(RX_PIN, TX_PIN, KPADDR, OutputStream);
#define DBG_OUTPUT_PORT Serial

String inputString;
bool ademco_mode = false;
#ifdef HTTP_SERVER
AsyncWebServer WebServer(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){
    OutputStream->printf("ws[%s][%u] connect\n", server->url(), client->id());
//    client->printf("Hello Client %u :)", client->id());
    client->ping();
  } else if(type == WS_EVT_DISCONNECT){
    OutputStream->printf("ws[%s][%u] disconnect: %u\n", server->url(), client->id());
  } else if(type == WS_EVT_ERROR){
    OutputStream->printf("ws[%s][%u] error(%u): %s\n", server->url(), client->id(), *((uint16_t*)arg), (char*)data);
  } else if(type == WS_EVT_PONG){
    OutputStream->printf("ws[%s][%u] pong[%u]: %s\n", server->url(), client->id(), len, (len)?(char*)data:"");
  } else if(type == WS_EVT_DATA){
    AwsFrameInfo * info = (AwsFrameInfo*)arg;
    String msg = "{\"type\":\"command\",\"command\":\"";
    if(info->final && info->index == 0 && info->len == len){
      //the whole message is in a single frame and we got all of it's data
      OutputStream->printf("ws[%s][%u] %s-message[%llu]: ", server->url(), client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
	  /*
          if ((data[i] >= 0x30 && data[i] <=0x39) || data[i] == 0x23 || data[i] == 0x2a || (data[i] >= 0x41 && data[i] <=0x44)){
            OutputStream->print("Queue ");
            OutputStream->println(data[i]);
            vista.out_wire_queue(data[i]);
          } else {
            OutputStream->print(data[i]);
            OutputStream->println(" unqueueable");
            all_queued = false;
          }
	  */
          msg += (char) data[i];
        }
	msg += "\"}";

	ws.text(client->id(), msg);

      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      OutputStream->printf("%s\n",msg.c_str());

    //  if(info->opcode == WS_TEXT)
      //  client->text("I got your text message");
    //  else
      //  client->binary("I got your binary message");
    } else {
      //message is comprised of multiple frames or the frame is split into multiple packets
      if(info->index == 0){
        if(info->num == 0)
          OutputStream->printf("ws[%s][%u] %s-message start\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
        OutputStream->printf("ws[%s][%u] frame[%u] start[%llu]\n", server->url(), client->id(), info->num, info->len);
      }

      OutputStream->printf("ws[%s][%u] frame[%u] %s[%llu - %llu]: ", server->url(), client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);

      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
      } else {
        char buff[3];
        for(size_t i=0; i < info->len; i++) {
          sprintf(buff, "%02x ", (uint8_t) data[i]);
          msg += buff ;
        }
      }
      OutputStream->printf("%s\n",msg.c_str());

      if((info->index + len) == info->len){
        OutputStream->printf("ws[%s][%u] frame[%u] end[%llu]\n", server->url(), client->id(), info->num, info->len);
        if(info->final){
          OutputStream->printf("ws[%s][%u] %s-message end\n", server->url(), client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
      //    if(info->message_opcode == WS_TEXT)
            //client->text("I got your text message");
        //  else
            //client->binary("I got your binary message");
        }
      }
    }
  }
}
#endif

void processStrCmd(Stream *in, Stream *out) {
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
      out->print("ademco>");
    }
    else if (subcommand.startsWith("clear")) {
      vista.out_wire_init();
      out->println("Clear ademco");
    }
    else if (subcommand.startsWith("keypad")) {
      String keypad_number = subcommand.substring(subcommand.indexOf(' '));
      keypad_number.trim();
      int keypad_num = keypad_number.toInt();
      if(keypad_num > 16 && keypad_num < 24) {
	out->print("Set keypad to: ");
	out->println(keypad_num);
      }
    }
  } else {
    out->print("Command: \'");
    out->print(inputString);
    out->println("\' not supported");
  }
}

void handleCmd(Stream *in, Stream *out)
{
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
      if (inChar == '\n') {
        inputString.trim();
	processStrCmd(in, out);
        inputString = ""; //Clear
      } else if ((inChar >= 32 && inChar < 128)) {
        inputString += inChar;
        out->write(inChar);
      }
    }
  }
}

void setup() {
  int i = 3;
  DBG_OUTPUT_PORT.begin(115200);
  SPIFFS.begin();
  OutputStream  = &Serial;
  // Setup Wifi
  OutputStream->printf("Chip ID = %08X\n", ESP.getChipId());
  WiFi.hostname(espHOST);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);
  i = 120; //One minute timeout for DHCP
  DBG_OUTPUT_PORT.printf("Connecting to %s\n",WiFiSSID);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    DBG_OUTPUT_PORT.println("Connection Fail");
    WiFi.disconnect(false);
    delay(1000);
    WiFi.begin(WiFiSSID, WiFiPSK);
    i--;
    if (i==0) {
      DBG_OUTPUT_PORT.println("Giving Up Wifi");
    }
  }
  OutputStream->print("Connected, IP address: ");
  OutputStream->println(WiFi.localIP());

#ifdef OTA
  ArduinoOTA.onStart([]() {
      String type;
      vista.stop();
      if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
      else // U_SPIFFS
      type = "filesystem";
      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      OutputStream->println("Start updating " + type);
      ws.textAll("{\"type\":\"firmware_update\",\"progress\":0.0}");
      });
  ArduinoOTA.onEnd([]() {
      OutputStream->printf("\nEnd");
      ws.closeAll(1001, "{\"type\":\"firmware_update\",\"progress\": 100.0}");
      });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      OutputStream->printf("Progress: %u%%\r", (progress / (total / 100)));
      ws.textAll("{\"type\":\"firmware_update\",\"progress\":" + String(progress / (total/100)));
      });
  ArduinoOTA.onError([](ota_error_t error) {
      OutputStream->printf("Error[%u]: ", error);
	if (error == OTA_AUTH_ERROR) {
	  OutputStream->println("Auth Failed");
	  ws.textAll("{\"type\":\"firmware_update\",\"msg\": \"Auth Failed\"}");
	}
	else if (error == OTA_BEGIN_ERROR) {
	  OutputStream->println("Begin Failed");
	  ws.textAll("{\"type\":\"firmware_update\",\"msg\": \"Begin Failed\"}");
	}
	else if (error == OTA_CONNECT_ERROR) {
	  OutputStream->println("Connect Failed");
	  ws.textAll("{\"type\":\"firmware_update\",\"msg\": \"Connect Failed\"}");
	}
	else if (error == OTA_RECEIVE_ERROR) {
	  OutputStream->println("Receive Failed");
	  ws.textAll("{\"type\":\"firmware_update\",\"msg\": \"Received Failed\"}");
	}
	else if (error == OTA_END_ERROR) {
	  OutputStream->println("End Failed");
	  ws.textAll("{\"type\":\"firmware_update\",\"msg\": \"End Failed\"}");
	}
      });
  ArduinoOTA.begin();
  if (!MDNS.begin(espHOST)) {
    OutputStream->println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  LLMNR.begin(espHOST);
  MDNS.addService("http","tcp",80);
#endif
  configTime( -7*3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now_t;
  while (!(time(&now_t))) {
    OutputStream->print(".");
    delay(1000);
  }

  // attach AsyncWebSocket
  ws.onEvent(onWsEvent);
  WebServer.addHandler(&ws);
  WebServer.addHandler(new SPIFFSEditor(http_username,http_password));
  WebServer.serveStatic("/",SPIFFS, "/").setDefaultFile("index.htm");
  WebServer.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/plain", String(ESP.getFreeHeap()));
      });
  //called when the url is not defined here
  //use it to load content from SPIFFS
  WebServer.onNotFound([](AsyncWebServerRequest *request){
      request->send(404);
      });


  char buffer[80] = "ESP Reset time ";
  strcat(buffer, ctime(&now_t));
  DBG_OUTPUT_PORT.println(buffer);
#ifdef TELNETSERIAL
  TelnetServer.begin();
#endif
#ifdef SMTP
  smtpClients;
  smtp_state = smtp_init;
  smtp.begin();
#endif
#ifdef HTTP_SERVER
  WebServer.begin();
  vista.setWS(&ws);
#endif
  vista.begin();

}
#ifdef SMTP
void smtpActivate() {
#ifdef IFTTT
  if(ifttt.triggerEvent("ipcam_alarm")){
    OutputStream->println("Send to ifttt");
  } else {
    OutputStream->println("Fail to send");
  }
#endif
}
void process_smtpState() {
    switch (smtp_state) {
      case smtp_init:
	smtpClients.write("220 weeminSmtp\r\n");
	smtp_state =  wait_helo;
	break;
      case wait_helo:
	if (smtp_str.startsWith("HELO")) {
	  String client_name = smtp_str.substring(smtp_str.indexOf(' ') +1);
	  String tmp_str = String("250 Hello") + client_name + String("\r\n");
	  smtpClients.write(tmp_str.c_str());
	  smtp_state = wait_mail;
	} else {
	  smtp_state = wait_helo;
	}
	break;
      case wait_mail:
	if (smtp_str.startsWith("MAIL FROM:") || smtp_str.startsWith("RCPT TO:")) {
#ifdef SMTP_DEBUG
	  OutputStream->println("Got: "+smtp_str.substring(smtp_str.indexOf(':')+1));
	  OutputStream->println("Send 250 Ok");
#endif
	  smtpClients.write("250 Ok\r\n");
	} else if (smtp_str.startsWith("DATA")) {
	  smtpClients.write("354 End Data with <CR><LF>.<CR><LF>\r\n");
	  smtp_state = wait_enddata;
	} else {
	  smtp_state = wait_helo;
	}
	break;
      case wait_enddata:
	if (smtp_str.startsWith(F(".")) && smtp_str.length() == 1) {
	  smtpClients.write("250 Ok: queued as 12345\r\n");
	  smtp_state = wait_quit;
	}
	break;
      case wait_quit:
	if (smtp_str.startsWith("QUIT")) {
	  smtpClients.write("221 Bye\r\n");
	  smtpClients.stop();
	  smtpActivate();
	} else {
	  smtp_state = wait_helo;
	}
	break;
      default:
	smtp_state = wait_helo;
#ifdef SMTP_DEBUG
	OutputStream->println(F("Not sure how we got to this state"));
#endif
	break;
    }
}

void smtpHandle() {
  if (smtpClients && !smtpClients.connected()) {
    OutputStream->println("Disconect SMTP Clients");
    smtpClients.stop();
  } else if (smtpClients.connected()) {
    if (smtpClients.available()) {
      char inChar = (char) smtpClients.read();
      if (inChar == '\n') {
#ifdef SMTP_DEBUG
	OutputStream->println(smtp_str);
#endif
	process_smtpState();
	smtp_str = "";
      } else {
	if (inChar != '\r') {
	  smtp_str += inChar;
	}
      }
    }
  } else if (smtp.hasClient()) {
    if (!smtpClients || !smtpClients.connected()) {
      if(smtpClients) smtpClients.stop();
      smtpClients = smtp.available();
      OutputStream->println("New Smtp Client:");
      smtp_state = wait_helo;
      smtpClients.write("220 weeminECP\r\n");
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

#ifdef TELNETSERIAL
  telnetHandle();
#endif
#ifdef SMTP
  smtpHandle();
#endif
  vista.handle();
}

