#ifndef __CONFIG__
#define __CONFIG__
//////////////////////
// WiFi Definitions //
//////////////////////
const char WIFISSID[] = "";
const char WIFIPSK[] = "";

const char ESPHOST[] = "";
const char HTTP_USERNAME[] = "";
const char HTTP_PASSWORD[] = "";
const char MAKER_KEY[] = "";
const char MAKER_IPCAM_ALARM[] = "";
#define RX_PIN 12
#define TX_PIN 13
#define KPADDR 22

//#define SMTP
#define OTA
#define IFTTT
#define SMTP
#define SMTP_DEBUG
#define TELNETSERIAL
#define HTTP_SERVER 1
#define MAX_SRV_CLIENTS 1


#define DEBUG_STATUS 1
#define DEBUG_DISPLAY 0
#define DEBUG_KEYS 1
#define DEBUG_OUTWIRE 1
#endif
