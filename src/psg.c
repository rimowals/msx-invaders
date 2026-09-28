/* psg.c - AY-3-8910 via ports 0xA0/0xA1.
 * Register 7: bit7 must be 1, bit6 must be 0 on MSX (I/O port direction),
 * otherwise joystick reading breaks on some machines. */
#include "psg.h"

static __sfr __at(0xA0) PSG_ADDR;
static __sfr __at(0xA1) PSG_DATA;

static void psg_wr(u8 reg, u8 val)
{
    __asm__("di");
    PSG_ADDR = reg;
    PSG_DATA = val;
    __asm__("ei");
}

/* mixer: tone A+B on, noise C on, rest off -> disable toneC,noiseA,noiseB */
#define MIXER_VAL (0x80 | 0x1C)

void psg_init(void)
{
    psg_wr(7, MIXER_VAL);
    psg_wr(8, 0);   /* vol A */
    psg_wr(9, 0);   /* vol B */
    psg_wr(10, 0);  /* vol C */
    psg_wr(6, 12);  /* noise period */
}

void psg_silence(void)
{
    psg_wr(8, 0);
    psg_wr(9, 0);
    psg_wr(10, 0);
}

/* --- effect state machines, advanced once per frame --- */

static u8  fxA_id, fxA_t;      /* channel A: shoot / ufo hit */
static u16 fxA_period;
static u8  fxC_t, fxC_vol;     /* channel C: noise bursts */
static u8  fxB_t;              /* channel B: march note timer */

void sfx_play(u8 id)
{
    switch (id) {
    case SFX_SHOOT:
        fxA_id = SFX_SHOOT;
        fxA_t = 12;
        fxA_period = 0x0180;
        break;
    case SFX_UFO_HIT:
        fxA_id = SFX_UFO_HIT;
        fxA_t = 24;
        fxA_period = 0x0080;
        break;
    case SFX_HIT:
        fxC_t = 6;
        fxC_vol = 13;
        break;
    case SFX_EXPLODE:
        fxC_t = 30;
        fxC_vol = 15;
        break;
    }
}

/* four-note bass loop, roughly the classic thump */
static const u16 march_notes[4] = { 0x0680, 0x05E0, 0x0540, 0x04C0 };

void sfx_step(u8 note)
{
    u16 p = march_notes[note & 3];
    psg_wr(2, (u8)p);
    psg_wr(3, (u8)(p >> 8));
    psg_wr(9, 11);
    fxB_t = 4;
}

void sfx_update(void)
{
    /* channel A */
    if (fxA_t) {
        fxA_t--;
        if (fxA_id == SFX_SHOOT) {
            fxA_period -= 0x18;              /* pitch up */
        } else {                             /* ufo hit: warble */
            fxA_period ^= 0x0020;
        }
        psg_wr(0, (u8)fxA_period);
        psg_wr(1, (u8)(fxA_period >> 8));
        psg_wr(8, fxA_t > 10 ? 12 : fxA_t);
        if (!fxA_t) psg_wr(8, 0);
    }
    /* channel B (march decay) */
    if (fxB_t) {
        fxB_t--;
        if (!fxB_t) psg_wr(9, 0);
    }
    /* channel C (noise) */
    if (fxC_t) {
        fxC_t--;
        if (fxC_vol) fxC_vol--;
        psg_wr(10, fxC_vol);
        if (!fxC_t) psg_wr(10, 0);
    }
}
