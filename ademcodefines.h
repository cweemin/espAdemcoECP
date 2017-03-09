#ifndef __ADEMCODEFINES_H_
#define __ADEMCODEFINES_H_
// F7 messages default length
#define  F7_MESSAGE_LENGTH  45

// Used to read bits on F7 message
#define BIT_MASK_BYTE1_BEEP 0x03

#define BIT_MASK_BYTE2_ARMED_HOME 0x80
#define BIT_MASK_BYTE2_READY 0x10

#define BIT_MASK_BYTE3_CHIME_MODE 0x20
#define BIT_MASK_BYTE3_AC_POWER 0x08
#define BIT_MASK_BYTE3_ARMED_AWAY 0x04

#endif
