# Dziennik testów

| data | wersja / commit | platforma | kto | wynik |
|---|---|---|---|---|
| 11.11.2026 | upstream `5c4b0d56`, syndwarsfx 0.3.8.4599, build `tools/amiga/build_x86.sh` (release) | PC Windows, i386 (MSYS2 mingw32), dane GOG + paczki upstream, muzyka CD → OGG | Claude | start, intro Smackera, brak błędów w `error.log` (15 s) |
| 11.11.2026 | j.w. | j.w. | użytkownik | **działa, gra się dobrze** – wersja referencyjna zaakceptowana jako wzorzec |
| 12.11.2026 | `be21c421`, build/amiga/SyndWars (bebbo gcc 6.5.0b, -m68060 -O2) | WinUAE A1200 68060 + JIT, RTG uaegfx 640x480x8 | Claude | misja 1 (`-q -m 0,1`): obraz zgodny z PC (agenci, budynki, HUD, skaner), ~48 kl./s (emulator - nie jest pomiarem wydajności); Ctrl-C kończy grę |
| 12.11.2026 | j.w. | **Amiga 1: A1200 + Warp 1260, 68060**, AmigaOS 3.2.2.1, RTG Warpa | Claude | misja 1: obraz zgodny z PC, **~5 kl./s** (log co 5 s, 330 klatek w 73 s); świat zamrożony (symulacja = zaślepki asm), bez dźwięku; Ctrl-C kończy grę |
