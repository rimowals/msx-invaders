# MSX Space Invaders

Klon Space Invaders dla MSX, pisany w C (SDCC) ze wstawkami ASM (Z80).

## Platformy docelowe

|Maszyna|Typ|Wersja ROM|
|-|-|-|
|Sony HB-F1XD|MSX2|`invaders.rom` (paleta V9938 ustawiana w runtime)|
|MSX Omega|MSX2+|`invaders.rom`|
|National CF-2000|MSX1, 16 KB RAM|`invaders-cf2000.rom` (lite) — `invaders.rom` też powinien działać|

Gra to zwykły ROM cartridge ("AB", strona 0x4000), więc ładuje się z:

* **MSX SD Mapper 512KB** — przez SofaRun / ExecROM (`sr invaders.rom` lub ExecROM z menu),
* **Carnivore2** — wgranie ROM-u do flash/RAM przez menu Carnivore lub SofaRun,
* emulatorów (openMSX, blueMSX): jako cartridge typu *plain ROM*.

## Budowanie

Wymagania: **SDCC >= 4.2** (w PATH: `sdcc`, `sdasz80`, `makebin`).

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

Wyniki w `bin\`:

* `invaders.rom` — pełna wersja (MSX1 + ulepszenia MSX2: paleta),
* `invaders-cf2000.rom` — kompilacja z `-DGAME_LITE`: mniej bomb, bez UFO,
  bez kodu palety MSX2 — profil dla podstawowego MSX1.

Obie wersje powstają z tych samych źródeł — różnice tylko przez `#ifdef GAME_LITE`.

## Uruchomienie w emulatorze

```bash
openmsx -machine Sony_HB-F1XD -carta bin/invaders.rom
```

(dla profilu MSX1: `-machine National_CF-2000` jeśli masz systemROM-y, albo dowolna maszyna `C-BIOS` MSX1).

## Sterowanie

* Kursory **←/→** lub joystick w porcie 1 — ruch
* **Spacja** lub przycisk joysticka — strzał

## Architektura

```
src/
  crt0.s    - nagłówek ROM "AB", init stosu/danych, wejście do main() (ASM)
  msx.h     - wspólne typy i adresy BIOS/systemowe
  input.c   - GTSTCK/GTTRIG przez BIOS (wstawki ASM), wait_vsync (HALT)
  vdp.c/h   - TMS9918/V9938: SCREEN 2, VRAM, sprite'y, paleta MSX2
  psg.c/h   - AY-3-8910: efekty (strzał, trafienie, eksplozja, marsz)
  data.c/h  - bitmapy kafli inwaderów, osłon i wzorów sprite'ów
  game.c    - logika gry, main()
```

Rozwiązania techniczne:

* **SCREEN 2**: inwaderzy = kafle (name table, ruch skokowy co 8 px, shadow RAM),
  gracz/pocisk/bomby/UFO = sprite'y sprzętowe 16×16.
* Wzory i kolory kafli identyczne we wszystkich 3 bankach SCREEN 2 —
  proste rysowanie na całym ekranie.
* Font kopiowany z BIOS-u (CGTABL, 0x0004) do VRAM.
* Pamięć: kod 0x4000+, dane od 0xC000 (mieści się w 16 KB RAM CF-2000),
  stos poniżej obszaru systemowego (0xF380).
* Przerwania BIOS-u zostają aktywne (HALT = synchronizacja z VBLANK);
  dostęp do portu VDP 0x99 chroniony DI/EI.

## TODO / pomysły

- [ ] Dźwięk marszu bliższy oryginałowi, syrena UFO
- [x] Animacja eksplozji inwadera (kafel przejściowy)
- [ ] Kolizja pocisk-bomba
- [ ] Tabela najlepszych wyników w SRAM (Carnivore2) / na SD
- [ ] Wersja MegaROM z muzyką (więcej banków)
