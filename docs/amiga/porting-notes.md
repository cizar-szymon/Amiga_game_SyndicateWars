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
| A1. Konwersja asm → C: symulacja świata (process_things) | ⏳ 12.11.2026: 15 z 29 (bez dźwięku) + 8 funkcji wyszukiwania ścieżek, fncheck 0 różnic na nagraniu misji 1 (poza stanem sterownika dźwięku) |
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

## 12.11.2026 — symulacja: asm → C (etap 1)

- Nagranie gry w misji 1 (wersja referencyjna PC, `-r`): `qdata/savegame/rc00m001.s01` (5 min,
  5578 tur, ruch agentów, drzwi, strzały); odtwarzanie `-q -W -m 0,1 -p 1`. Plik leży przy danych gry
  (nie w gicie).
- `fncheck` z wyborem wywołań: `SW_FNCHK_ONLY=a,b`, `SW_FNCHK_EVERY=N` (pierwsze 1000 wywołań każdej
  funkcji, potem co N-te) – funkcje wołane dla każdej postaci w każdej turze (1,7 mln wywołań) bez tego
  spowalniały przebieg ~20×.
- Faktyczna lista do zrobienia (wywołania asm z C po preprocesorze, build ARM): 37 funkcji w misji 1,
  w tym 8 dźwięku (na później). Na Amidze trafiane zaślepki: `RAM:SyndWars-stubs.txt`.
- Przepisane (0 różnic, całe nagranie): `unkn_update_lights` (+ dwie wewnętrzne; misja 1 nie używa
  gałęzi typów świateł – do sprawdzenia na innych misjach), `delete_snode`, `remove_sthing`,
  `process_danger` (+ `search_for_danger`), `process_stamina`, `check_person_within`, `arctan`,
  `move_mapwho`.
- **Błąd portu:** `remove_sthing` oryginału czyta `sthings_empty_head` jako górną połowę 32-bitowego
  odczytu `sthings_used` (0x19EC80/82); deklaracje C były w innej kolejności i asm brał
  `sthings_used_head`. Kolejność przywrócona.
- `post_process_blips` (przepisana wcześniej) ma pojedyncze różnice w tablicy grających próbek – stan
  sterownika dźwięku PC nie jest cofany przez `fncheck`; do sprawdzenia przy etapie dźwięku.
- Amiga 1 po tej grupie: 68k 71,9 ms/turę (`things` 7,6 ms), ARM 30,2 ms/turę – stabilnie. Zaślepki
  jeszcze trafiane (bez dźwięku): `calc_lighting`, `mem_group_arrived_square2`, `player_weapon_time`,
  `process_gate1`, `process_smoke_generator`, `people_intel`, `person_goto_point`, `process_wander`.
- Dalej (0 różnic): `calc_lighting`, `player_weapon_time`, `process_smoke_generator`,
  `mem_group_arrived_square2`, `process_wander` (odczyt `word_1AA390` – w oryginale górna połowa
  32-bitowego odczytu `word_1AA38E`; zmienne leżą w danych asm w oryginalnym układzie).
- **Błąd upstreamu:** `check_for_other_people_on_tile()` miała niezainicjowane `found` – zderzenia ludzi
  zależały od stosu (funkcja C porównana sama ze sobą: 897 różnic). Oryginał trzyma jeden wynik dla
  9 pól, zerowany raz; poprawione (commit `37f57ed7`). Wniosek: przy niejasnych różnicach najpierw test
  „C kontra C” (ta sama funkcja po obu stronach `FNCHK_*`).
- Zostały trafiane na Amidze: `person_goto_point` (~460 linii, z lokalną `do_next_navigate_index`),
  `people_intel`, `process_gate1`.
- `person_goto_point` (+ lokalna `do_next_navigate_index`): całe nagranie, 18123 sprawdzone wywołania,
  12 różnic – wszystkie tylko w `sample_id` (`person_move()` gra próbkę 79, sterownik dźwięku trzyma
  wolny slot poza pamięcią cofaną przez `fncheck`). `fncheck` opisuje teraz do 20 różnic na funkcję.
- Amiga 2 po tej funkcji: 78,1 ms/turę ze slotami, `things` 14,9 ms. Nowe trafiane zaślepki (ludzie
  idą teraz po ścieżkach): `build_navigate_path`, `two4_line_intersection`, `create_scale_effect`,
  `process_im_shoved`; dalej `people_intel`, `process_gate1`, `BAT_unknsub_20` i dźwięk.
