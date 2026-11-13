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

## Pierwszy test na płycie (12.11.2026, Amiga 1, Warp 1260 Extreme)

`tools/warp/exthello` (`build.sh`: AC6 → zapieczętowany `hello.wxt`, vbcc → `exthello`). Wynik: **działa**.
cswarp.library 30.73, WarpExtEnv **v6**, ARM 480 MHz; okno **19 904 KB** (arena 19 832 KB), render
area 8 192 KB; szybka pamięć: 32 KB ITCM (`$00008000`, exec+TCM), 64 KB + 16 KB SRAM (cache).
Pętla 10M mnożeń-dodawań 45,2 ms (~2,2 cyklu na iterację), kopia 1 MB w SDRAM 20,4 ms (~51 MB/s),
`extFetch` 64 KB do Fast RAM 1,1 ms (~60 MB/s, 0 błędnych bajtów), echo przez pierścienie i wyjście
z kodem 42 – w porządku. Pamięci jest więcej, niż zakładał plan (19,4 MB zamiast 14).

## Gra na ARM – pierwsze uruchomienie (12.11.2026, Amiga 1)

Etapy 2–3 w wersji minimalnej: **cała obecna gra liczona na ARM** (te same źródła co wersja 68k, symulacja
nadal zaślepiona), 68060 tylko wyświetla klatki, paletę, obsługuje pliki i wejście.

- Budowa: `tools/warp/build_arm.sh` (AC6, ten sam zestaw plików co `build_amiga.sh`, `m68k_prep.py` usuwa asm
  x86) → `build/warp/swcore.wxt` (695 KB, zapieczętowany); `tools/warp/build_host.sh` (vbcc) → `swhost`.
  Uruchomienie: `python tools/warp/run.py [--amiga2] [--secs N] [--keep] -q -m 0,1` (log `RAM:swarm-log.txt`).
- Warstwa ARM: `platform/warp/` (`wmain.c` wejście, `wlibc.c` sterta/pliki/stdio/getopt/czas, `wlink.c`
  rekordy, `prof.c` profiler na liczniku cykli), `bflibrary/src/arm-warp/` (ekran, czas, zdarzenia,
  klawiatura). Protokół `platform/warp/swlink.h`. Pliki czytane przez łącze (okna 64 KB, `extFeed`),
  klatka: ARM rejestruje `WScreen`, 68k robi `extFetch` prosto do bitmapy ekranu.
- Pułapki znalezione przy uruchamianiu: wpisy `.init_array` armlinka są względne; `#pragma pack(1)` ze
  wskaźnikami w danych inicjowanych daje relokacje pod nieparzystym adresem (loader odmawia –
  `PathInfo`/`MemSystem` bez pakowania w `SW_WARP`); odczyt spod adresu 0 (kontrola `mapwho` przed
  przydziałem mapy) to na ARM błąd szyny i reset całej Amigi; `extFeed` wymaga co najmniej 32 bajtów.
- Pamięć: sterta 18 044 KB, szczyt 16 960 KB (arena gry 16,5 MB w całości).

Pomiar, misja 1, Amiga 1 (Warp 1260 Extreme, ARM 480 MHz / 68060 100 MHz), profiler poziom 1, czas na turę:

| | 68060, 320×200 | ARM, 320×200 | ARM, 640×480 |
|---|---:|---:|---:|
| tury/s (limit 16) | 14,1 | 15,6 | 15,7 |
| rysowanie (`draw`) | 60,0 ms | 24,4 ms | 50,8 ms |
| – trójkąty (`draw_screen`) | 28,1 | 11,7 | 31,5 |
| – obiekty (`draw_things`) | 12,9 | 5,6 | 7,7 |
| – podłoga | 8,8 | 2,4 | 3,4 |
| – HUD | 9,2 | 4,4 | 7,8 |
| symulacja (`things`, zaślepiona) | 7,1 | 3,2 | 3,2 |
| wyświetlenie (`swap`) | 3,4 | ~1,2 | 9,2 |
| **praca na turę** | **71 ms** | **~29 ms** | **~63 ms** |

ARM w 320×200 jest ~2,5× szybszy i ma zapas (~34 tur/s możliwe, gra trzyma 16); 640×480 na ARM ≈ 320×200
na 68060 – jak w Quake II. Bez żadnej optymalizacji pod M7 (`-O2`, kod i dane w SDRAM, bez ITCM/SRAM).
Pobranie klatki: 1,1 ms (64 KB), 5,2 ms (300 KB) – dziś ARM czeka na nie; podwójna strona ukryje ten czas.

### Szybkie zyski (12.11.2026, Amiga 1)

Miara: `busy per turn` (raport profilera na ARM, także bez slotów: czas tury bez czekania na następną),
misja 1 bez ruchu, `tools/warp/measure.sh` (320×200, potem F8 → 640×480).

| zmiana | 320×200 | 640×480 |
|---|---:|---:|
| start (`-O2`) | 31,6 ms | 64,8 ms |
| `-O3` (domyślne w `build_arm.sh`) | 30,6 | 63,4 |
| dwie strony z kopią klatki | 30,7 | 65,7 – **odrzucone**: kopia w SDRAM kosztuje tyle co pobranie |
| `WScreen` w render area (bez write-allocate), pobieranie prosto z niego | 29,1 | 58,5 |
| klatka wysłana zaraz po narysowaniu (68k pobiera w czasie symulacji i czekania) | 29,9–30,4 | **54,0** |

