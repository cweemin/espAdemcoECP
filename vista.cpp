#include "config.h"
#include "ecpSoftwareSerial.h"
#include "vista.h"
#include "ademcodefines.h"
#include <string.h>
#include <Arduino.h>

// As the Arduino attachInterrupt has no parameter, lists of objects
// and callbacks corresponding to each possible GPIO pins have to be defined
#define MAX_PIN 15
Vista *ObjList[MAX_PIN+1];

void ICACHE_RAM_ATTR vista_isr_0() { ObjList[0]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_1() { ObjList[1]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_2() { ObjList[2]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_3() { ObjList[3]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_4() { ObjList[4]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_5() { ObjList[5]->handleISR(); };
// Pin 6 to 11 can not be used
void ICACHE_RAM_ATTR vista_isr_12() { ObjList[12]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_13() { ObjList[13]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_14() { ObjList[14]->handleISR(); };
void ICACHE_RAM_ATTR vista_isr_15() { ObjList[15]->handleISR(); };

static void (*ISRList[MAX_PIN+1])() = {
  vista_isr_0,
  vista_isr_1,
  vista_isr_2,
  vista_isr_3,
  vista_isr_4,
  vista_isr_5,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  vista_isr_12,
  vista_isr_13,
  vista_isr_14,
  vista_isr_15
};
Vista::Vista(int receivePin, int transmitPin, char kpaddr, Stream *OutStream)
  : m_state(normal), m_kpaddr(kpaddr), m_receivePin(receivePin), OutputStream(OutStream), m_transmitPin(transmitPin), low_time(0), idx(0),outbufIdx(0), write_seq(1)
{
  vistaSerial = new SoftwareSerial(receivePin, transmitPin, true, 64);
  ObjList[receivePin] = this;
  cbuf = (char*) malloc(100);
  if (cbuf != NULL) {
    OutputStream->println("Created buffer");
    outbuf = (char*) malloc(20);
    if (outbuf== NULL) {
      OutputStream->println("Error creating input buffer");
    }
  }
  else
    OutputStream->println("Error creating buffer, try rebooting?");
}


Vista::~Vista() {
  free(vistaSerial);
  detachInterrupt(m_receivePin);
  ObjList[m_receivePin] = NULL;
  free(cbuf);
}

void ICACHE_RAM_ATTR Vista::handleISR() {
  if (digitalRead(m_receivePin)) {
    switch (m_state) {
      case ack_f7:
        vistaSerial->setParity(false);
        vistaSerial->write(0xFF);
        m_state = pulse;
        break;
      case pulse:
        vistaSerial->write(0xFF);
        m_state = send_kp;
        break;
      case send_kp:
        vistaSerial->write(kpaddr_to_bitmask(m_kpaddr));
        vistaSerial->setParity(true);
        m_state = normal;
        low_time = 0;
        send_keypad = true;
        break;
      case normal:
        //High
        if (low_time && (millis() - low_time)  > 9) {
          if (have_message()) {
            vistaSerial->setParity(false);
            vistaSerial->write(0xFF);
            m_state = pulse;
          }
          return;
        }
        vistaSerial->rxRead();
        low_time = 0;
        break;
      default:
        //Not suppose to be here
        break;
    }
  //Low
  } else {
    low_time = millis();
  }
  // Must clear this bit in the interrupt register,
  // it gets set even when interrupts are disabled
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, 1 << m_receivePin);
}

void Vista::handle() {
  uint8_t x = 0;
  uint8_t len = 0;
#if DEBUG_OUTWIRE
  if (send_keypad) {
    send_keypad = false;
    OutputStream->print(".");
  }
#endif
  if (vistaSerial->available()) {
    x = vistaSerial->read();
    if (x == 0xF7) {
      memset(cbuf, 0, sizeof(cbuf));
      idx = 0;
      cbuf[idx++] = x;
      len = F7_MESSAGE_LENGTH;
      while(idx<len) {
        if(vistaSerial->available())
          cbuf[idx++] = vistaSerial->read();
        yield(); //Don't starve cpu
      }

      // start pulsing our way out
      m_state = ack_f7;

      on_display();

    } else if ( x == 0xF2) {
      memset(cbuf, 0, sizeof(cbuf));
      idx = 0;
      cbuf[ idx++ ] = x;
      // Wait for next byte to come
      while (!vistaSerial->available()) { yield();}
      cbuf[idx++] = vistaSerial->read();
      len = cbuf[idx-1] + 2;
      while(idx<len) {
        if(vistaSerial->available())
          cbuf[idx++] = vistaSerial->read();
        yield(); //Don't starve cpu
      }
      on_status();
    } else if (x == 0xF6) {
      memset(cbuf, 0, sizeof(cbuf));
      idx = 0;
      cbuf[idx++] = x;
      //Single char read
      while (!vistaSerial->available()) { yield();}
      cbuf[idx++] = vistaSerial->read();
      m_state = midAck;
      vistaSerial->setParity(true);
      write_to_device();
      m_state = normal;
      vistaSerial->setParity(false);

    }else{
      if (x) {
        OutputStream->print("Unknown byte: ");
        OutputStream->println(x, HEX);
      }
    }
  }
}

void Vista::stop() {
  detachInterrupt(m_receivePin);
}
void Vista::begin() {
  vistaSerial->begin(4800);
  //Disable interrupt so we can interrupt ourself
  //vistaSerial->enableRx(false);
  attachInterrupt(m_receivePin, ISRList[m_receivePin], CHANGE);
}

void Vista::write_to_device() {

#if DEBUG_KEYS
  OutputStream->print("F6: kp ack addr = ");
  OutputStream->println(cbuf[1], DEC);
#endif
  delay(200);
  if (cbuf[1] == m_kpaddr) {
    delay(600);
    write_chars();
    out_wire_init();

  }
}
void Vista::on_display() {
#if DEBUG_OUTWIRE
  if (have_message()) {
    OutputStream->print("Still have message queued: ");
    for(uint8_t i =0; i < outbufIdx; i++)
      OutputStream->print(outbuf[i]);
    OutputStream->println("");
  }
#endif
  String tStr = "";
  // first 4 bytes are addresses of intended keypads to display this message
  // from left to right MSB to LSB
  // 5th byte represent zone
  // 6th binary encoded data including beeps
  // 7th binary encoded data including status armed mode
  // 8th binary encoded data including ac power and chime
  // 9th byte Programming mode = 0x01
  // 10th byte promt position in the display message of the expected input
#if DEBUG_DISPLAY
  OutputStream->print("F7: {");
  for (int x = 1; x <= 11 ; x++) {
    OutputStream->print( cbuf[x], HEX);
    OutputStream->print(",");
  }

  OutputStream->print (" chksm: ");
  OutputStream->print( cbuf[idx-1], HEX );
  OutputStream->println ("}");

  //12-end is the body
  OutputStream->print("F7: ");
  for (int x = 12; x < idx -2; x++) {
    OutputStream->print ( cbuf[x] );
    OutputStream->print (": ");
    OutputStream->print ( cbuf[x], HEX);
    OutputStream->print (", ");
  }
  OutputStream->print ( cbuf[idx-2] );
  OutputStream->print (":");
  OutputStream->print ( cbuf[idx-2], HEX);
  OutputStream->println();
#endif

  // print out message as JSON
  tStr="{\"type\":\"display\"";
  for (int x = 0; x <= 10 ; x++) {
    switch ( x ) {
      case 3:
        tStr+=", \"addr"+String(x)+ "\": " + String(cbuf[x], HEX);
        display_data.keypad = cbuf[x];
        if (cbuf[x] & 0x02) {
          tStr+=", \"kp17\": \"active\"";
        }
        if (cbuf[x] & 0x04) {
          tStr+=", \"kp18\": \"active\"";
        }
        if (cbuf[x] & 0x08) {
          tStr+=", \"kp19\": \"active\"";
        }
        if (cbuf[x] & 0x10) {
          tStr+=", \"kp20\": \"active\"";
        }
        if (cbuf[x] & 0x20) {
          tStr+=", \"kp21\": \"active\"";
        }
        if (cbuf[x] & 0x40) {
          tStr+=", \"kp22\": \"active\"";
        }
        if (cbuf[x] & 0x80) {
          tStr+=", \"kp23\": \"active\"";
        }
        break;
      case 1:
      case 2:
      case 4:
        tStr+=", \"addr"+String(x)+ "\": " + String(cbuf[x], HEX);
        break;
      case 5:
        display_data.zone = cbuf[x];
        tStr+=", \"zone\": " + String(cbuf[x], HEX);
        break;
      case 6:
        display_data.status.bit.beep = (cbuf[x] & BIT_MASK_BYTE1_BEEP) > 0;
        tStr+=", \"beep\": " + String(cbuf[x] & BIT_MASK_BYTE1_BEEP);
        break;
      case 7:
        display_data.status.bit.armed_stay = ((cbuf[x] & BIT_MASK_BYTE2_ARMED_HOME) > 0);
        tStr+=", \"ARMED_STAY\": " + String(cbuf[x] & BIT_MASK_BYTE2_ARMED_HOME);
        display_data.status.bit.ready = ((cbuf[x] & BIT_MASK_BYTE2_READY) > 0);
        tStr+=", \"READY\": " + String(cbuf[x] & BIT_MASK_BYTE2_READY);
        break;
      case 8:
        display_data.status.bit.chime = ((cbuf[x] & BIT_MASK_BYTE3_CHIME_MODE) > 0);
        tStr+=", \"chime\": " + String(cbuf[x] & BIT_MASK_BYTE3_CHIME_MODE);
        display_data.status.bit.ac_power = ((cbuf[x] & BIT_MASK_BYTE3_AC_POWER) > 0);
        tStr+=", \"ac_power\": " + String(cbuf[x] & BIT_MASK_BYTE3_AC_POWER);
        display_data.status.bit.armed_away = ((cbuf[x] & BIT_MASK_BYTE3_ARMED_AWAY) > 0);
        tStr+=", \"ARMED_AWAY\": " + String(cbuf[x] & BIT_MASK_BYTE3_ARMED_AWAY);
        break;
        /*
           case 9:
        //  OutputStream->print(", \"programming_mode\": \"");
        if ( cbuf[x] == 0x01 ) {
        print_hex( cbuf[x], 8);
        } else {
        //  OutputStream->print("0");
        }
        //  OutputStream->print("\"");
        break;
         */
      case 10:
        if ( cbuf[x] != 0x00 ) {
          //  OutputStream->print(",\"prompt_pos\": \"");
          //  OutputStream->print( (int)cbuf[x] );
          //  OutputStream->print( cbuf[x], HEX);
          //  OutputStream->print("\"");
        }

        break;
      default:
        break;
    }
  }
  tStr+=", \"msg\": \"";
  String trail = "";
  for (int x = 12; x < idx -1; x++) {
    if ((int)cbuf[x] < 32 || (int)cbuf[x] > 126) {
      // Replace unprintable char with a space_bar
      trail += ' ';

    } else {
      //OutputStream->print(cbuf[x]);
      trail += (char) cbuf[x];
    }
    //TODO: fix timing
    //this is because there's a timing bug and we get an extra
    //1 bit in the most significant position
    //	if (cbuf[x] & 0x80) {
    //		cbuf[x] = cbuf[x] ^ 0x80;
    //		OutputStream->print( cbuf[x] );
    //	}

  }
  trail.trim();
  tStr+=trail;
  tStr+="\"}";

  OutputStream->println(tStr);
#ifdef HTTP_SERVER
  ws->textAll( tStr );
#endif

  //	#ifdef DEBUG_DISPLAY
  //	debug_cbuf(cbuf, idx, false);
  //	#endif
}

void Vista::on_status() {
  String tStr = "";

#if DEBUG_STATUS
  OutputStream->print("F2: {");

  //first 6 bytes are headers
  for (int x = 1; x < 7 ; x++) {
    if (x > 1)
      OutputStream->print(",");
    OutputStream->print( cbuf[x], HEX);
  }
  //7th byte is incremental counter
  OutputStream->print (" cnt: ");
  OutputStream->print( cbuf[7], HEX );
  OutputStream->println ("}");

  //8-end is body
  OutputStream->print("F2: {");
  for (int x = 8; x < idx ; x++) {
    OutputStream->print( cbuf[x], HEX);
    OutputStream->print(",");
  }

  OutputStream->println("}");
#endif

  //F2 messages with 18 bytes or less don't seem to have
  // any important information
  if ( 19 >  cbuf[1]) {
#if DEBUG_STATUS
    OutputStream->println("F2: Unknown message - too short");
#endif
    return;
  }

  //print as JSON
  tStr = "{\"type\":\"status\"";
#if DEBUG_STATUS
  //19, 20, 21, 22
  tStr += "\"byte\": [";
  tStr += String(cbuf[19], HEX);
  tStr += "," + String(cbuf[20], HEX);
  tStr += "," + String(cbuf[21], HEX);
  tStr += "," + String(cbuf[22], HEX) + "]";
#endif


  //19th spot is 01 for disarmed, 02 for armed
  uint8_t armed = (0x02 & cbuf[19]) && !(cbuf[19] & 0x01);
  //uint8_t armed = 0x02 & cbuf[19];

  //20th spot is away / stay
  // this bit is really confusing
  // it clearly switches to 2 when you set away mode
  // but it is also 0x02 when an alarm is canceled,
  // but not cleared - even if you are in stay mode.
  uint8_t away = 0x02 & cbuf[20];

  //21st spot is for bypass
  uint8_t bypass = 0x02 & cbuf[21];

  //22nd spot is for alarm types
  //1 is no alarm
  //2 is ignore faults (like exit delay)
  //4 is a alarm
  //6 is a fault that does not cause an alarm
  //8 is for panic alarm.
  uint8_t no_alarm = (cbuf[22] & 0x01);
  uint8_t exit_delay = (cbuf[22] & 0x02);
  uint8_t fault      = (cbuf[22] & 0x04);
  uint8_t panic     = (cbuf[22] & 0x08);


  tStr += ",\"armed\": ";
  if (armed) {
    tStr += "\"yes\"";
  } else {
    tStr += "\"no\"";
  }
  tStr += ", \"mode\": ";
  if (away) {
    tStr += "\"away\"";
  } else {
    tStr += "\"stay\"";
  }
  tStr += ", \"ignore_faults\": ";
  if (exit_delay) {
    tStr += "\"yes\"";
  } else {
    tStr += "\"no\"";
  }
  tStr += ", \"faulted\": ";
  if (fault) {
    tStr += "\"yes\"";
  } else {
    tStr += "\"no\"";
  }
  tStr += ", \"panic\": ";
  if (panic) {
    tStr += "\"yes\"";
  } else {
    tStr += "\"no\"";
  }

  tStr += "}";

  OutputStream->println(tStr);
#ifdef HTTP_SERVER
  ws->textAll( tStr );
#endif
  if ( armed && fault && !exit_delay ) {
    //on_alarm();
    tStr = "{\"type\": \"alarm\"}";
    //save gcbuf for debugging
    //strncpy(alarm_buf[0],  cbuf, 30);
  } else if ( !armed  && fault  && away && !exit_delay) {
    //away bit always flips to 0x02 when alarm is canceled
    tStr = "{\"type\": \"cancel\"}";
  } else {
    //OutputStream->println ("F2: no alarm");
  }
  OutputStream->println(tStr);
#ifdef HTTP_SERVER
  ws->textAll( tStr );
#endif

}

bool ICACHE_RAM_ATTR Vista::have_message() {
  return (outbufIdx > 0);
}

void Vista::out_wire_queue(char byt ) {
  outbuf[outbufIdx++] = byt; // Save the data in a character array
}

void Vista::out_wire_init() {
  //clear outbuf buffer
  memset(outbuf,0,sizeof(outbuf));
  outbufIdx = 0;
}


void Vista::write_chars(){

  if (outbufIdx == 0) {return;}

  int header = ((++write_seq << 6) & 0xc0) | (m_kpaddr & 0x3F);


  //header is the bit mask YYXX-XXXX
  //	where YY is an incrementing sequence number
  //	and XX-XXXX is the keypad address (decimal 16-31)
  //int header = KPADDR & 0x3F;

  vistaSerial->write((int)header);

  vistaSerial->write((int) outbufIdx +1);

  /*
     print_hex((int)header, 8);
     Serial.println();
   */
  //Serial.println(outbufIdx, DEC);
  //adjust characters to hex values.
  //ASCII numbers get translated to hex numbers
  //# and * get converted to 0xA and 0xB
  // send any other chars straight, although this will probably
  // result in errors
  int checksum = 0;
  for(int x =0; x < outbufIdx; x++) {
    //translate digits between 0-9 to hex/decimal
    if (outbuf[x] >= 0x30 && outbuf[x] <= 0x39) {
      outbuf[x] -= 0x30;
      checksum += outbuf[x];
    }
    //translate * to 0x0b
    if (outbuf[x] == 0x23) {
      outbuf[x] = 0x0B;
      checksum += outbuf[x];
    }
    //translate # to 0x0a
    if (outbuf[x] == 0x2A) {
      outbuf[x] = 0x0A;
      checksum += outbuf[x];
    }
    //translate A to 0x1C (function key A)
    //translate B to 0x1D (function key B)
    //translate C to 0x1E (function key C)
    //translate D to 0x1F (function key D)
    if (outbuf[x] >= 0x41 && outbuf[x] <= 0x44) {
      outbuf[x] = outbuf[x] - 0x25;
      checksum += outbuf[x];
    }
    vistaSerial->write(outbuf[x]);
  }

  int chksum = 0x100 - (header + checksum + (int)outbufIdx+1);

  //vistaSerial.write((int) (0x100-(header+checksum+ outbufIdx+1)) );
  vistaSerial->write(chksum);

#if DEBUG_KEYS
  OutputStream->print("Packet Header: ");
  OutputStream->println(header, HEX);

  OutputStream->print("Packet: ");
  for(uint8_t x =0; x < outbufIdx; x++) {
    OutputStream->print(outbuf[x], HEX);
  }
  OutputStream->println();

  OutputStream->print("Checksum: ");
  OutputStream->println(chksum, HEX);
#endif
}
