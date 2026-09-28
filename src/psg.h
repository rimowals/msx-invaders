/* psg.h - AY-3-8910 sound effects */
#ifndef PSG_H
#define PSG_H

#include "msx.h"

enum {
    SFX_NONE = 0,
    SFX_SHOOT,      /* player shot: rising pitch, channel A */
    SFX_HIT,        /* invader destroyed: short noise, channel C */
    SFX_EXPLODE,    /* player death: long noise, channel C */
    SFX_UFO_HIT     /* bonus: two-tone, channel A */
};

void psg_init(void);
void psg_silence(void);
void sfx_play(u8 id);
void sfx_step(u8 note);   /* invader march tick, note 0..3, channel B */
void sfx_update(void);    /* call once per frame */

#endif
