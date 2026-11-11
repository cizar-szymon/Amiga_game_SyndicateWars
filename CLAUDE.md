# CLAUDE.md — Syndicate Wars na Amigę (port SyndWarsFX → AmigaOS 3.2 / 68060 / RTG)

## Cel projektu

Uruchomić Syndicate Wars na Amidze z AmigaOS 3.2.2.1, procesorem 68060 i kartą graficzną RTG (Picasso96),
na bazie fanowskiego portu **SyndWarsFX** (https://github.com/swfans/syndwarsfx, GPL3, dawniej `swfans/swars`).

Port upstream działa tylko na i386: część gry jest nadal zdezasemblowanym kodem x86, splecionym z C.
Dlatego projekt ma dwa duże etapy: **(A) doprowadzić kod do czystego, przenośnego C** i dopiero potem
**(B) zrobić warstwę platformową Amigi i optymalizację pod 68060**.

## Środowisko i sprzęt (UZUPEŁNIJ)

- Maszyny testowe (zob. `PORTING_PLAN.md`, „Środowisko testowe”): **Amiga 2: A4000 + Warp 7060**
  (główna, `C:/amiga-dev/amiagent-2.cfg`), Amiga 1: A1200 + Warp 1260 (`amiagent.cfg`), WinUAE.
  Testy na Amidze robi Claude sam przez AmiAgenta; użytkownik ocenia grę.
- Grafika: RTG Warpa (Picasso96) — `TODO: tryby 8-bit (320x200 / 640x480) do sprawdzenia na Warpie`
- Dźwięk: Paula (`audio.device`) domyślnie, AHI jako opcja (plan portowania)
- **Build referencyjny x86 (Windows, MSYS2 mingw32 / i686):** `tools/amiga/build_x86.sh [debug]`
  → `build/x86-release/src/syndwarsfx.exe`. Pakiety i686 z pacmana + OpenAL Soft 1.24.3
  zbudowany ze źródeł do `/mingw32` (pakietu brak w MSYS2), WildMIDI 0.5.0 z zipa (README upstream).
- Toolchain m68k: bebbo `m68k-amigaos-gcc` w Dockerze (obraz `amixcom-build`, jak w AmiGTA) —
  `TODO: potwierdzić przy etapie B`
- **Dane gry:** GOG w `Syndicate Wars/` (ignorowany przez gita): obraz płyty `SWARS/game.ins` +
  `game.gog`; rozpakowany `CD/` (`tools/amiga/cdimage.py`: pliki ISO + `track_1..3.wav`);
  gotowa instalacja do gry na PC: **`Syndicate Wars/syndwarsfx/`** (`util/install -M` + exe + DLL-e
  + `music/track_N.ogg` z ffmpeg). Gra czyta dane z bieżącego katalogu.
  **Danych gry nigdy nie commitujemy.**

## Struktura repozytorium

- Pracujemy na **osobnej gałęzi** (`amiga`), upstream trzymamy jako remote `upstream`.
- Katalogi upstream warte uwagi: `src/` (logika gry + pozostały asembler), `swrendersoft/` (programowy
  renderer 3D — główny kandydat do optymalizacji pod 68k), `bflibrary/`, `bfsoundlib/`, `bfsmacklib/`
  (biblioteki Bullfroga), `util/` (skrypty instalacyjne), `conf/`.
- Nasze dodatki:
  - `platform/amiga/` — warstwa systemowa AmigaOS (ekran, wejście, dźwięk, czas, pliki)
  - `docs/amiga/porting-notes.md` — dziennik decyzji i odkryć (prowadź go na bieżąco)
  - `docs/amiga/asm-inventory.md` — lista funkcji nadal w asemblerze i ich status
  - `docs/amiga/test-log.md` — wyniki testów zgłaszane przez użytkownika z prawdziwej Amigi / WinUAE

## Zasady pracy

1. **Nie psuj buildu x86.** Build i386 (Linux/WSL lub MSYS2 mingw32) jest naszą wersją referencyjną.
   Każda zmiana w logice gry musi dać się zbudować i uruchomić na x86, zanim trafi na Amigę.
2. Małe, osobne commity. Konwersja jednej funkcji asm→C = jeden commit z opisem, co i jak sprawdzono.
3. Nie zgaduj zachowania oryginału — porównuj z kodem asm i z buildem referencyjnym. Jeśli nie da się
   zweryfikować, zapisz to w `porting-notes.md` jako otwarte ryzyko.
4. Testy na Amidze: AmiAgent (`AMIAGENT.md`) — wgranie, start, zrzuty, klawisze robi Claude.
   Ocena grywalności, dźwięku i sterowania należy do użytkownika. Wyniki wpisz do `test-log.md`.
5. Przed większą zmianą (nowy etap, zmiana architektury warstwy platformowej) przedstaw plan i poczekaj
   na akceptację.
6. Upstream jest aktywnie rozwijany i sam przepisuje asm do C. Przed konwersją większego modułu sprawdź
   (`git fetch upstream`), czy nie został już przepisany. Konwersje, które są ogólnie przydatne,
   przygotowuj tak, żeby dało się je zgłosić upstream jako PR.
7. Kod Bullfroga (zdezasemblowany) ma inny status prawny niż kod GPL — nie przenoś go do nowych plików
   bez zaznaczenia pochodzenia.

## Etap A — przenośne C (na x86)

### A0. Inwentaryzacja
- Zbuduj wersję referencyjną x86 według README upstream (32-bit, `-m32 -fno-PIC -fno-PIE`).
- Policz, ile kodu jest nadal w asemblerze (pliki `.S`/`.asm`, wywołania `asm_*`, wrappery C↔asm).
- Utwórz `asm-inventory.md`: funkcja, rozmiar, kto ją woła, co woła, moduł (renderer / AI / UI / dźwięk / I/O),
  szacowana trudność. Na tej podstawie zaproponuj kolejność konwersji.

### A1. Konwersja asm → C
- Konwertuj funkcja po funkcji, zachowując identyczne zachowanie (łącznie z arytmetyką stałoprzecinkową,
  przepełnieniami, zaokrągleniami przy dzieleniu i przesunięciach liczb ze znakiem).
- Używaj typów o jawnym rozmiarze (`int8_t`, `uint16_t`, `int32_t`...), nigdy gołego `int` tam, gdzie
  rozmiar ma znaczenie.
- Po każdej konwersji: build x86 + uruchomienie testów z `swfans/swars-tests` (jeśli dostępne) + krótki
  test w grze opisany w commicie.
- Cel końcowy etapu: brak kodu x86 w buildzie, możliwość kompilacji z `-fPIC` i na innej architekturze.

### A2. Higiena przenośności (jeszcze na x86)
- **Endianness:** 68k to big-endian. Znajdź każde miejsce, gdzie dane z plików (mapy, levele, sprite'y,
  palety, sejwy, konfiguracja) są wczytywane przez `fread` prosto do struktur albo czytane przez rzutowanie
  wskaźników. Zastąp je jawnymi funkcjami deserializacji (`read_u16le`, `read_u32le` itd.).
- **Układ struktur:** gcc dla m68k wyrównuje `int`/`long` domyślnie do 2 bajtów, a nie do 4 jak na x86.
  Struktury odpowiadające formatom plików lub pamięci oryginału muszą mieć jawny układ —
  dodaj `_Static_assert(sizeof(...) == N)` dla każdej z nich.
- **Aliasing i sztuczki z unią:** szukaj odczytu bajtów z wartości 16/32-bitowych przez wskaźniki
  (`*(uint8_t*)&x`, `((uint16_t*)p)[1]`) — na big-endian dają inny wynik.
- Dobra metoda kontrolna: zbuduj wersję x86 z warstwą deserializacji i porównaj, że stan gry po wczytaniu
  levelu jest bit w bit taki sam jak wcześniej.

## Etap B — Amiga

### B1. Pierwsze uruchomienie
- Wariant szybki: jeśli w toolchainie jest SDL 1.2 dla m68k-amigaos (upstream obsługuje SDL1.2),
  użyj go do pierwszego uruchomienia. Wariant docelowy: natywna warstwa w `platform/amiga/`.
- Kompilacja: `m68k-amigaos-gcc -m68060 -O2 -fomit-frame-pointer -noixemul` (libnix).
  `TODO: potwierdzić flagi i bibliotekę C`.
- Najpierw tryb 320x200, dopiero potem 640x480.

### B2. Natywna warstwa platformowa
- **Obraz:** gra jest 8-bitowa z paletą — otwórz ekran RTG 8-bit (CLUT) przez `intuition.library` /
  `BestCModeIDTags` z `cybergraphics.library` / Picasso96. Blit bufora: `WritePixelArray8` na start,
  potem bezpośredni zapis przez `LockBitMapTags` i podwójne buforowanie. Palety przez `LoadRGB32`.
- **Wejście:** IDCMP (mysz, klawiatura), mapowanie klawiszy na kody z gry.
- **Czas:** `timer.device`, ogranicznik klatek zgodny z logiką upstream.
- **Dźwięk:** AHI zamiast OpenAL. Muzyka: Vorbis jest za ciężki dla 68060 w tle gry — rozważ Tremor
  (wersja całkowitoliczbowa), wcześniejszą konwersję do formatu łatwiejszego do odtwarzania albo
  wyłączenie muzyki na start. WildMIDI raczej pominąć.
- **Pliki:** ścieżki w stylu AmigaDOS (`PROGDIR:`), uwaga na wielkość liter i separatory.

### B3. Wydajność
- Profiluj na prawdziwym sprzęcie (WinUAE nie oddaje wydajności 68060). Raportuj FPS w trybie debug.
- Główne miejsca do optymalizacji: `swrendersoft/` (rasteryzacja, teksturowanie, sortowanie wielokątów),
  kopiowanie bufora na kartę, przeliczanie obiektów na mapie.
- 68060 nie ma sprzętowych instrukcji FPU dla funkcji przestępnych (są emulowane) — unikaj `sin`/`cos`/
  `sqrt` w gorących pętlach; preferuj tablice i arytmetykę stałoprzecinkową, jak oryginał.
- Ręczny asembler 68k tylko w udowodnionych gorących miejscach, zawsze z wersją C jako fallback.

## Definicja „gotowe” dla etapu

- Build x86 działa i przechodzi testy.
- Build Amigi kompiluje się bez ostrzeżeń dotyczących rozmiarów typów i wyrównania.
- Użytkownik potwierdził test na Amidze, wynik jest w `test-log.md`.
- `porting-notes.md` opisuje, co zostało zrobione i jakie są znane problemy.

## Linki

- SyndWarsFX: https://github.com/swfans/syndwarsfx
- Dane dodatkowe: `swfans/syndwarsfx-gfx`, `syndwarsfx-sfx`, `syndwarsfx-levels`
- Testy black-box: https://github.com/swfans/swars-tests
- Społeczność (pytania o stan dekompilacji): Discord z README upstream
