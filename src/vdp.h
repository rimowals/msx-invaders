/* vdp.h - TMS9918/V9938 low level, SCREEN 2 layout */
#ifndef VDP_H
#define VDP_H

#include "msx.h"

/* SCREEN 2 VRAM layout (BIOS-standard) */
#define VRAM_PATTERN 0x0000  /* 3 banks x 0x800 */
#define VRAM_NAME    0x1800  /* 768 bytes */
#define VRAM_SAT     0x1B00  /* sprite attributes, 32*4 */
#define VRAM_COLOR   0x2000  /* 3 banks x 0x800 */
#define VRAM_SPT     0x3800  /* sprite patterns */

void vdp_reg(u8 reg, u8 val);
void vdp_setwr(u16 addr);            /* start VRAM write at addr */
void vdp_out(u8 v);                  /* write one byte at current addr */
void vdp_write(const u8 *src, u16 len);
void vdp_fill(u16 addr, u8 val, u16 len);
void vdp_memcpy(u16 addr, const u8 *src, u16 len);

void vdp_init_screen2(void);         /* set mode, clear VRAM, screen off */
void vdp_display(u8 on);
void vdp_set_palette_msx2(void);     /* no-op unless running on MSX2+ */

#endif
