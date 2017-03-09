#ifndef _DATA_H
#define _DATA_H

struct ademco_display {
  union {
    struct {
      uint8_t beep:1;
      uint8_t armed_stay:1;
      uint8_t armed_away:1;
      uint8_t chime:1;
      uint8_t ac_power:1;
      uint8_t ready:1;
      uint8_t unused:2; 
    } bit;
    char byte;
  } status;
  uint8_t keypad;
  uint8_t zone;
};

#endif
