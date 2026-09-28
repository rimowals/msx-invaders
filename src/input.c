/* input.c - keyboard via direct PPI matrix read + joystick via BIOS.
 * Keyboard row 8 holds space (bit0) and cursors (bit4 left, bit7 right);
 * reading the matrix directly works on every MSX and on C-BIOS, whose
 * BIOS keyboard routines are incomplete. Joystick port 1 goes through
 * BIOS GTSTCK/GTTRIG. ASM wrappers take no C parameters so they are
 * independent of the SDCC calling convention. */
#include "msx.h"

volatile u8 g_stick;
volatile u8 g_trig;

void input_update(void) __naked
{
__asm
    PPI_C  = 0xAA           ; keyboard row select (low nibble)
    PPI_B  = 0xA9           ; keyboard column read

    ; --- read matrix row 8 atomically (BIOS ISR also selects rows) ---
    di
    in      a, (PPI_C)
    and     a, #0xF0
    or      a, #0x08
    out     (PPI_C), a
    in      a, (PPI_B)
    ei
    cpl                     ; now 1 = pressed
    ld      b, a

    ; trigger: space = bit0
    xor     a, a
    bit     0, b
    jr      z, 00151$
    dec     a               ; 0xFF = pressed
00151$:
    ld      (_g_trig), a

    ; direction: bit7 = right, bit4 = left
    xor     a, a
    bit     7, b
    jr      z, 00152$
    ld      a, #3           ; east
    jr      00154$
00152$:
    bit     4, b
    jr      z, 00153$
    ld      a, #7           ; west
    jr      00154$
00153$:
    ld      a, #1           ; keyboard idle -> BIOS GTSTCK(joystick 1)
    call    0x00D5
00154$:
    ld      (_g_stick), a

    ; no space pressed -> also check joystick 1 trigger
    ld      a, (_g_trig)
    or      a, a
    jr      nz, 00155$
    ld      a, #1
    call    0x00D8          ; GTTRIG(1)
    ld      (_g_trig), a
00155$:
    ret
__endasm;
}

/* Wait for next VBLANK interrupt (BIOS ISR keeps running). */
void wait_vsync(void) __naked
{
__asm
    ei
    halt
    ret
__endasm;
}

/* MSX version byte from main BIOS ROM (page 0 is main ROM for a cart). */
u8 msx_version(void)
{
    return *(volatile const u8 *)ROM_MSXVER;
}

