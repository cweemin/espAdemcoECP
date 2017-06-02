#ifndef __VISTA_H_
#define __VISTA_H_

#include "ecpSoftwareSerial.h"
#include <ESPAsyncWebServer.h>
#include <Stream.h>
#include "ademcodata.h"

#define MAX_STREAM 4

enum ecpState { midAck, pulse, normal, ack_f7,send_kp };

class Vista {
  public:
  Vista(int receivePin, int transmitPin, char kpaddr, Stream *OutStream);
  ~Vista();
  void begin();
  void stop();
  void handle();
  void handleISR();
  void setStream(Stream* tmp) { OutputStream = tmp;}
  void setWS(AsyncWebSocket *tmp) { ws = tmp; }
  void setkpddr(char tmp) { m_kpaddr = tmp; }
  void out_wire_queue(char byt);
  void out_wire_init();

  private:
  ecpState m_state;
  char m_kpaddr;

  int m_receivePin, m_transmitPin;
  SoftwareSerial *vistaSerial;
  Stream *OutputStream;
  AsyncWebSocket *ws;
  volatile unsigned long low_time;
  ademco_display display_data;
#if DEBUG_OUTWIRE
  bool send_keypad = false;
#endif
  char  *cbuf, *outbuf;
  uint8_t idx, outbufIdx;
  uint8_t write_seq;
  void on_status();
  void on_display();
  void write_chars();
  void write_to_device();
  bool have_message();
  char kpaddr_to_bitmask(char kpaddr) { return 0xFF ^ (0x01 << (kpaddr - 16)); }
#if DEBUG_HANDLE
#define DELTABUF_SIZE 128
  unsigned long *delta_buf;
  uint8_t deltaIdx;
  bool printed_delta = false;
#endif
};

#endif
