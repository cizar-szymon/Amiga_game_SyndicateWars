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
- **Arm Compiler 6** – jest (Keil MDK, 6.13.1, sekcja „Kompilator”); GCC się nie nadaje
  (format `--bare_metal_pie`).
- Nagłówki są na dysku: `C:\Workprojects\csAmigaWarp\Arm\common\WarpExtEnv.h`,
  `C:\Workprojects\csWarpAmigaSoftware\cswarp_library\sdk_includes` (inline tylko dla vbcc – dla bebbo
  gcc własne wstawki LVO `extLoad` -558, `extXchg` -564, `extFetch` -570, `extFeed` -576, `extStop` -582).
- Wzorzec projektu: `C:\Workprojects\AmigaSoft\YouTube_viewer\warpVideoPlay-main` (arm/Makefile,
  `ext_libc.c`, `ext_stdio.h`, `mkwxt.py`, `host_main.c` z zastępnikiem strony 68k, `wxt_blob.s`,
  `common/wvp_link.h`, narzędzia `wxrun.py`/`wxsample.py`).

## Wzorzec: WarpQuake2ARM (cała gra na ARM, 68k jako terminal)

`https://github.com/cslab-sc/WarpQuake2ARM` (kopia tylko do odczytu: `C:\Workprojects\ext-ref\WarpQuake2ARM`,
GPL v2 – przed skopiowaniem plików sprawdzić nagłówki licencyjne; format protokołu można powtórzyć).
Z opcją `-arm` cały silnik Quake II (klient, serwer, gra, renderer, czyste C) chodzi na ARM, program
68k tylko otwiera ekran, pokazuje klatki, czyta wejście, odtwarza dźwięk i obsługuje pliki.

- Pliki platformy po stronie silnika kompilowane drugi raz na ARM; system tylko przez wywołania
  `qa*` (`src/amiga_q2.h`): na ARM rekordy łącza (`arm/ext_q2.c`), na 68k obsługa (`src/amiga_arm.c`).
- Protokół `src/q2link.h`: `QL_FOPEN/FREAD/FWRITE/FIND`, `QL_SCREEN_OPEN`, `QL_PALETTE`, `QL_FRAME` +
  `QL_FRAME_DONE` (2 strony w render area, `extFetch` prosto do ekranu, bufor zapasowy), `QL_SOUND`,
  `QL_KEY/QL_MOUSE`, `QL_NET_*`; obsługa rekordów z `warplink.c` (WarpQuakeARM).
- Dźwięk miksowany na ARM; od WarpExtEnv v6 prosto do miksera karty (bez AHI, bez łącza) – tak też
  dla Syndicate Wars.
- Czas z licznika cykli M7 (64 bity), stos 512 KB, Makefile AC6 `-O3 -ffp-mode=fast -fno-unroll-loops`,
  kontener w programie 68k (`armclient.s`), `-armclient <plik>` do testów, CRC klatek (`-crc*`, `framecmp.py`).

Wyniki z historii repozytorium (demo1, Warp z ARM 480 MHz; 68060 104 MHz):

| | ms na klatkę | kl./s |
|---|---:|---:|
| 68060, 320×240×8 (C + asm 68060) | 84,00 | 11,9 |
| ARM, 320×240×8, sam C renderera id | 36,07 | 27,7 |
| ARM, 320×240×8, pętle M7 + pamięć na chipie | 30,74 | 32,5 |
| ARM, 640×480×8 (bez dźwięku / z dźwiękiem) | 67,1 / 70,3 | ~14,5 |
| ARM, 640×360×16 | 43,5 | ~23 |

Szczyt sterty przy 640×480 z dźwiękiem: 16,7 MB z 18,4 MB.

## Kompilator (stan 12.11.2026)

Na PC jest **Keil MDK Plus 5.29 z Arm Compiler 6.13.1** (`C:\Keil_v5\ARM\ARMCLANG\bin`, licencja
działa). Starszy niż testowany 6.24, ale bare-metal PIE działa: test skompilowany, zlinkowany
(`--bare_metal_pie`, microlib) i zamieniony na kontener przez `mkwxt.py` (relokacje sprawdzone). Różnica:
linker 6.13 wymaga pustego `void __arm_preinit_(void) { }` obok `__arm_relocate_pie_` i ostrzega
L6085 (opcja przestarzała – wyłączyć `--diag_suppress=6085`). Nowszy AC6 tylko z kontem Arm – na razie
nie potrzeba.

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
