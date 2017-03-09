#ifndef __VISTA_H_
#define __VISTA_H_

#include "ecpSoftwareSerial.h"
#include <Stream.h>
#include "ademcodata.h"

#define DEBUG_STATUS 0
#define DEBUG_DISPLAY 0
#define DEBUG_KEYS 1
#define DEBUG_HANDLE 0
enum ecpState { midAck, poll, normal, ack_f7 }; 

class Vista {
  public:
  Vista(int receivePin, int transmitPin, char kpaddr, Stream *OutStream);
  ~Vista();
  void begin();
  void stop();
  void handle();
  void handleISR();
  void setStream(Stream* tmp) { OutputStream = tmp;}
  void setkpddr(char tmp) { m_kpaddr = tmp; }
  void out_wire_queue(char byt);
  void out_wire_init();

  private:
  ecpState m_state;
  char m_kpaddr;
  int m_receivePin, m_transmitPin;
  SoftwareSerial *vistaSerial;
  Stream *OutputStream;
  volatile unsigned long low_time;
  volatile uint8_t pulse_seq;
  void on_status();
  void on_display();
  void write_chars();
  void write_to_device();
  bool have_message();
  void handle_pulse();
  uint8_t kpaddr_to_bitmask(int kpaddr) { return 0xFF ^ (0x01 << (kpaddr - 16)); }
  ademco_display display_data;
  char  *cbuf, *outbuf;
  uint8_t idx, outbufIdx;
  uint8_t write_seq;
#if DEBUG_HANDLE
#define DELTABUF_SIZE 128 
  unsigned long *delta_buf;
  uint8_t deltaIdx;
  bool printed_delta = false;
#endif
};

#endif
