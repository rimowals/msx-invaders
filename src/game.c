/* game.c - MSX Space Invaders, main logic.
 * SCREEN 2: invaders are name-table tiles moved in 8px steps,
 * player/bullet/bombs/UFO are hardware sprites.
 * Build with -DGAME_LITE for the National CF-2000 profile
 * (fewer bombs, no UFO, no MSX2 palette code). */
#include "msx.h"
#include "vdp.h"
#include "psg.h"
#include "data.h"

/* ---------------- configuration ---------------- */
#define GRID_COLS 8
#define GRID_ROWS 5
#define CELL_W    3            /* tiles per column slot (2 tiles + 1 gap) */

#ifdef GAME_LITE
#define MAX_BOMBS   2
#define BOMB_DELAY  70
#else
#define MAX_BOMBS   3
#define BOMB_DELAY  40
#endif

#define PLAYER_Y    160        /* sprite y; ship pixels at 168..175 */
#define GROUND_ROW  22
#define SHIELD_ROW  17
#define SCORE_ROW   0

/* ---------------- state ---------------- */
static u8 ntbuf[768];          /* name table shadow */
static u8 sat[28];             /* sprite attributes, planes 0..5 + stopper */

static u8 grid[GRID_ROWS][GRID_COLS];
static u8 alive_count;
static s8 gx;                  /* grid origin in tiles (may go negative) */
static u8 gy;
static s8 gdir;
static u8 step_timer, anim_frame, march_note;

static u8 px;                  /* player sprite x */
static u8 dying;               /* death animation countdown */

static u8 bullet_on, bx, by, fire_cd, prev_trig;

static u8 bomb_on[MAX_BOMBS], bomb_x[MAX_BOMBS], bomb_y[MAX_BOMBS];
static u8 bomb_timer;

#ifndef GAME_LITE
static u8 ufo_on, ufo_x;
static u16 ufo_timer;
#endif

static u16 score, hiscore;
static u8 lives, wave, invaded;

static u8 boom_t, boom_tx, boom_ty;   /* transient invader explosion tile */

static u8 rnd;

static const u8 row_chr[GRID_ROWS] =
    { CHR_SQUID1, CHR_CRAB1, CHR_CRAB1, CHR_OCTO1, CHR_OCTO1 };
static const u8 row_score[GRID_ROWS] = { 30, 20, 20, 10, 10 };

/* ---------------- name table helpers ---------------- */
static void nt_putc(u8 x, u8 y, u8 c)
{
    u16 off = (u16)y * 32 + x;
    ntbuf[off] = c;
    vdp_setwr(VRAM_NAME + off);
    vdp_out(c);
}

static void nt_flush_row(u8 y)
{
    u16 off = (u16)y * 32;
    vdp_memcpy(VRAM_NAME + off, &ntbuf[off], 32);
}

static void draw_text(u8 x, u8 y, const char *s)
{
    u16 off = (u16)y * 32 + x;
    u8 n = 0;
    while (s[n]) { ntbuf[off + n] = (u8)s[n]; n++; }
    vdp_memcpy(VRAM_NAME + off, &ntbuf[off], n);
}

static void print_dec5(u8 x, u8 y, u16 v)
{
    char buf[6];
    u8 i;
    for (i = 5; i-- > 0; ) {
        buf[i] = '0' + (v % 10);
        v /= 10;
    }
    buf[5] = 0;
    draw_text(x, y, buf);
}

static void clear_playfield(void)
{
    u8 y;
    u16 i;
    for (i = 32; i < 768; i++) ntbuf[i] = CHR_EMPTY;
    for (y = 1; y < 24; y++) nt_flush_row(y);
}

/* ---------------- HUD ---------------- */
static void draw_hud(void)
{
    draw_text(0, SCORE_ROW, "SCORE       HI       LIVES  ");
    print_dec5(6, SCORE_ROW, score);
    print_dec5(15, SCORE_ROW, hiscore);
    nt_putc(27, SCORE_ROW, '0' + lives);
}

