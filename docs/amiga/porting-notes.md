# Dziennik portu Syndicate Wars na Amigę

Sposób portowania według `PORTING_PLAN.md`: **D (port ze źródeł)** z etapem wstępnym
„asm x86 → C” (etap A w `CLAUDE.md`).

## Stan prac

| krok | stan |
|---|---|
| A0. Repo: klon SyndWarsFX, remote `upstream`, gałąź `amiga` | ✅ 11.11.2026, baza `5c4b0d56` |
| A0. Build referencyjny i386 (MSYS2 mingw32) | ✅ `tools/amiga/build_x86.sh` |
| A0. Dane GOG → instalacja PC (`Syndicate Wars/syndwarsfx`) | ✅ gra startuje, intro Smackera gra |
| A0. Inwentaryzacja asm | ✅ wstępna (statyczna), `asm-inventory.md` |
| A0. Kolejność konwersji | ⏳ do akceptacji użytkownika |
| A0. Test grywalności wersji referencyjnej (użytkownik) | ✅ 11.11.2026 „działa, gra się dobrze” |
| A1. Konwersja asm → C: wczytanie i rysowanie misji 1 | ✅ ~35 funkcji, fncheck 0 różnic |
| A1. Konwersja asm → C: symulacja świata (process_things) | ⏳ ~57 funkcji |
| A2. Kolejność bajtów: mapa, poziom, prymitywy, sprite'y, animacje, FLI, WAD | ✅ (misja 1) |
| B1. Build Amigi (Docker, bebbo gcc), warstwa bflibrary m68k-amigaos | ✅ |
| B1. Misja 1 na Amidze | ✅ 12.11.2026: Amiga 1 (Warp 1260) ~5 kl./s, obraz zgodny z PC |
| B3. Optymalizacja | ⏳ 12.11.2026: 218 → 77 ms na klatkę (4,6 → 12,9 kl./s), tabela niżej |

## 11.11.2026 — start

- **MSYS2 wycofuje i686:** repo `mingw32` ma ok. 220 pakietów, brak `mingw-w64-i686-openal`.
  OpenAL Soft 1.24.3 zbudowany ze źródeł (cmake + ninja i686) i zainstalowany do `/mingw32`.
  Skrypt zależności nie jest w repo (jednorazowy); kroki: `git clone --branch 1.24.3 kcat/openal-soft`,
  `cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/mingw32 -DALSOFT_EXAMPLES=OFF -DALSOFT_UTILS=OFF`.
- **`make install` upstream nie działa ze spacją w ścieżce** (hook kopiowania DLL-i w
  `src/Makefile.am`, `cp ... $(DESTDIR)${prefix}/...` bez cudzysłowów). DLL-e skopiowane ręcznie.
  Kandydat na mały PR upstream.
- **Obraz płyty GOG:** `game.ins` (CUE) + `game.gog` (BIN, MODE1/2352 + 3 ścieżki audio).
  Wolumin `SYN WARS QA 9.0`, 794 pliki. Własny ekstraktor `tools/amiga/cdimage.py` (bez 7-Zip).
  Muzyka: ścieżki CD 2–4 → `track_1..3` (ok. 9,5–10 min każda), do gry PC jako OGG 192 kb/s.
- Gra bez wpisu w rejestrze czyta dane i zapisy z bieżącego katalogu (`sys_get_data_path`).
- Wersja: `syndwarsfx ver 0.3.8.4599`, okno „Syndicate Wars Fan Expansion”, 640×480.

## 11.11.2026 — pokrycie asm w misji (pomiar na żywo)

Build `tools/amiga/build_x86.sh cov`: przy każdej etykiecie funkcji asm `movl $1, znacznik`
(nie zmienia flag ani rejestrów; `tools/amiga/asm_cov_filter.py` przez `CCAS=tools/amiga/ccas_cov.sh`),
`src/asm_coverage.c` zbiera trafienia według faz pętli `game_process` i zapisuje `asm_coverage.txt`.
Raport: `tools/amiga/cov_report.py`. Wynik: `asm-coverage-mission1.md`.

Misja 1 (`-q -W -m 0,1`), 40 s bez sterowania (636 klatek), agenci stoją, przechodnie i ruch uliczny działają:

| faza | funkcje asm | instrukcje |
|---|---:|---:|
| init (wczytanie misji) | 39 | 4 293 |
| **draw (rysowanie widoku misji)** | **3** | **186** |
| things (symulacja świata) | 57 | 8 552 |
| other (pakiety, kamera, test misji) | 10 | 962 |
| razem | 95 | 12 723 |