- **Wyszukiwanie ścieżek** (`049612d0`, 0 różnic): `two4_line_intersection`, `triangle_contains8`,
  `triangle_findSE8`, `pointed_at8`, `route_to_path` (+ `edge_points8`, `fov_region`),
  `ma_triangle_route_3` (+ przeszukiwanie z kopcem `triangle_route_do_unkn5`; `unkn6` to ten sam kod),
  `path_init8_thing`, `tri_path_init8`. Wewnętrzne etykiety asm wołane z asm: eksport `ASM_*` i most
  `ac_*` (`conf/wrappers_game.conf`). Kopiec `heap`: wierzchołek to `heap[1]` (`data_1c30c0` w asm).
- `build_navigate_path` + `use_nav_data` + `fatter_than_nothing` (`cde51156`, 0 różnic w ok. 20 min
  nagrania): nawigacja na Amidze działa. `set_xz_for_quad210/_unkn03` zostały w asm (misja 1 ich nie
  używa – ludzie na czworokątnych ścianach), wołane przez wyeksportowane etykiety.
- Przebiegi fncheck odtąd najwyżej 10 min (`timeout 600`) – decyzja użytkownika.
- **Błąd portu (Amiga): za małe struktury z danych asm.** `gen_asm_data.py` kończył dane symbolu na
  każdej etykiecie, także wewnętrznej (pole struktury), więc `map_craters` miało 2 bajty zamiast 1024
  (kratery po wybuchach nadpisywały zmienne za tablicą), `redo_scanner` 2 z 512, `datagram_backup`
  4 z 4400, `com_dev` 0, `IPXPlayer` 46 z 271. Teraz struktura dostaje rozmiar do następnego symbolu
  globalnego (`6ba54116`).
- `tools/amiga/offsets.sh -size` nie działa (opcja tylko w opisie).
- Amiga 1 po tych zmianach: 78,9 ms/turę (sloty), `things` 14,9 ms; zrzut poprawny.

## 13.11.2026 — optymalizacja 68060 (Amiga 2) i zasada dla ARM

**Zasada (decyzja użytkownika 13.11.2026): każde przyspieszenie robione dla 68060 ma też trafić do
wersji ARM (Warp).** Zmiany we wspólnym kodzie C (renderer, symulacja) przechodzą do ARM same; to,
co zależy od platformy (zmienne `ENV:`, asm 68k, profiler), trzeba dla ARM zrobić osobno.

Pomiary: profiler próbkujący (`ENV:SyndWarsProf=3`, timer CIA-B) i próbnik adresów
(`ENV:SyndWarsSample`, `tools/amiga/samples.py` z `func=NAZWA`). Misja 1, 320×200, 68060:

| Zmiana | commit | 68060 | ARM |
|---|---|---|---|
| pomijanie obiektów w całości poza ekranem (`draw_object_faces`, `transform_shbox_outside`) | `3004417e` | `draw_things` 12,8 → 6,8 ms | wspólny C – jest; niezmierzone |
| omijanie ludzi: puste kafelki bez wywołania (`check_for_other_people`) | `d94eb5a7` | symulacja −1,0 ms (misja 21 −1,7) | wspólny C – jest; niezmierzone |
| bez cyklicznych wpisów do `error.log` w zwykłej grze | `98987063` | drobne szarpnięcia co 5 s | ARM pisze log osobno – do sprawdzenia |
| pomijanie podłogi poza ekranem (`lvdraw_do_floor`, siatka wysokości 8×8 kafelków) | `f25d66cd` | podłoga 8,6 → 4,8 ms (640×480: 12,8 → 6,2) | wspólny C – jest; ARM 640×480 55,0 → 52,4 ms |
| kropki radaru tylko w kadrze (`SCANNER_draw_things_dots`) | `d3da6c85` | kropki 3,0 → 1,5 ms | wspólny C – jest |
| opcja radaru co 2. klatkę (`SyndWarsRadar2 1`, domyślnie wył.) | `92b6fa10` | radar 3,4 → 2,0 ms | `swhost` przekazuje `radar2=N` |
| opcja dalekich przechodniów (`SyndWarsCivLOD` 1/2) | `e80c891a` | symulacja 18,8 → 12,5 / 8,8 ms, 13,4 → 15,4 tury/s | `7088a07c`: `swhost` przekazuje `civlod=N`; domyślnie poziom 3 (Amiga i ARM), ARM 40,2 → 30,8 ms na turę |

