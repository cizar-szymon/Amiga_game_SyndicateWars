# Plan: cała gra na ARM Warpa (STM32H7), 68060 tylko sterowanie i dźwięk

Stan: **odłożony do czasu otrzymania klucza rozszerzeń** (12.11.2026). Źródło:
`C:\Workprojects\AmigaGAMES\Warp-ARM-Extension-DevNotes.pdf` (firmware 30.52+, cswarp.library 30.41+,
WarpExtEnv v4).

## Dlaczego

Na samym 68060 (Amiga 2, misja 1, 320×200, bez symulacji świata) tura gry trwa **69 ms** (14,5 tury/s,
oryginał 16). Symulacja (dziś zaśleplony asm x86) dołoży szacunkowo 14–28 ms na turę (pomiar na PC:
`process_things` 0,40 ms/turę, przelicznik PC→68060 35–70× z kodu C mierzonego na obu maszynach),
czyli ok. 85–100 ms na turę: gra w zwolnionym tempie albo z pomijaniem obrazów. Cortex-M7 400–480 MHz na
**Warp7060 i Warp1260 Extreme** (obie nasze Amigi) ma liczyć całą grę: symulację, rysowanie, menu.
68060: wejście, pliki, wyświetlenie gotowej klatki i palety, dźwięk.

## Warunki wstępne

- **Klucz `ext_key_warp.h`** – od autora firmware (użytkownik prosi o niego). Trzymany poza repozytorium,
  nigdy nie commitowany, nie wypisywany, nie kopiowany do projektu; w Makefile tylko ścieżka (`EXT_KEY=`).
  Bez klucza `mkwxt.py` robi kontener niepodpisany, który firmware wydaniowy odrzuca (`EXTERR_SIGNATURE`).
- **Arm Compiler 6** (armclang/armlink/fromelf, testowany 6.24) – instaluje Claude na Windows
  (CMSIS SDK); GCC się nie nadaje (format `--bare_metal_pie`).
- Nagłówki są na dysku: `C:\Workprojects\csAmigaWarp\Arm\common\WarpExtEnv.h`,
  `C:\Workprojects\csWarpAmigaSoftware\cswarp_library\sdk_includes` (inline tylko dla vbcc – dla bebbo
  gcc własne wstawki LVO `extLoad` -558, `extXchg` -564, `extFetch` -570, `extFeed` -576, `extStop` -582).
- Wzorzec projektu: `C:\Workprojects\AmigaSoft\YouTube_viewer\warpVideoPlay-main` (arm/Makefile,
  `ext_libc.c`, `ext_stdio.h`, `mkwxt.py`, `host_main.c` z zastępnikiem strony 68k, `wxt_blob.s`,
  `common/wvp_link.h`, narzędzia `wxrun.py`/`wxsample.py`).

## Pamięć ARM (szacunek z kodu, do zmierzenia)

Okno 14 MB (obraz programu + bss + stos + arena) i render area 8 MB.

| Część | Dziś (port 68k) | Na ARM |
|---|---|---|
| kod gry | 0,9 MB | 0,9 MB |
| `.data` + BSS | 2,0 MB (1,2 MB to bufory debug w `poly_gp.c`) | 0,8 MB |
| blok silnika (mapa, obiekty, ściany, światła, lista rysowania, things) | 5,9 MB | 5,9 MB |
| sprite'y misji | 1,35 MB | 1,35 MB |
| tekstury + tablice cieni/ghost | 0,65 MB | 0,65 MB |
| `W_SCREEN` | 3,7 MB (2560×1441, `game.c:357`) | 0,3 MB (640×480) |
| sterta dźwięku | 1,5 MB | 0 (dźwięk na 68k) |
| **razem** | ~19 MB | **≈ 10 MB** |

Do zmierzenia: szczyt przy wczytywaniu mapy (`.mad` do 1,87 MB, `lvfiles.c:1754`) i menu
(`data/equip` 14 MB na dysku, ładowane po kawałku). Arena `LbMemorySetup` (dziś 16,5 MB,
`smemory.c:448`, `platform/amiga/bfconfig.h`) do zmniejszenia.

## Architektura

```
68060 (SyndWars, bebbo gcc)                 Cortex-M7 (swcore.wxt, armclang)
 ekran RTG 8-bit, paleta LoadRGB32  <-FRAME/PAL-   cała gra: game_process, symulacja,
 wejście IDCMP (klawisze, mysz)     -INPUT->       renderer, menu, HUD (ten sam kod C)
 pliki na żądanie                   <-FREQ / extFeed  pliki z pamięci (ext_libc), zapisy -> 68k
 dźwięk Paula/AHI z próbek          <-SND-           play_sample_* -> rekord SND
 zegar, Ctrl-C/Esc, extStop         -TICK->          clock()/LbTimerClock z env->millis
 extFetch klatki prosto do VRAM ekranu (QSDMA), strony podwójne + DONE
```

- ARM jest little-endian jak x86: formaty plików i struktury jak w wersji referencyjnej; piksele 8-bit
  bez zamiany bajtów.
- Granica z systemem (już wydzielona przez port Amigi): pliki `sfile.c`/`gfile.c`, ekran `sscreen.c`,
  paleta `LbPaletteSet`, wejście `swindows.c`→`gkeybd.c`/`gmouse.c`, czas `stime.c` + `clock()`
  (`game_speed.c:145`, symulacja zależy od zegara), dźwięk `play_sample_using_heap`/`play_dist_sample`/
  `PlayCDTrack` (dziś `platform/amiga/sound_stubs.c`). Logika nie czyta pikseli ekranu.
- Protokół jak w PDF §8 (rekordy LE, HELLO z wersją, LOG). Nowa warstwa `platform/warp/`.

## Etapy

1. Narzędzia: AC6, klucz (ścieżka), zbudowanie przykładu / minimalnego rozszerzenia, `WarpDebug extState`
   na Amidze 2 i 1, wstawki LVO dla bebbo gcc.
2. Rdzeń gry bez SDL (`platform/warp/`), arena ≤ 10 MB, `W_SCREEN` 640×480; **host build na PC**
   (32-bit x86 – asm symulacji tam działa) z `host_main.c` i zastępnikiem 68k (SDL): CRC klatek = build
   referencyjny.
3. Gra na M7 z zaślepkami symulacji: misja 1 rysowana przez ARM, `extFetch` do ekranu, wejście z 68k;
   pomiar 320×200 i 640×480 na Amidze 2.
4. Konwersja symulacji asm→C na x86 (`fncheck`, 0 różnic; kolejność wg `asm-coverage-mission1.md`:
   57 funkcji, ~8 500 instrukcji), potem walka/pojazdy. Każda konwersja działa też w wersji 68k.
5. Dźwięk na 68k (rekordy SND), muzyka później.
6. Menu, zapisy gry (pliki do 68k), filmy (Smacker – asm) na końcu.
7. Optymalizacja M7: ITCM dla pętli pikseli, SRAM dla tablic, `wxsample.py`; cel 16 tur/s i więcej obrazów.

## Sprawdzanie

- CRC każdej klatki: host = build referencyjny (deterministyczne wejście, `time(0)`/`clock()`), ARM = host.
- Pomiary tylko na prawdziwej Amidze; build x86 nie może się zepsuć.
