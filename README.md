# Syndicate Wars for the Amiga with a Warp card

**Syndicate Wars** (Bullfrog, 1996) on the Amiga: [SyndWarsFX](https://github.com/swfans/syndwarsfx),
the fans' open source version of the PC game, ported to AmigaOS 3.

**This is a port for the Amiga 500 / 1200 / 3000 / 4000 with a Warp accelerator card:
Warp 560, Warp 1240, Warp 1260, Warp 1260 Extreme or Warp 7060.** It was ported and tested on
these machines - an A1200 with a Warp 1260 Extreme and an A4000 with a Warp 7060, both with a
68060 - and tuned for them. The game runs whole on the 68060; on the **Warp 1260 Extreme and
Warp 7060** it can also run on the card's ARM, with the 68060 showing the picture and playing
the sound and the videos. Other Amigas with a 68060 and a Picasso96 card run it as well, but
slower.

- the game of SyndWarsFX: the campaigns, missions, menus, research, equipment, saved games (a beta:
  the port is new, bug reports are welcome)
- 320x200 or 640x480 (F8 in a mission), 8-bit screens on the graphics card
- **the ARM version** (Warp 1260 Extreme, Warp 7060): full speed at 640x480 even in the heavy
  scenes with many explosions; one icon for both versions (Tool Type `ARM`)
- sound through **Paula or AHI**, the music of the game's CD
- **the videos** (intro, elections, endings, mission end) with their sound, on the 68060
- speed options in the game (SYSTEM -> VISUAL DEPTH): far pedestrians, radar every 2nd frame,
  lower explosion detail while the game is slow, the game's speed in % on the screen
- saved games and settings in the PC's format: they go between the Amiga and SyndWarsFX on a PC
- **bug reports from the game**: the Help key opens a window that sends your description, the
  screen and the state of the game to the game server of the Amiga ports
- Esc asks before aborting a mission or quitting the game

## Play it

The ready-to-unpack release is in [`releases/amiga`](releases/amiga): `SyndWars-<version>.zip`
(also unpacked next to it). On the Amiga:

    UnZip SyndWars-<version>.zip -d Work:Games

Then make the game's data on a PC (below) and copy it into the drawer. Step by step:
[`Install.txt`](releases/amiga/SyndWars/doc/Install.txt). The description for players:
[`SyndWars_en.txt`](releases/amiga/SyndWars/doc/SyndWars_en.txt) (Polish:
[`SyndWars.txt`](releases/amiga/SyndWars/doc/SyndWars.txt)), the release history:
[`SyndWars_release_en.txt`](releases/amiga/SyndWars/doc/SyndWars_release_en.txt).

**Requirements:** an Amiga with a Warp card (or another 68060 Amiga with a Picasso96 card,
slower), AmigaOS 3.2, Picasso96 with 8-bit screens of 320x200 and 640x480, about 64 MB Fast RAM
and 300 MB on the disk; for the ARM version a Warp 1260 Extreme or Warp 7060 (`cswarp.library`
30.41 or newer); a TCP/IP stack (e.g. Roadshow) for the bug reports.

**Settings:** Tool Types of the SyndWars icon (in brackets in the package, remove them to switch
on): `ARM` - the ARM version, `SOUND=AHI` (or `PAULA`, `NONE`), `MUSIC=OFF`. The same words work in
the Shell: `SyndWars ARM SOUND=AHI`. The speed options are in the game's menu and saved with the
other options.

### The game's data is not included

The graphics, levels, sounds, music and videos are files of the original **Syndicate Wars**,
copyrighted by Bullfrog / Electronic Arts. They are not in this repository: they come from **your
own copy of the game bought from GOG**. The release has a tool for a PC (Windows, Linux, macOS)
that needs only Python 3:

    python SyndWarsData.py "C:\GOG Games\Syndicate Wars" SyndWarsData

It reads the GOG version's CD image, downloads the three data packages of SyndWarsFX from GitHub
(levels, graphics, sounds), turns the sounds and the music into the Amiga's formats and writes a
folder to copy into the game's drawer on the Amiga.

## Bug reports

Press **Help** in the game: it saves the screen and the state of the game, you write your e-mail
and what happened, and SEND gives it all to the game server of the Amiga ports. The version of
the port is in the corner of the main menu and in the Shell (`Version SyndWars`).

## Building

- the 68060 program: bebbo's m68k-amigaos-gcc 6.5 (libnix) in Docker -
  `sh tools/amiga/build_amiga.sh` -> `build/amiga/SyndWars`
- `swhost`, the 68k host of the ARM version: vbcc with the NDK 3.2, AHI, Roadshow and the
  `cswarp.library` SDK - `sh tools/warp/build_host.sh`
- `swcore.wxt`, the game for the ARM: the Warp's ARM toolchain and extension key, which are not
  public - `sh tools/warp/build_arm.sh`
- the reference PC build (i386, MSYS2) - `tools/amiga/build_x86.sh`
- the release - `sh tools/amiga/make_release.sh` (refuses to put any game data into the public
  archive)

The notes of the port, its decisions and measurements: [`docs/amiga`](docs/amiga). The README of
SyndWarsFX itself: [`README_SyndWarsFX.md`](README_SyndWarsFX.md).

## Licence

GPL-3.0, as SyndWarsFX ([`COPYING`](COPYING)). The video decoder is libsmacker (LGPL-2.1,
[`third_party/libsmacker`](third_party/libsmacker)). Syndicate Wars is (c) Bullfrog / Electronic
Arts; the game's data comes from your own copy.
