/* vdp.c - TMS9918/V9938 access via I/O ports 0x98/0x99.
 * Address setup is wrapped in DI/EI because the BIOS ISR reads the VDP
 * status port, which resets the control-port write latch.
 * Plain C loops naturally satisfy the ~29 T-state VRAM access spacing
 * required by the TMS9918 outside VBLANK in SCREEN 2. */
#include "vdp.h"

static __sfr __at(0x98) VDP_DATA;
static __sfr __at(0x99) VDP_CTRL;
static __sfr __at(0x9A) VDP_PAL;   /* V9938 palette port (MSX2 only) */

static u8 reg1_shadow;

void vdp_reg(u8 reg, u8 val)
{
    __asm__("di");
    VDP_CTRL = val;
    VDP_CTRL = 0x80 | reg;
    __asm__("ei");
    if (reg == 1) reg1_shadow = val;
}

void vdp_setwr(u16 addr)
{
    __asm__("di");
    VDP_CTRL = (u8)addr;
    VDP_CTRL = (u8)(addr >> 8) | 0x40;
    __asm__("ei");
}

void vdp_out(u8 v)
{
    VDP_DATA = v;
}

void vdp_write(const u8 *src, u16 len)
{
    while (len--) VDP_DATA = *src++;
}

void vdp_fill(u16 addr, u8 val, u16 len)
{
    vdp_setwr(addr);
    while (len--) VDP_DATA = val;
}

void vdp_memcpy(u16 addr, const u8 *src, u16 len)
{
    vdp_setwr(addr);
    vdp_write(src, len);
}

void vdp_display(u8 on)
{
    u8 v = reg1_shadow;
    if (on) v |= 0x40; else v &= (u8)~0x40;
    vdp_reg(1, v);
}

void vdp_init_screen2(void)
{
    vdp_reg(0, 0x02);   /* M3: SCREEN 2 */
    vdp_reg(1, 0xA2);   /* 16K, display OFF, VBLANK int on, 16x16 sprites */
    vdp_reg(2, 0x06);   /* name table   0x1800 */
    vdp_reg(3, 0xFF);   /* color table  0x2000, full 3 banks */
    vdp_reg(4, 0x03);   /* pattern tbl  0x0000, full 3 banks */
    vdp_reg(5, 0x36);   /* sprite attr  0x1B00 */
    vdp_reg(6, 0x07);   /* sprite pat   0x3800 */
    vdp_reg(7, 0x01);   /* black backdrop */

    vdp_fill(0x0000, 0x00, 0x4000);  /* wipe all VRAM */
}

#ifndef GAME_LITE
/* Arcade-ish palette for the colors this game uses; V9938 only.
 * Entries are (R<<4|B), G with 3 bits per gun. */
static const u8 pal_msx2[32] = {
    0x00,0x00,  /* 0 transparent */
    0x00,0x00,  /* 1 black */
    0x11,0x06,  /* 2 medium green */
    0x33,0x07,  /* 3 light green */
    0x17,0x01,  /* 4 dark blue */
    0x27,0x03,  /* 5 light blue */
    0x51,0x01,  /* 6 dark red */
    0x27,0x06,  /* 7 cyan */
    0x71,0x01,  /* 8 medium red */
    0x73,0x03,  /* 9 light red */
    0x61,0x06,  /* 10 dark yellow */
    0x63,0x07,  /* 11 light yellow */
    0x11,0x04,  /* 12 dark green */
    0x65,0x02,  /* 13 magenta */
    0x55,0x05,  /* 14 gray */
    0x77,0x07   /* 15 white */
};
#endif

void vdp_set_palette_msx2(void)
{
#ifndef GAME_LITE
    u8 i;
    if (msx_version() == 0) return;   /* MSX1: no palette registers */
    __asm__("di");
    VDP_CTRL = 0;                     /* palette pointer = 0 */
    VDP_CTRL = 0x80 | 16;
    for (i = 0; i < 32; i++) VDP_PAL = pal_msx2[i];
    __asm__("ei");
#endif
}
