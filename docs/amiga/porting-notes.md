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
| A1. Konwersja asm → C | – |
| A2. Higiena przenośności (endianness, układ struktur) | – |
| B. Amiga | – |

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

## Otwarte ryzyka

- Ok. 56,5 tys. żywych instrukcji x86 logiki gry (`swars.sx`) — główny koszt etapu A.
  Upstream przepisuje ok. 30 funkcji miesięcznie; trzeba się z nim zgrać.
- Brak (jeszcze) narzędzia do porównania stanu gry asm vs C po konwersji funkcji.