/* ---------------- invaders ---------------- */
static void redraw_rows(u8 y0, u8 y1)
{
    u8 y, c, r, base;
    s8 dy, tx;
    u8 *row;
    if (y0 < 1) y0 = 1;
    if (y1 > 21) y1 = 21;
    for (y = y0; y <= y1; y++) {
        row = &ntbuf[(u16)y * 32];
        for (c = 0; c < 32; c++) row[c] = CHR_EMPTY;
        dy = (s8)(y - gy);
        if (dy >= 0 && dy <= 8 && !(dy & 1)) {
            r = (u8)dy >> 1;
            base = row_chr[r] + (anim_frame ? 2 : 0);
            for (c = 0; c < GRID_COLS; c++) {
                if (grid[r][c]) {
                    tx = gx + (s8)(c * CELL_W);
                    row[(u8)tx] = base;
                    row[(u8)tx + 1] = base + 1;
                }
            }
        }
        if (boom_t && y == boom_ty) {      /* keep the explosion visible */
            row[boom_tx] = CHR_BOOM_L;
            row[boom_tx + 1] = CHR_BOOM_R;
        }
        nt_flush_row(y);
    }
}

static u8 step_period(void)
{
    u8 p = 2 + (alive_count >> 1);
#ifdef GAME_LITE
    p += 2;                       /* a bit more relaxed on MSX1 entry level */
#endif
    if (p > 6 && wave) { p -= wave * 2 > p - 6 ? p - 6 : wave * 2; }
    return p;
}

static void grid_extent(u8 *cmin, u8 *cmax, u8 *rmax)
{
    u8 r, c;
    *cmin = GRID_COLS; *cmax = 0; *rmax = 0;
    for (r = 0; r < GRID_ROWS; r++)
        for (c = 0; c < GRID_COLS; c++)
            if (grid[r][c]) {
                if (c < *cmin) *cmin = c;
                if (c > *cmax) *cmax = c;
                if (r > *rmax) *rmax = r;
            }
}

static void update_invaders(void)
{
    u8 cmin, cmax, rmax, descended = 0;
    if (--step_timer) return;
    step_timer = step_period();

    grid_extent(&cmin, &cmax, &rmax);
    if (gdir > 0) {
        if (gx + (s8)(cmax * CELL_W) + 1 < 31) gx++;
        else { gy++; gdir = -1; descended = 1; }
    } else {
        if (gx + (s8)(cmin * CELL_W) > 0) gx--;
        else { gy++; gdir = 1; descended = 1; }
    }
    anim_frame ^= 1;
    sfx_step(march_note++);

    if (gy + rmax * 2 >= 20) invaded = 1;    /* reached the player line */

    redraw_rows(descended ? gy - 1 : gy, gy + 8);
}

static void kill_invader(u8 r, u8 c)
{
    u8 tx = (u8)(gx + (s8)(c * CELL_W));
    u8 ty = gy + r * 2;
    grid[r][c] = 0;
    alive_count--;
    nt_putc(tx, ty, CHR_BOOM_L);
    nt_putc(tx + 1, ty, CHR_BOOM_R);
    boom_t = 12;
    boom_tx = tx;
    boom_ty = ty;
    score += row_score[r];
    draw_hud();
    sfx_play(SFX_HIT);
}

/* clear the explosion tile once its time is up; the marching grid may
 * already have redrawn those cells, so only wipe what is still a boom */
static void update_boom(void)
{
    u16 off;
    if (!boom_t || --boom_t) return;
    off = (u16)boom_ty * 32 + boom_tx;
    if (ntbuf[off] == CHR_BOOM_L) nt_putc(boom_tx, boom_ty, CHR_EMPTY);
    if (ntbuf[off + 1] == CHR_BOOM_R) nt_putc(boom_tx + 1, boom_ty, CHR_EMPTY);
}