640×480 ma teraz pełne 16 tur/s z zapasem ~8 ms; 320×200 ~30 ms z 62. Obraz (misja w obu rozdzielczościach,
menu) sprawdzony zrzutami. Dalej: profil funkcji na ARM (brak próbkowania – sloty) i pamięć na chipie
(ITCM 32 KB, SRAM 64+16 KB) dla najgorętszych pętli i tablic.

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

### Optymalizacja, noc 12/13.11.2026 (Amiga 1)

Scena pomiarowa: `tools/warp/bench.sh` – nagranie misji 1 (`-p 1`, agenci chodzą i strzelają),
640×480, średnia `busy per turn` od 30 s. Symulacja jest już w większości w C, więc liczy naprawdę
(`things` 3 → 11–13 ms). W misjach 3–10 nie ma więcej postaci, a pojazdy nie pracują (`process_vehicle`
i dalej to jeszcze zaśleplony asm) – nagranie misji 1 jest najcięższą sceną.

| zmiana | 640×480 (bench, bez slotów) |
|---|---:|
| start (po przepisaniu nawigacji) | ~61 ms (pomiar ze slotami) |
| pętla pikseli bez ponownego wczytywania tablic, osobna funkcja na ARM (`cec20d06`) | **55,4 ms** (54,4–56,5) |
| `-funroll-loops` | 56,9 – gorzej, odrzucone |
| tablica cieni i najczęstsza strona tekstury w SRAM na chipie, zapis 4 pikseli słowem | bez zmian – odrzucone (łatka w `experiments/`) |

Podział (PROF=2, 62 ms ze slotami): trójkąty 25 ms (piksele 14,4 ms: 353 tys. pikseli, 13 tys.
odcinków, 425 trójkątów na klatkę – ok. 16 cykli na piksel, pętla nie czeka na pamięć), obiekty 7,9,
HUD 7,4 (skaner 4,8), podłoga 3,4, czyszczenie ekranu 1,8; symulacja 12,9 (postacie 10,2: spacer
cywili 5,3, w tym `check_for_other_people` 2,6 – przechodzenie list obiektów na 9 kaflach).
Na ARM dostępna pamięć na chipie: ITCM 32 KB (kod), SRAM 64 KB + 16 KB (`env->fastMem`).

Dalej: pętla pikseli ręcznie w asm ARM z przeplotem dwóch pikseli (z trybem porównania z C), koszty
na odcinek (13 tys.), skaner, obiekty; w symulacji `check_for_other_people`.

Dalsze próby tej nocy (bench, 640×480; łatki w `docs/amiga/experiments/`):

| próba | wynik |
|---|---|
| pętla pikseli w asm Thumb-2, 2 piksele w przeplocie (13 instr./piksel), tryb porównania na ARM: 0 różnic na 7 mln odcinków | bez zmian (piksele 14,8 ms) |
| ta pętla w ITCM (kod bez pobierania z SDRAM) | bez zmian |
| pomiar: bez zapisu do ekranu / tekstura i tablica cieni przycięte do 256 B (zawsze w cache) | bez zmian (14,6–15,2 ms) |
| pomiar: bez pętli pikseli | piksele 14,8 → 3,8 ms, `draw_screen` 25,5 → 13,5 ms |

Wniosek: pętla pikseli to ~11 ms przy 353 tys. pikseli (~15 cykli na piksel) i nie zależy ani od
pamięci, ani od miejsca kodu – ogranicza ją szeregowy łańcuch przeniesień (4 zależne instrukcje na
piksel) i zależne odczyty. Dalszy zysk tylko z mniejszej liczby pikseli (353 tys. na 307 tys. pikseli
ekranu – ~15% nadrysowania) albo innego algorytmu. Do sprawdzenia: `draw_screen` poza pikselami
(~10,7 ms: przekształcenia, sortowanie, obsługa 425 trójkątów), symulacja, skaner.

`draw_things` (8,7 ms ze slotami, PROF=2): ściany budynków 3,9 ms (`draw_thing_object` budynków →
`object_face_get_visible_max_depth`, przejścia ściana → punkt → punkt ekranu), obiekty 0,6, obiekty
proste 0,8, samo przechodzenie kafli i list `mapwho` ~3,4 ms. Koszt to głównie odczyty rozrzucone po
dużych tablicach – bez zmiany układu danych silnika nie widać szybkiego zysku.

### Czy pomoże 68060 (pytanie użytkownika, 13.11.2026)

W wersji ARM 68060 jest prawie wolny (pobiera klatkę ~5,7 ms z 62,5 ms tury), ale nie widzi pamięci
ARM – dane przechodzą tylko przez łącze (`extFetch`, ~60 MB/s). Opłaca się więc dać mu pracę z małym
wejściem i wyjściem prosto na ekran RTG:
- **dźwięk** (miksowanie albo odtwarzanie strumienia) – plan w `sound-plan.md`;
- **panel HUD i skaner** (~2,5 + 4,8 ms ARM w 640×480): ARM wysyła mały rekord stanu (zdrowie,
  broń, pozycje kropek skanera), 68k rysuje panel z własnych sprite'ów na ekranie po pobraniu klatki –
  zysk do ~7 ms ARM (ok. 13%), duża praca (rysowanie panelu i skanera na 68k, zgodność obrazu);
- symulacji i rysowania 3D nie – potrzebują całego stanu gry (MB danych).