Sprawdzone, bez zysku (wycofane): przestawienie pętli pikseli `gpoly_lines_shaded_68k` pod 68060 –
pętla jest ograniczona łańcuchem dodawań z przeniesieniem, nie pamięcią (odczyt tekstury −0,7 ms,
zapis −1,2 ms, wszystkie piksele 8,4 ms przy 59 tys. pikseli na klatkę).

Do zrobienia: radar co 2. klatkę (opcja), wszystkie opcje szybkości w menu gry, opcje dla ARM.

## 15.11.2026 — asm gry jednoosobowej w C, stan wydajności

Cały asm używany w grze jednoosobowej jest w C (lista commitów od `0a89a2c3` do `9d3308ed`);
na Amidze 1 w misjach 1 i 109 nie jest wywoływana żadna zaślepka asm. Zostało 35 wywołań asm
z C: minigra BAT (7), sieć IPX/Radica (10), budowa siatki dróg z pliku `.dat` (6, gra wczytuje
gotową siatkę z `.mad`), nieużywane (4), grupy graczy i czat sieciowy (3), `process_things_*`
(gra sieciowa, 3), kamera przelotu w napisach (`traffic_unkn_func_01`).

Test rozkazów: `SW_TEST_CMDS` (wersja z fncheck) co kilkadziesiąt tur wydaje agentom rozkazy
i wywołuje rzadkie funkcje (pojazdy, przedmioty, kratery, kanistry, kapsuły, dusze, działka…).

Znalezione przy porównaniach błędy oryginału (zachowane albo opisane w commitach):
`set_angle_to_avoid_group` (kąt z niezainicjowanego stosu, C bierze kąt po obrocie),
`my_preprocess_text` (nazwa gracza czytana zza `net_host_player_no`, C używa `login_name`),
`map_things_unkn_func_04` (stary wrapper obcinał wynik do bajtu). Poprawiony błąd wcześniejszej
konwersji: `set_xz_for_quad_unkn03` (brak testu trójkąta).

Wydajność, Amiga 1 (A1200, Warp 1260, 68060), misja 1, 320×200, `PROF=1`: klatka 63,1 ms
(ok. 16 kl./s): `draw_screen` 27,9 ms, `draw_things` 6,6, `hud_panel` 6,1, symulacja 5,5,
podłoga 4,8, radar 3,6, `swap` 3,5. Profil próbkujący: rasteryzator asm (`gpoly_init_state`
i dalej) ok. 28% próbek, `engine_draw_things` 5%, `process_things` 3,9%, `draw_drawlist_2` 3,5%,
podłoga 3,3%, `draw_object_faces` 3%.

## 15.11.2026 — dźwięk na Amidze: Paula

Decyzje użytkownika: Paula domyślnie, AHI jako opcja; muzyka z płyty przekonwertowana na PC
(bez dekodowania Vorbis na Amidze); muzyka MIDI („danger music”) pominięta; najpierw 68060, potem ARM.

- **bfsoundlib kompilowany dla Amigi** bez `drv_oal.c` (OpenAL) i `oggvorbis.c`; zamiast nich
  `platform/amiga/snd_paula.c` z tymi samymi funkcjami `OPENAL_*` i `ogg_vorbis_stream_*`.
  Wymagało czytania nagłówków WAV i banku jako little endian (`b1b594c8`).
- **Efekty:** 4 kanały `audio.device` (przydział wszystkich na starcie, priorytet 127, bez czekania).
  Kanał wg strony (0 i 3 lewe, 1 i 2 prawe), strona wg `pan` jak w wersji PC (OpenAL): `pan > 64`
  lewo, `pan < 64` (poza 0) prawo – odwrotnie niż opis w `mssal.h`, zachowane jak w buildzie
  wzorcowym. Gdy brak wolnego kanału, przejmuje najcichszy (nie głośniejszy od nowego). Próbka bez
  kanału „gra” po cichu i kończy się w swoim czasie (zapętlona czeka na wolny kanał). Próbki do
  64 KB jednym zapisem (powtórzenia robi Paula: `ioa_Cycles`), dłuższe kawałkami po 64 KB
  (2 w kolejce). Głośność i okres zmieniane `ADCMD_PERVOL`. Licznik pętli `n > 1` grany n razy
  (OpenAL w PC gra raz – tam błąd sterownika, tu zachowanie MSS).