/* ---------------- shields ---------------- */
static void draw_shields(void)
{
    static const u8 sx[4] = { 4, 11, 18, 25 };
    u8 i;
    for (i = 0; i < 4; i++) {
        u8 x = sx[i];
        /* original bunker shape: slanted top corners, notch underneath */
        nt_putc(x,     SHIELD_ROW,     CHR_SHIELD_TL);
        nt_putc(x + 1, SHIELD_ROW,     CHR_SHIELD);
        nt_putc(x + 2, SHIELD_ROW,     CHR_SHIELD_TR);
        nt_putc(x,     SHIELD_ROW + 1, CHR_SHIELD);
        nt_putc(x + 1, SHIELD_ROW + 1, CHR_SHIELD_BC);
        nt_putc(x + 2, SHIELD_ROW + 1, CHR_SHIELD);
    }
}

/* returns TRUE if a shield tile absorbed the hit at pixel (x,y) */
static u8 hit_shield(u8 x, u8 y)
{
    u8 tx = x >> 3, ty = y >> 3;
    u16 off = (u16)ty * 32 + tx;
    u8 c = ntbuf[off];
    if (c >= CHR_SHIELD && c <= CHR_SHIELD_BC) {
        u8 next;
        if (c == CHR_SHIELD_D2) next = CHR_EMPTY;
        else if (c == CHR_SHIELD_D1) next = CHR_SHIELD_D2;
        else next = CHR_SHIELD_D1;   /* full or any shaped tile */
        nt_putc(tx, ty, next);
        return TRUE;
    }
    return FALSE;
}

/* ---------------- player, bullet, bombs, ufo ---------------- */
static void update_player(void)
{
    u8 s = g_stick;
    if (s >= 2 && s <= 4) { if (px < 238) px += 2; }
    else if (s >= 6 && s <= 8) { if (px > 2) px -= 2; }

    if (fire_cd) fire_cd--;
    if (g_trig && !prev_trig && !bullet_on && !fire_cd) {
        bullet_on = TRUE;
        bx = px;
        by = PLAYER_Y + 2;
        fire_cd = 8;
        sfx_play(SFX_SHOOT);
    }
    prev_trig = g_trig;
}

static void update_bullet(void)
{
    u8 tipx, ty;
    s8 dx, dy;
    u8 c, r;
    if (!bullet_on) return;
    if (by < 12) { bullet_on = FALSE; return; }
    by -= 4;
    tipx = bx + 7;

#ifndef GAME_LITE
    if (ufo_on && by <= 18 && by + 5 >= 12 &&
        tipx >= ufo_x && tipx <= ufo_x + 15) {
        ufo_on = FALSE;
        bullet_on = FALSE;
        score += 100;
        draw_hud();
        sfx_play(SFX_UFO_HIT);
        return;
    }
#endif

    if (hit_shield(tipx, by)) { bullet_on = FALSE; return; }

    ty = by >> 3;
    dx = (s8)(tipx >> 3) - gx;
    dy = (s8)ty - (s8)gy;
    if (dx >= 0 && dy >= 0 && !(dy & 1)) {
        c = (u8)dx / CELL_W;
        r = (u8)dy >> 1;
        if (c < GRID_COLS && r < GRID_ROWS &&
            ((u8)dx % CELL_W) < 2 && grid[r][c]) {
            kill_invader(r, c);
            bullet_on = FALSE;
        }
    }
}

static void spawn_bomb(void)
{
    u8 i, t, c, r, low;
    for (i = 0; i < MAX_BOMBS; i++) if (!bomb_on[i]) break;
    if (i == MAX_BOMBS) return;
    for (t = 0; t < 8; t++) {
        c = rnd & 7;
        rnd = rnd * 5 + 0x31;
        low = 0xFF;
        for (r = 0; r < GRID_ROWS; r++) if (grid[r][c]) low = r;
        if (low != 0xFF) {
            u16 sy = (u16)(gy + low * 2 + 1) * 8;
            /* invaders this low would drop a bomb already inside the kill
             * band - it would hit with no visible fall, so skip it */
            if (sy >= 152) return;
            bomb_on[i] = TRUE;
            bomb_x[i] = (u8)((gx + (s8)(c * CELL_W)) * 8) + 2;
            bomb_y[i] = (u8)sy;
            return;
        }
    }
}

