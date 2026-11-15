# Credits — Cannonball PS3

## PlayStation 3 Port & Frontend
- **theheroGAC** — PS3 port architecture, PSL1GHT / Cell PPU backend (`ps3_platform`, `ps3_video`, `ps3_audio`, `ps3_input`, `ps3_font`, `ps3_ui`), 720p OutRun Synthwave UI, animated boot splash screen, Missing Game Data verification & USB importer dialog, XMB `ICON0.PNG` / `PIC1.PNG`, in-game Quick Pause Menu, 3-slot persistent save profile manager, Boost-free `minixml` / `zlib` ROM & `.zip` loader.

## Core Engine & Console Reference
- **Chris White (`djyt`)** — Creator of [Cannonball](https://github.com/djyt/cannonball) (0.35 Enhanced OutRun Engine), widescreen tilemap patches, enhancements, and tools.
- **rsn8887** — Reference for console control defaults, analog trigger handling, and portable controller usability improvements.
- **MAME Contributors** — Reference FM (`ym2151`) and Sega PCM (`segapcm`) sound chip implementations included in Cannonball.

## SDK & Open-Source Libraries
- **PS3DEV & PSL1GHT Teams** — PlayStation 3 Cell PPU GCC toolchain, RSX/GCM system libraries, `ioPad`, `audio`, `sysutil`, `sprxlinker`, `make_self_npdrm`, `sfo`, `pkg`, and `package_finalize`.
- **SDL2 & SDL2_ttf (PSL1GHT port)** — Windowing, timing, WAV loading, and FreeType glyph rendering (`SDL_PSL1GHTrender.o`).
- **FreeType, libpng, zlib** — Font rasterization, PNG screenshot export, CRC32 ROM verification, and `.zip` decompression.

## Legal & Copyright Notice
- **OutRun** is a trademark of **SEGA Corporation**. This project is an independent homebrew engine port and is **not affiliated with or endorsed by SEGA**.
- **No copyrighted OutRun arcade ROM files are included** in the source code, release archive, or PS3 `.pkg` installer. Users must supply their own legally obtained OutRun (Revision B) arcade ROM set.
- Original Cannonball source code and modifications are distributed under the non-commercial terms in [docs/license.txt](docs/license.txt).