- **Bank efektów:** `tools/amiga/make_sound_bank.py` – WAV 16 bit → 8 bit **ze znakiem** (wbrew
  standardowi WAV; sterownik Pauli tego oczekuje), ten sam układ pliku, 10 → 5 MB. Także
  `syncreds.dat` (napisy końcowe).
- **Sterta dźwięków w Chip RAM** (Paula czyta tylko stamtąd), maks. 800 KB, z zapasem 200 KB
  Chip RAM dla systemu; sterta trzyma ostatnio używane próbki (8 bit – mieści dwa razy więcej
  niż 16 bit na PC).
- **Muzyka:** `tools/amiga/make_music.py` (ffmpeg) – ścieżki OGG → `track_N.smu`: nagłówek
  `SWMU`, 22 050 Hz, 8 bit ze znakiem, bloki po 8 KB: lewy kanał, potem prawy (czytane jednym
  `Read` wprost do bufora w Chip RAM). 3 bloki w kolejce (1,1 s). Muzyka zajmuje kanały 0 i 1,
  efekty mają wtedy 2 i 3; przy głośności 0 muzyka zwalnia kanały. ~78 MB na 3 ścieżki.
- **Katalog danych Amigi:** `tools/amiga/make_amiga_data.sh` robi `Syndicate Wars/amiga_data`
  z danych PC (twarde dowiązania, przekonwertowany dźwięk i muzyka, kopie `qdata` i konfiguracji).
  WinUAE (`SyndWars_A1200_OS32_net*.uae`, wolumin `SWARS:`) wskazuje teraz ten katalog (kopie
  starych konfiguracji: `*.uae.bak-pcdata`).
- Opcje: `ENV:SyndWarsSound` = `NONE` wyłącza dźwięk; `ENV:SyndWarsSoundLog` – liczniki co 5 s
  do `RAM:SyndWars-sound.txt`.
- ARM: na razie zaślepki dźwięku (`sound_stubs.c` pod `SW_WARP`).

Otwarte: odsłuch na prawdziwej Amidze, koszt CPU (pomiar na Amidze 1), `AIL_ms_count` przez
`gettimeofday` (sprawdzić rozdzielczość/koszt w libnix), wstrzymanie muzyki gubi zakolejkowane
~1 s, AHI.

### AHI (15.11.2026)

- `platform/amiga/snd_amiga.c` (dawniej `snd_paula.c`): Paula lub AHI (`ENV:SyndWarsSound=AHI`,
  bez AHI powrót do Pauli), `ENV:SyndWarsMusic=OFF` wyłącza muzykę.
- AHI: 8 głosów, prawdziwa panorama jak w PC; sterta dźwięków w Fast RAM (1,5 MB) jako jeden
  dźwięk AHI, próbki to jej fragmenty; koniec próbki liczony z czasu. Muzyka: dwa pierścienie
  (lewy/prawy) po 2 połówki po 2 bloki, ładowane jako dźwięki AHI przed startem odtwarzania;
  funkcja dźwięku (hak z argumentami w rejestrach) liczy rozpoczęte połówki, wolna jest
  dopełniana z dysku.
- **Błąd pamięci znaleziony przy tej okazji:** generator `gen_asm_data.py` dawał prywatnym
  tablicom struktur rozmiar do następnej etykiety GLOBAL, a w asm x86 wewnątrz
  `BAT_balls_store`/`BAT_bonus_store` są inne etykiety. Na Amidze tablice miały 32/16 B zamiast
  1280/384 B; `BAT_unknsub_20` (w C od `9d3308ed`, przy każdym starcie) pisało listy wolnych
  elementów po zmiennych za nimi (tu `AHIBase`). Rozmiary teraz z `sizeof` kompilatora m68k
  (`STRUCT_SIZE` w generatorze); inne takie tablice sprawdzone – mają zapas.

### Dźwięk w wersji ARM (15.11.2026)

- Silnik wydzielony: `platform/amiga/snd_engine.c` (Paula/AHI, sloty, muzyka; gcc i vbcc),
  `snd_bfs.c` (bfsoundlib → silnik, wspólny dla 68k i ARM), `snd_env.c` (opcje z ENV: na 68k).