static void update_bombs(void)
{
    u8 i, tx, ty;
    if (--bomb_timer == 0) {
        bomb_timer = BOMB_DELAY + (rnd & 31);
        spawn_bomb();
    }
    for (i = 0; i < MAX_BOMBS; i++) {
        if (!bomb_on[i]) continue;
        bomb_y[i] += 2;
        tx = bomb_x[i] + 6;
        ty = bomb_y[i] + 6;
        if (ty >= 176) { bomb_on[i] = FALSE; continue; }     /* ground */
        if (hit_shield(tx, ty)) { bomb_on[i] = FALSE; continue; }
        if (ty >= 168 && tx >= px && tx <= px + 15 && !dying) {
            bomb_on[i] = FALSE;
            dying = 60;
            sfx_play(SFX_EXPLODE);
        }
    }
}

#ifndef GAME_LITE
static void update_ufo(void)
{
    if (ufo_on) {
        ufo_x += 1;
        if (ufo_x >= 240) ufo_on = FALSE;
        return;
    }
    if (--ufo_timer == 0) {
        ufo_timer = 700 + ((u16)(rnd) << 1);
        if (alive_count > 6) { ufo_on = TRUE; ufo_x = 0; }
    }
}
#endif

/* ---------------- sprites ---------------- */
static void set_spr(u8 plane, u8 y, u8 x, u8 pat_idx, u8 color)
{
    u8 *p = &sat[plane << 2];
    p[0] = y - 1;
    p[1] = x;
    p[2] = pat_idx << 2;
    p[3] = color;
}

/* Park a sprite below the screen. Y must NOT be 208 (0xD0): that value
 * terminates sprite processing, which would also hide every later plane.
 * 216 is off-screen and clear of the 225+ wrap-to-top region. */
static void hide_spr(u8 plane)
{
    sat[plane << 2] = 216;
}

static void build_sat(void)
{
    u8 i;
    if (dying)
        set_spr(0, PLAYER_Y, px, SPR_BOOM, 8);
    else
        set_spr(0, PLAYER_Y, px, SPR_PLAYER, 15);

    if (bullet_on) set_spr(1, by, bx, SPR_BULLET, 15);
    else hide_spr(1);

    for (i = 0; i < MAX_BOMBS; i++) {
        if (bomb_on[i]) set_spr(2 + i, bomb_y[i], bomb_x[i], SPR_BOMB, 9);
        else hide_spr(2 + i);
    }
#if MAX_BOMBS < 3
    hide_spr(2 + MAX_BOMBS);
#endif

#ifndef GAME_LITE
    if (ufo_on) set_spr(5, 8, ufo_x, SPR_UFO, 13);
    else hide_spr(5);
#else
    hide_spr(5);
#endif

    sat[6 << 2] = 0xD0;   /* stop sprite processing here */
}

static void flush_sat(void)
{
    vdp_memcpy(VRAM_SAT, sat, 28);
}

/* ---------------- VRAM asset loading ---------------- */
static void load_assets(void)
{
    const u8 *font = *(const u8 * const *)0x0004;   /* BIOS CGTABL */
    u16 bank;
    u8 i, j;

    for (bank = 0; bank < 0x1800; bank += 0x800) {
        /* font chars 32..127 */
        vdp_memcpy(VRAM_PATTERN + bank + 32 * 8, font + 32 * 8, 96 * 8);
        /* custom tiles from 128 up */
        vdp_memcpy(VRAM_PATTERN + bank + 128 * 8, (const u8 *)tiles_pat, NUM_TILES * 8);
        /* colors: white text everywhere, then per-tile colors */
        vdp_fill(VRAM_COLOR + bank, 0xF0, 128 * 8);
        vdp_setwr(VRAM_COLOR + bank + 128 * 8);
        for (i = 0; i < NUM_TILES; i++)
            for (j = 0; j < 8; j++)
                vdp_out(tiles_col[i] << 4);
    }
    vdp_memcpy(VRAM_SPT, (const u8 *)sprites_pat, 5 * 32);
}