**Wniosek:** renderer misji jest praktycznie cały w C. W rysowaniu zostały tylko `alt_at_point`,
`check_big_point_triangle` i `process_map_craters`. Widok misji na Amidze wymaga przepisania ok.
4,5 tys. instrukcji (init + draw), a nie 56 tys. Symulacja świata dochodzi potem, według pokrycia
kolejnych scenariuszy (walka, pojazdy, perswadotron). Uwaga: pokrycie z jednego biegu bez walki,
pozostałe ścieżki (broń, eksplozje, pojazdy) trzeba jeszcze zmierzyć.

## Otwarte ryzyka

- Ok. 56,5 tys. żywych instrukcji x86 logiki gry (`swars.sx`) — główny koszt etapu A.
  Upstream przepisuje ok. 30 funkcji miesięcznie; trzeba się z nim zgrać.
- Brak (jeszcze) narzędzia do porównania stanu gry asm vs C po konwersji funkcji.

## 12.11.2026 — misja 1 na Amidze

Droga do pierwszego obrazu (kolejne awarie i ich przyczyny):
1. brak `config.ini` (Data="Max") - gra szukała danych na „CD”;
2. indeks WAD (`Offset`, `Length`) little endian - zły tekst celów (asercja);
3. mapa `.mad` i poziom - konwersja przez `lvendian.c` + `data_layouts.h`;
4. prymitywy pojazdów (`qdata/primveh.obj`) - skaner poza mapą;
5. `mul_shift16_sign_pad_lo` (wstawka x86 bez `call`) zamieniona przez zaślepkę na 0;
6. odtwarzacz FLI (billboardy) - nagłówki little endian, przepełnienie bufora
   nadpisywało mapę (listy mapwho) -> nieskończona pętla w `engine_draw_things`.
Znalezione śladem faz pętli (`AMIGA_STEP`, `platform/amiga/phase_trace.c`) i kontrolą list mapwho.

Pomiar: Amiga 1 (A1200 + Warp 1260, 68060) ~5 kl./s w misji 1, 640x480x8, świat zamrożony.
WinUAE z JIT (`C:/amiga-dev/winuae/SyndWars_A1200_OS32_net_jit.uae`, bez MMU) - testy funkcjonalne.

## 12.11.2026 (noc) — optymalizacja widoku misji na Amidze 1

Pomiar zawsze tak samo: `sh tools/amiga/prof_a1.sh` (build, wgranie do `WorkSD:Games/SyndWars`,
misja 1 `-q -m 0,1`, 30–40 s, ostatni raport profilera z `error.log`). Amiga 1: A1200 + Warp 1260,
68060, RTG Warpa, 640x480x8. Świat zamrożony (symulacja = zaślepki asm), bez dźwięku.

Narzędzia:
- profiler slotów (`platform/amiga/prof.c`): sloty co klatkę zawsze; dokładne (per trójkąt, typ
  elementu listy, tryb `trig`) tylko z `SetEnv SyndWarsProf 2` — **jeden odczyt E-clock kosztuje
  ok. 15 µs**, dokładny profil dokłada ok. 20 ms na klatkę;
- **profiler próbkujący** (`platform/amiga/sampler.c`, `SetEnv SyndWarsSample 1`): proces
  o priorytecie 5 co 1 ms kopiuje zapisany kontekst zadania gry (`tc_SPReg`), PC jest w słowie 31
  (wcześniej rejestry FPU); `tools/amiga/samples.py RAM:SyndWars-samples.bin nm.txt` daje udział
  funkcji. Funkcje `static` nie mają symboli — są liczone do poprzedniego symbolu globalnego
  (np. pętle odcinków gpoly jako `_gpoly_divtable`);
- tryb sprawdzania (`SetEnv SyndWarsCheck 1`): każda szybka ścieżka liczona też wzorcowo w C
  i porównywana (odcinki gpoly, kroki krawędzi, odcinki trybów 6/10).

