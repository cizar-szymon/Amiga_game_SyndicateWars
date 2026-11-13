Syndicate Wars - Amiga 68060, wersja testowa (DEMO)
===================================================

Port fanowskiej wersji SyndWarsFX (https://github.com/swfans/syndwarsfx, GPL3)
na AmigaOS 3.2 z procesorem 68060 i karta graficzna RTG.

Wymagania
---------
- Amiga z procesorem 68060 (testowano: A4000 + Warp 7060), Fast RAM 64 MB+
- AmigaOS 3.2, Picasso96 (RTG), ekran 8-bitowy 320x200 i 640x480
- ok. 120 MB miejsca na dysku (paczka ZIP ok. 40 MB)

Instalacja i uruchomienie
-------------------------
  UnZip SyndWars-68060-demo.zip -d Work:Games
Start: ikona SyndWars albo w Shellu:
  CD Work:Games/SyndWars
  SyndWars

Opcje szybkosci (SYSTEM -> VISUAL DEPTH)
----------------------------------------
  FAR PEDESTRIANS  ON: przechodnie poza ekranem i radarem stoja (szybciej,
                   bez zmian w rozgrywce); domyslnie ON
  RADAR HALF RATE  ON: radar rysowany co 2. klatke; domyslnie OFF
Zapis ustawien: SYSTEM -> INTERFACE -> SAVE.

Klawisze
--------
  F8      przelaczenie rozdzielczosci w misji: 320x200 (szybciej) / 640x480
  Ctrl-C  w Shellu konczy gre

Stan tej wersji
---------------
- Brak dzwieku i muzyki.
- Brak filmow Smacker (czolowka, filmy w menu) - sa pomijane.
- Menu, logowanie i pierwsza misja dzialaja; dalsze misje moga sie
  zawiesic albo zachowywac inaczej niz na PC (czesc procedur gry nie jest
  jeszcze przeniesiona z asemblera x86).
- Predkosc na 68060: ok. 15-16 tur gry/s w 320x200 w misji 1,
  mniej w ciezszych scenach i w 640x480 (pelne tempo gry to 16 tur/s).
- Log gry: error.log w katalogu gry.

Dane gry pochodza z wersji GOG Syndicate Wars - paczka jest tylko do
prywatnych testow i nie moze byc rozpowszechniana.


Syndicate Wars - Amiga 68060, test build (DEMO)
===============================================

Port of SyndWarsFX to AmigaOS 3.2 with a 68060 and an RTG graphics card.
Needs a 68060 (tested on A4000 + Warp 7060), 64 MB+ Fast RAM, AmigaOS 3.2,
Picasso96. Unpack with UnZip into Work:Games, start the SyndWars icon or
`SyndWars` in the game directory. F8 switches 320x200 / 640x480 in a mission.
No sound, no music, no videos yet; menus, login and the first mission work,
later missions may not. The game data comes from the GOG release - this
package is for private testing only, do not distribute it.