- ARM: `platform/warp/snd_proxy.c` udaje silnik i wysyła rekordy `SWL_SND_*`/`SWL_MUS_*`
  (`swlink.h`); czas dźwięków liczy sam. Sterta dźwięku 1,5 MB na końcu obszaru render (na stercie
  ARM brak miejsca – gra zajmuje ~17,3 z 17,7 MB), zarejestrowana jako `SWL_BUF_SOUND`.
- 68k: `tools/warp/swhost_snd.c` – ten sam silnik; przy `SND_PLAY` pobiera bajty próbki
  (`extFetch`, min. 32 B) do pierścienia w stercie silnika (Chip RAM dla Pauli), nadpisane dźwięki
  zatrzymuje; muzykę czyta z dysku; budzi się co 20 ms. Opcje Paula/AHI/muzyka czyta `swhost`
  z ENV: i odsyła w `SND_OPEN`. `swhost` zostaje na vbcc (nagłówki `cswarp.library` tylko dla vbcc).
- Koszt na ARM: 0,4 ms (Paula) – 1,4 ms (AHI) na turę.

### Zapisy gry i ustawienia: jeden format, little endian (15.11.2026)

- Decyzja użytkownika: zapisy (`qdata/savegame/*.sav`), ustawienia (`default.ini`) i `keys.dat`
  mają na wszystkich platformach format PC (little endian), więc są wymienne między PC, 68060 i ARM.
- `src/game_save.c`: pola ustawień czytane i pisane przez `file_read_le`/`file_write_le` (i wersje
  tablicowe); bufor zapisu (format 12) przestawiany przez `save_buf_fix_order()` według układów
  struktur (`tools/amiga/struct_layout.py`: AgentInfo, ResearchInfo, PlayerInfo, MissionStatus)
  przed szyfrowaniem i po odszyfrowaniu. Klucz szyfru nakładany jako `lb_le32(key)`; suma kontrolna
  to XOR bajtów w 4 torach, więc zapisywana bez zmian. Na big endian starsze formaty (< 12) są
  odrzucane (zawierają fragmenty struktur).
- `PlayerInfo.MyAgent[4]` to wskaźniki, ważne tylko w misji (tak w oryginale); w zapisie różnią się
  między platformami i przebiegami, nie mają znaczenia po wczytaniu w menu.
- Test: `ENV:SyndWarsSaveTest=N` (PC `SW_SAVETEST`) w misji od tury N zapisuje grę do slotu 12
  (`swars011.sav`), wczytuje i zapisuje do slotu 13, porównuje pliki i kończy grę;
  `SyndWarsSaveTestLoad=1` (`SW_SAVETESTLOAD`) tylko wczytuje slot 12 (zapis z innej platformy).
  Bufor zapisu menu to strona tekstur, a przy `-m` nie jest ustawiony – test używa własnego.
- `tools/amiga/save_decrypt.py FILE.sav` odszyfrowuje zapis, `--diff A B` pokazuje różniące się
  bajty z nazwą pola.

### Wybór wersji ARM (15.11.2026)

- Decyzja użytkownika: jedna ikona `SyndWars`, Tool Type `ARM` (w ikonie jako podpowiedź `(ARM)`)
  uruchamia wersję ARM; bez niego gra na 68060. Z Shella: `SyndWars ARM [argumenty gry]`.
- ARM z własną pamięcią (SDRAM) mają tylko Warp 1260 Extreme i Warp 7060; `cswarp.library` bywa
  zainstalowana i na innych Warpach. Test (`platform/amiga/arm_launch.c`): `extXchg` bez danych –
  płyta bez SDRAM albo ze starym firmware odpowiada `CSWEXT_ERR_NODEV`, płyta z ARM `NOTREADY`
  (nic nie działa) / `OK` / `BUSY`. Bez ARM: okienko z wyjaśnieniem i gra na 68060.
- `swhost` jest uruchamiany przez `SystemTags` z wejściem/wyjściem `NIL:` (bez okna konsoli przy
  starcie z ikony), pełną ścieżką z `GetProgramDir()` i z katalogiem programu jako bieżącym
  (`PROGDIR:` nie istnieje w nowym procesie Shella). `SyndWars` czeka na jego koniec.