| zmiana | klatka [ms] | sprawdzenie |
|---|---:|---|
| start (C po konwersji, `-O2 -m68060`) | 218 | — |
| pętla odcinka gpoly w asm 68k (ADD/ADDX zamiast bajtowych przeniesień w C) | 126 | check: 0 różnic |
| `CopyMemQuick` dla całej klatki zamiast pętli wierszy | 122 | zrzut |
| software pipelining pętli odcinka | 114,5 | check: 1,4 mln odcinków, 0 różnic |
| dokładne sloty profilera tylko na żądanie (koszt odczytu E-clock) | 107,2 | — |
| własne `__muldi3` (libgcc ma 64-bit MULU.L = pułapka 060), mieszanie skanera w zmiennych lokalnych, odcinki gpoly po 2 piksele z przeplotem | 104,9 | PC: 20 mln mnożeń 0 różnic; check 0 |
| kroki krawędzi gpoly (`incr4/decr4/incr3/decr3`) w asm | 100,4 | check: 1,8 mln kroków, 0 różnic |
| odcinki `trig` trybów 6 (uszkodzenia podłogi) i 10 (cienie postaci) w asm | 97,2 | check: 131 tys. odcinków, 0 różnic |
| `draw_drawlist_2` pomija puste kubełki (24 000 na klatkę, 2 wywołania i 2 dzielenia każdy); `asm_stub_hit` bez `strcmp` przy każdym wywołaniu | **83,5** | zrzut zgodny |
| własne `__divdi3/__moddi3`, `mul_shift16_sign_pad_lo` bez mnożenia 64-bit | 84,0 (szum) | PC: 0 różnic |
| areny pamięci wyrównane do 16 B (bufor ekranu był przesunięty o 8 → MOVE16 nie działał: kopia 9,7 ms zamiast 5,8) | 84,0 | log adresu |
| rasteryzator `shaded_nobound` (165 z 212 trójkątów) ze stanem w rejestrach | 82,8 | check: trójkąt rysowany obiema wersjami, wiersze porównane, 0 różnic |
| cała pętla linii trójkąta w asm (`gpoly_lines_shaded_68k`: odcinek + krawędzie + krok bez wywołania na linię) | 79,2 | check: ~8800 trójkątów, 0 różnic; widok 3D identyczny na zrzucie |
| `gpoly_mul_rot_1/2`, `gpoly_get_uv_inc` bez iloczynu 64-bit (ok. 9 `__muldi3` na trójkąt) | 78,8 | PC: 30 mln par 0 różnic; zrzut |
| rasteryzator `shaded_bound` (trójkąty przy krawędzi ekranu) ze stanem w rejestrach | 78,1 | check: 2048 trójkątów, 0 różnic |
| pętla mieszania mapy skanera w asm (mapa skanera 3,5 → 2,5 ms) | **77,2** | check: piksele i stan końcowy każdej linii, 0 różnic |

Pomiar końcowy (60 s): **77,3 ms na klatkę, 12,9 kl./s** (66 klatek w 5,1 s, stabilnie).
Licznik pikseli gpoly w raporcie nie obejmuje już linii rysowanych przez `gpoly_lines_shaded_68k`.

Próby nieudane:
- zapis odcinka długimi słowami (4 piksele w rejestrze) — wolniej (42,0 zamiast 39,6 ms rasteryzacji):
  zapis bajtów do Fast RAM nie jest wąskim gardłem;
- maska indeksu tekstury (eksperyment: tekstura w 256 B) — wolniej, czyli chybienia cache tekstury
  też nie; pętla odcinka jest ograniczona łańcuchem zależności (przeniesienia ADDX, indeks → odczyt);
- `-O2` z wklejoną wstawką asm do rasteryzatora: gcc 6.5 „internal compiler error” w reload —
  funkcje odcinków są `noinline`.

Podział klatki 84 ms (sloty, bez próbkowania): rysowanie 70 ms, w tym widok 3D 44,6 ms
(piksele gpoly ok. 26 ms: 176 tys. pikseli, 6578 odcinków, 212 trójkątów), `draw_things` 9 ms,
podłoga 6,3 ms (2080 punktów siatki), HUD 9,9 ms (skaner 6,8: mapa 3,5 + kropki 2,8);
kopia na kartę 5,8 ms (granica zapisu do VRAM ~55 MB/s), czyszczenie 2,1 ms, `things` 3,6 ms.

Profil próbkujący (po zmianach): pętle odcinków gpoly 31%, system (kopia na kartę, timer) 11%,
`transform_shpoint` 5,5% (3800 wywołań na klatkę, 2080 z podłogi), rasteryzatory gpoly 7,9%
(narzut na linię), mieszanie mapy skanera 4%, `draw_object_faces` + widoczność ścian ok. 4%.

Zostało do zrobienia (kolejność według zysku):
- pętla pikseli gpoly (ok. 25% klatki) — ograniczona łańcuchem zależności ADDX; dalsze próby
  wymagają innego rozkładu instrukcji albo zmiany formatu stanu (z zachowaniem bitów wyniku);
- `draw_things` 9 ms: przechodzenie kafli (`engine_draw_things`), ściany budynków
  (`draw_object_faces`, `object_face_get_visible_max_depth`) — dużo drobnych kosztów;
