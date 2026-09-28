/* data.h - graphics data */
#ifndef DATA_H
#define DATA_H

#include "msx.h"

/* character codes of custom tiles (same in all 3 SCREEN2 banks) */
#define CHR_EMPTY     0
#define CHR_SQUID1    128   /* +1 = right half; frames are +0/+2 */
#define CHR_SQUID2    130
#define CHR_CRAB1     132
#define CHR_CRAB2     134
#define CHR_OCTO1     136
#define CHR_OCTO2     138
#define CHR_SHIELD    140   /* 140 full, 141 damaged, 142 nearly gone */
#define CHR_SHIELD_D1 141
#define CHR_SHIELD_D2 142
#define CHR_SHIELD_TL 143   /* top-left corner, slanted like the original */
#define CHR_SHIELD_TR 144   /* top-right corner */
#define CHR_SHIELD_BC 145   /* bottom-center notch (must end shield range) */
#define CHR_GROUND    146
#define CHR_BOOM_L    147   /* transient invader explosion, left half */
#define CHR_BOOM_R    148

#define NUM_TILES     21

/* 16x16 sprite pattern indices (pattern number = index * 4) */
#define SPR_PLAYER  0
#define SPR_BULLET  1
#define SPR_BOMB    2
#define SPR_UFO     3
#define SPR_BOOM    4

extern const u8 tiles_pat[NUM_TILES][8];  /* chars 128.. patterns */
extern const u8 tiles_col[NUM_TILES];     /* fg color per char (bg=0) */
extern const u8 sprites_pat[5][32];  /* 16x16 sprite patterns */

#endif
