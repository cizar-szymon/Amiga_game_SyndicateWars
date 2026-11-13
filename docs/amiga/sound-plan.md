# Dźwięk: Paula czy AHI, który procesor (analiza 13.11.2026)

## Co gra ma do zagrania

- **Efekty i mowa:** jeden bank w danych GOG, `sound/sound_eng.dat` – wariant `1622`
  (16 bitów, 22 050 Hz, mono), **ok. 10 MB próbek** (inne warianty banku: 8 bitów, 11/44 kHz – w tych
  danych puste). Gra trzyma część próbek w „stercie dźwięków” (`sound_heap_s.sx`, `heapmgr_*`) –
  wczytuje je na żądanie, bo w DOS-ie też nie mieściły się w pamięci.
- **Do 10 próbek naraz** (`audOpts.MaxSamples = 10`, `game.c`), z głośnością, panoramą i wysokością
  zależną od odległości i kierunku (`play_dist_sample`), część zapętlona (np. pająk-mech).
- **Muzyka:** ścieżki CD jako OGG Vorbis 44,1 kHz stereo (`music/track_1..3.ogg`, 11–13 MB każda).
- Dziś na Amidze i ARM wszystko to są zaśleplone funkcje asm (8 funkcji: `play_sample_using_heap`,
  `play_dist_sample`, `process_sound_heap`, `monitor_all_samples`, `stop_sample_using_heap`,
  `fill_ail_sample_ids`, `heapmgr_*`) – przed odtwarzaniem trzeba je przepisać na C
  (na x86 z `fncheck`, jak resztę).

## Ograniczenia sprzętu

| | Paula wprost | AHI (miksowanie programowe) |
|---|---|---|
| głosy | 4 sprzętowe (2 lewe, 2 prawe) | dowolnie dużo, wszystkie 10 |
| format | 8 bitów (14 bitów z dwóch kanałów na stronę) | 16 bitów, panorama płynna |
| pamięć próbek | **Chip RAM** (A1200: 2 MB razem z ekranem i systemem) | Fast RAM |
| koszt CPU | prawie zero (DMA) | miksowanie 10 głosów 22 kHz – na 68060 kilka % czasu (w WoW: ~5% przy 060/50) |
| panorama | twarda: każdy głos lewy albo prawy | dowolna |

Bank 10 MB w 16 bitach (5 MB po zamianie na 8 bitów) **nie zmieści się w Chip RAM** – przy Pauli
wprost próbki musiałyby być kopiowane do Chip RAM na żądanie (sterta dźwięków gry), a z 10 głosów
gra musiałaby wybrać 4 (najgłośniejsze / najbliższe).

## Ile czasu ma każdy procesor

- **Wersja ARM** (gra liczona na ARM Warpa): 68060 jest prawie bezczynny – w 640×480 pobiera klatkę
  (ok. 5,7 ms na turę 62,5 ms), poza tym czeka. ARM jest zajęty grą w ~90% (55 ms z 62,5 w 640×480).
- **Wersja 68k** (bez ARM): 68060 liczy wszystko i jest za wolny (ok. 79 ms na turę w 320×200) –
  każdy procent dla dźwięku to mniej klatek.

## Zalecenie

**Wersja ARM – miksowanie na ARM, Paula gra gotowy strumień.**
- ARM ma próbki w swojej pamięci (19 MB, bank 10 MB się mieści, bez sterty na żądanie) i miesza
  10 głosów do jednego strumienia stereo 22 050 Hz (koszt rzędu 2–3 mln cykli na sekundę, **~0,5%
  ARM**), z dokładną głośnością, panoramą i wysokością jak na PC.
- Muzykę ARM może dekodować z OGG (integer Vorbis/Tremor – szacunkowo 5–10% ARM przy 44,1 kHz;
  albo raz przekonwertować na 22 kHz i tylko miksować).
- 68060 pobiera strumień kawałkami (`extFetch`, ~60 MB/s: 88 KB/s stereo 16-bit to ~1,5 ms na
  sekundę) do dwóch małych buforów w Chip RAM i daje je Pauli – **4 kanały, 14 bitów stereo** (albo
  8 bitów na 2 kanałach). Chip RAM: tylko ~2 × 8 KB buforów. Koszt 68060 pomijalny, a i tak jest wolny.
- Zalety: wszystkie głosy naraz, jakość jak na PC, prawie zero kosztu dla obu procesorów, brak
  problemu z Chip RAM. Do zrobienia: strumień przez łącze (rekord „AUDIO” i bufor pierścieniowy
  ~0,25 s, żeby chwilowe spóźnienie ARM nie dawało trzasków), podwójne buforowanie Pauli na
  przerwaniu audio na 68k.

**Wersja tylko 68k – Paula wprost, 4 głosy.**
- AHI kosztowałoby kilka procent czasu 68060, którego tu brakuje najbardziej.
- Próbki zamienione na 8 bitów przy wczytaniu, trzymane w stercie w Chip RAM (rozmiar np. 512 KB),
  wybór 4 głosów z 10 według głośności; muzyka – osobna decyzja (np. wyłączona albo z dysku na
  2 kanałach kosztem 2 głosów efektów).
- AHI jako opcja (`SOUND=AHI`) dla Amig z kartą dźwiękową.

Kolejność prac: (1) funkcje dźwięku asm → C na x86 (fncheck), (2) wersja ARM: miksowanie na ARM +
strumień do Pauli, (3) wersja 68k: Paula wprost.