- podłoga 6,4 ms (2080 punktów siatki: wysokość, `transform_shpoint`, cień);
- kropki skanera 2,8 ms (`map_coords_to_scanner`: 2 dzielenia 64-bit na obiekt);
- czyszczenie bufora 2,1 ms i kopia na kartę 5,8 ms (granica szybkości VRAM);
- uwaga: zaślepki symulacji (`process_*`, `people_intel`, ale też **`calc_lighting`** i **`arctan`**)
  są wywoływane co turę — po ich konwersji czas `things` wzrośnie.

## 12.11.2026 (rano) — 320×200, Amiga 2, błąd pamięci

- **320×200 domyślnie na Amidze** (`set_default_gfx_settings`, tylko m68k; F8 i menu opcji
  dalej przełączają, zapisane ustawienia gracza mają pierwszeństwo). Misja 1 na Amidze 1:
  640×480 77 ms → 320×200 **37,0 ms** (27 kl./s). Zrzut 320×200 zgodny.
- Pomiary przeniesione na **Amigę 2** (A4000 + Warp 7060): ten sam build 36,8 ms (Amiga 1: 37,0).
  Dane gry w `WorkSD:Games/SyndWars` (paczki `pack_data.py` + `config.ini`, bez którego gra
  szuka danych na CD). Skrypty: `TARGET=amiga2`. Uwaga: w `amiga_remote.py run` średnik to
  komentarz AmigaDOS — jedno polecenie na wywołanie.
- **Profil z funkcjami `static`**: plik wykonywalny Amigi ma tylko symbole globalne, więc
  próbki z funkcji statycznych trafiały do poprzedniej globalnej (np. 15% „`gpoly_init_state`”
  to `gpoly_lines_shaded_68k`). `tools/amiga/nm_statics.py` dokłada symbole statyczne z plików
  `.o` (przesunięcie względem globalnego symbolu tego samego pliku).
- **Dzielenie 64/32** (`__divdi3` z dzielnikiem 32-bit i szerszą dzielną: kropki i linie
  skanera) szło pętlą 64 kroków; teraz dwa sprzętowe `DIVU.L` 32/32 na cyfrach 16-bit
  (Hacker's Delight, `divlu`). PC: 37 mln losowych par + przypadki brzegowe, 0 różnic.
  Kropki skanera 2,7 → 2,1 ms, klatka 36,8 → 36,5 ms.
- **Błąd portu: `low_trans_grey_bright_limit[]` miała 1 bajt zamiast 256** (`asm_data.c`,
  tablica bez rozmiaru i bez inicjalizatora). Inicjalizacja sprite'ów wypełnia 256 bajtów, więc
  przy starcie misji nadpisywała 255 bajtów zmiennych za nią (dane BAT, `signal_count`,
  `map_crater_next`, `datagram_backup`, `dword_1C6DE8`, `data_1c8468`, liczniki profilera,
  początek `joy`). Wykryte, bo nowa flaga profilera w tym miejscu zmieniała się sama; zrzut
  pamięci pokazał rampę 0..63. Poprawione w `asm_data.c` i w `gen_asm_data.py`.
- **Profiler tylko na żądanie** (`ENV:SyndWarsProf`: brak = sam średni czas klatki, 1 = sloty,
  2 = sloty dokładne; `prof_a1.sh PROF=1|2`). Sloty kosztowały ok. 1,7 ms na klatkę.

Wynik (Amiga 2, 320×200, misja 1, bez profilera): **34,8 ms na klatkę, 28,7 kl./s**.

Profil próbkujący 320×200 (z symbolami statycznymi): `gpoly_lines_shaded_68k` 14%,
`transform_shpoint` 8% (punkty podłogi i obiektów; kod gcc dobry, dominuje dzielenie
perspektywy ~38 taktów), system 6% (kopia na kartę 1,2 ms), `engine_draw_things` 4% (przejście
kafli w kolejności z, chybienia cache), `draw_object_faces` 3,6%, `lvdraw_do_floor` 3,5%,
`object_face_get_visible_max_depth` 3,2%, `shpoint_compute_coord_y` 2,7%, `draw_drawlist_2` 2,4%.

Dalsze możliwości (każda ~0,5–1 ms): podłoga — przyrostowe iloczyny obrotu wzdłuż wiersza
(dokładne, suma przed przesunięciem rośnie o stałą), wysokość i transformacja w jednej pętli;
dzielenie perspektywy w FPU równolegle z częścią całkowitą (wynik dokładny: dla dodatnich
liczb < 2^31 obcięcie ilorazu `double` = iloraz całkowity), z wersją bez FPU.
