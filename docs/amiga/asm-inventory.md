# Inwentaryzacja asemblera x86 (etap A0)

Stan na upstream `5c4b0d56` (5.11.2026, „Remade new_bang()”), analiza z 11.11.2026.
Narzędzie: `python tools/amiga/asm_inventory.py [--md PLIK] [--csv PLIK] [--dead PLIK]`.
Pełna tabela żywych funkcji: [`asm-inventory-table.md`](asm-inventory-table.md).

## Metoda

Analiza statyczna, bez buildu:

1. Każdy linkowany plik `.sx` (lista z `src/Makefile.am`) jest dzielony na funkcje (`GLOBAL_FUNC`,
   etykiety najwyższego poziomu w `.text`; etykiety `jump_*` należą do bieżącej funkcji) i obiekty
   danych (`.data`: tablice skoków `vtable_*`, `data_*`).
2. Z każdego obiektu zbierane są odwołania do symboli.
3. Korzenie: symbole asm użyte w żywym kodzie C/C++ (`src`, `bflibrary`, `bfsoundlib`, `bfsmacklib`,
   `swrendersoft`, `bfariadne`) po usunięciu komentarzy i bloków `#if 0`. Tak upstream wyłącza starą
   ścieżkę po przepisaniu funkcji (`asm volatile ("call ASM_...")` w `#if 0`).
4. Osiągalne = domknięcie grafu odwołań od korzeni.

Wynik to **górne oszacowanie**: funkcja C z `asm volatile`, której sama gra już nie woła, nadal liczy
się jako korzeń. Dokładną listę da mapa linkera / analiza wywołań w zbudowanym `syndwarsfx.exe`
(po zbudowaniu wersji referencyjnej).

## Wynik

| plik | funkcje | instrukcje | żywe funkcje | żywe instrukcje | moduł |
|---|---:|---:|---:|---:|---|
| `src/swars.sx` | 881 | 102 118 | 432 | **56 535** | logika gry: ludzie, pojazdy, rzeczy, ścieżki, budynki, eksplozje |
| `bfsmacklib/src/bfsmacklib_s.sx` | 315 | 7 953 | 296 | 6 693 | odtwarzacz Smacker (filmy) |
| `src/radica1/2/3.sx` | 59 | 6 565 | 49 | 6 298 | sieć (protokół gry sieciowej) |
| `src/netdos/netipx1/netser1.sx` | 88 | 3 775 | 39 | 2 374 | sieć DOS: IPX, port szeregowy |
| `src/bat_s.sx` | 35 | 2 779 | 8 | 1 148 | minigra „Breakout” (BAT) |
| `bfsoundlib/src/ailssa.sx` | 204 | 3 611 | 62 | 1 115 | Miles Sound System (emulacja) |
| `src/sound_smp_s.sx`, `sound_heap_s.sx` | 21 | 1 417 | 16 | 1 383 | dźwięk: próbki, sterta |
| `src/thing_expld_s.sx`, `thing_fire_s.sx` | 44 | 6 359 | 13 | 571 | eksplozje, ogień |
| `src/bflib_vidraw_s.sx` | 8 | 1 597 | 0 | 0 | (martwy) |
| **razem** | **1 655** | **136 174** | **915** | **~76 100** | |

- **740 funkcji (~60 tys. instrukcji) jest już martwych**: przepisane w C, ale asm nadal leży w plikach.
  Przy przenośnym buildzie po prostu ich nie linkujemy.
- Renderer 3D (`swrendersoft/`) i biblioteka `bflibrary` (poza DOS-owym joystickiem, który nie jest
  linkowany) są już w C.
- W `swars.sx` 165 żywych funkcji (~21 tys. instrukcji) jest wołanych tylko z asm (wewnętrzne
  podprocedury). Reszta ma bezpośredni korzeń w C. Najwięcej: `people.c` (68 funkcji, 8,5 tys. instr.),
  `vehicle.c` (35 / 5,5 tys.), `thing.c` (46 / 5,5 tys.), `game.c`, `pathtrig.c`, `building.c`.
- Tempo upstream: w 2026 ok. 170 commitów „Remade/Routed”, od sierpnia ok. 30 funkcji na miesiąc.
  Przy tym tempie sam `swars.sx` zniknie z gry za ok. 1–1,5 roku.

## Proponowana kolejność (do akceptacji)

Kod poza logiką gry na Amidze i tak zastępujemy, więc nie trzeba go wiernie przepisywać:

1. **Build przenośny bez modułów DOS** (flaga konfiguracji, np. `--disable-x86-asm-modules`):
   - sieć (`radica*`, `netdos`, `netipx1`, `netser1`, ~8,7 tys. instr.) → zaślepki w C
     (gra jednoosobowa; sieć ewentualnie później przez TCP, jak w Settlers / OpenDUNE),
   - Smacker (`bfsmacklib_s.sx`, ~6,7 tys.) → dekoder Smackera w C (np. libsmacker, LGPL;
     do sprawdzenia licencji) albo na początek pominięcie filmów,
   - Miles / dźwięk (`ailssa.sx`, `sound_*_s.sx`, ~2,5 tys.) → warstwa dźwięku w C (na Amidze Paula),
   - minigra BAT (~1,1 tys.) → przepisanie (mały, zamknięty moduł) albo wyłączenie.
2. **Logika gry `swars.sx` (~56,5 tys. instr.)** w kolejności modułów, równolegle z upstream
   i w uzgodnieniu z nim (Discord), żeby nie robić tej samej funkcji dwa razy:
   najpierw liście grafu (funkcje bez wywołań asm), potem moduły od najmniejszych zależności:
   `thing_expld`/`thing_fire` → `bmbang` → `weapon` → `building` → `pathtrig` → `vehicle` → `people`
   → `thing` → `game`.
3. Każda przepisana funkcja: build x86, ten sam przebieg gry z porównaniem stanu z wersją asm
   (do ustalenia: nagrywanie wejść + porównanie stanu co klatkę, jak lockstep w Vastar / Xain'd Sleena).

## Otwarte pytania

- Czy upstream ma narzędzie do porównywania stanu (asm vs C)? `swars-tests` sprawdzić po zbudowaniu.
- Dokładna lista żywych funkcji z mapy linkera (po zbudowaniu x86).
