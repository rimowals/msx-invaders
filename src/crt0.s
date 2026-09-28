; crt0.s - MSX cartridge ROM startup (page 1, 0x4000)
; Plain "AB" ROM, works on MSX1/MSX2/MSX2+ (Sony HB-F1XD, Omega, National CF-2000).
; Stack below system work area, data in page 3 RAM (0xC000+, fits 16KB machines).

    .module crt0
    .globl  _main
    .globl  l__INITIALIZER
    .globl  s__INITIALIZED
    .globl  s__INITIALIZER
    .globl  l__DATA
    .globl  s__DATA

    .area   _HEADER (ABS)
    .org    0x4000
    .db     0x41                ; 'A'
    .db     0x42                ; 'B'
    .dw     init                ; INIT
    .dw     0x0000              ; STATEMENT
    .dw     0x0000              ; DEVICE
    .dw     0x0000              ; TEXT
    .dw     0x0000, 0x0000, 0x0000  ; reserved

    .area   _CODE
init:
    di
    ld      sp, #0xF380         ; top of free RAM, below BIOS work area

    ; zero uninitialized data (_DATA)
    ld      bc, #l__DATA
    ld      a, b
    or      a, c
    jr      z, zdone
    ld      hl, #s__DATA
    ld      (hl), #0x00
    dec     bc
    ld      a, b
    or      a, c
    jr      z, zdone
    ld      d, h
    ld      e, l
    inc     de
    ldir
zdone:
    call    gsinit              ; copy initialized data ROM -> RAM
    ei
    call    _main
lock:
    halt
    jr      lock

    .area   _HOME

    .area   _GSINIT
gsinit:
    ld      bc, #l__INITIALIZER
    ld      a, b
    or      a, c
    jr      z, gsdone
    ld      de, #s__INITIALIZED
    ld      hl, #s__INITIALIZER
    ldir
gsdone:
    .area   _GSFINAL
    ret

    ; ROM copy of initialized data lives after code
    .area   _INITIALIZER

    ; RAM areas (placed at --data-loc)
    .area   _DATA
    .area   _INITIALIZED
    .area   _BSEG
    .area   _BSS
    .area   _HEAP
