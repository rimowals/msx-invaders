/* msx.h - common defs for MSX Space Invaders */
#ifndef MSX_H
#define MSX_H

#include <stdint.h>

/* BIOS entry points (main ROM in page 0 while our cart runs in page 1/2) */
#define BIOS_GTSTCK 0x00D5
#define BIOS_GTTRIG 0x00D8

/* System RAM */
#define SYS_JIFFY   0xFC9E  /* frame counter incremented by BIOS ISR */
#define ROM_MSXVER  0x002D  /* 0=MSX1, 1=MSX2, 2=MSX2+, 3=turboR */

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef int16_t  s16;

#define TRUE  1
#define FALSE 0

/* input.c */
extern volatile u8 g_stick;   /* 0=center, 1..8 = N,NE,E,SE,S,SW,W,NW */
extern volatile u8 g_trig;    /* 0xFF = fire pressed */
void input_update(void);
void wait_vsync(void);
u8   msx_version(void);

#endif