/* ---------------- flow ---------------- */
static void frame(void)
{
    wait_vsync();
    flush_sat();          /* early, still in VBLANK */
    sfx_update();
    input_update();
    rnd += *(volatile u8 *)SYS_JIFFY;
    rnd = rnd * 5 + 1;
}

static void pause_frames(u8 n)
{
    while (n--) frame();
}

static void new_wave(void)
{
    u8 r, c, y;
    for (r = 0; r < GRID_ROWS; r++)
        for (c = 0; c < GRID_COLS; c++)
            grid[r][c] = 1;
    alive_count = GRID_ROWS * GRID_COLS;
    gx = 4;
    gy = 3 + (wave > 4 ? 4 : wave);
    gdir = 1;
    anim_frame = 0;
    step_timer = step_period();
    bullet_on = FALSE;
    for (r = 0; r < MAX_BOMBS; r++) bomb_on[r] = FALSE;
    bomb_timer = BOMB_DELAY;
    boom_t = 0;
#ifndef GAME_LITE
    ufo_on = FALSE;
    ufo_timer = 700;
#endif

    clear_playfield();
    for (y = 0; y < 32; y++) nt_putc(y, GROUND_ROW, CHR_GROUND);
    draw_shields();
    redraw_rows(gy, gy + 8);
    draw_hud();
}

static void player_death(void)
{
    u8 i;
    bullet_on = FALSE;
    for (i = 0; i < MAX_BOMBS; i++) bomb_on[i] = FALSE;
    while (dying > 1) { dying--; build_sat(); frame(); }
    dying = 0;
    lives--;
    px = 120;
    draw_hud();
    pause_frames(30);
}

static void game_loop(void)
{
    score = 0;
    lives = 3;
    wave = 0;
    invaded = 0;
    px = 120;
    dying = 0;
    prev_trig = 0xFF;      /* ignore the fire press that started the game */
    march_note = 0;
    new_wave();

    for (;;) {
        frame();
        update_player();
        update_bullet();
        update_bombs();
#ifndef GAME_LITE
        update_ufo();
#endif
        update_invaders();
        update_boom();
        build_sat();

        if (dying) {
            player_death();
            if (!lives) break;
        }
        if (invaded) break;
        if (!alive_count) {
            pause_frames(60);
            wave++;
            new_wave();
        }
    }

    psg_silence();
    if (score > hiscore) hiscore = score;
    draw_text(11, 11, "GAME OVER");
    pause_frames(60);
    while (!g_trig) frame();
}

static void title_screen(void)
{
    clear_playfield();
    draw_hud();
    draw_text(6, 5,  "*** MSX INVADERS ***");
    draw_text(6, 15, "SQUID ...... 30 PTS");
    draw_text(6, 17, "CRAB ....... 20 PTS");
    draw_text(6, 19, "OCTOPUS .... 10 PTS");
    nt_putc(3, 15, CHR_SQUID1); nt_putc(4, 15, CHR_SQUID1 + 1);
    nt_putc(3, 17, CHR_CRAB1);  nt_putc(4, 17, CHR_CRAB1 + 1);
    nt_putc(3, 19, CHR_OCTO1);  nt_putc(4, 19, CHR_OCTO1 + 1);
    draw_text(6, 10, "PRESS SPACE OR FIRE");

    hide_spr(0); hide_spr(1); hide_spr(2); hide_spr(3); hide_spr(4); hide_spr(5);
    sat[6 << 2] = 0xD0;

    while (g_trig) frame();     /* wait for release first */
    while (!g_trig) frame();
}

void main(void)
{
    vdp_init_screen2();
    load_assets();
    vdp_set_palette_msx2();
    psg_init();
    rnd = 0xA5;
    hiscore = 0;
    lives = 3;          /* shown in the HUD on the title screen */
    vdp_display(1);

    for (;;) {
        title_screen();
        game_loop();
    }
}
