# Cannonball PS3 — Enhanced OutRun Engine

**PlayStation 3 Port by `theheroGAC`**  
*Title ID: `CNBLPS300` • Engine: Cannonball `0.35-PS3` (based on `djyt/cannonball`)*

Cannonball is a modern re-implementation of Sega's arcade classic **OutRun**, adding widescreen `16:9` gameplay, `60 FPS` smooth frame pacing, optional bug fixes, Time Trial and Continuous modes, selectable Ferrari colors, custom soundtrack support, and DualShock 3 analog driving controls on Sony PlayStation 3.

---

## Important Copyright Notice (ROMs Not Included)

**OutRun arcade ROMs are copyrighted by SEGA and are NOT included in this repository or in the `.pkg` builds.**  
You must provide your own legally dumped **OutRun (Revision B)** MAME ROM files (loose files or `outrun.zip`).

If required files are missing at startup, Cannonball PS3 automatically opens a **Missing Game Data** window listing all `31` required Revision B ROMs, their expected CRC32 checksums, and `2` bundled engine data files (`tilemap.bin`, `tilepatch.bin`), along with one-button USB-to-HDD import (`Square`).

---

## Key Features (PS3 Edition)

- **Animated Boot Splash & Custom XMB Artwork**: Dedicated `ICON0.PNG` (`320x176`) and `PIC1.PNG` (`1920x1080`) OutRun Synthwave artwork plus a skippable 720p animated splash screen (`X` / `START`).
- **720p OutRun Synthwave Menu (`ps3_ui`)**:
  - **PLAY**: Direct launch for *Enhanced (60FPS / 16:9 / Hi-Res)*, *Original (4:3)*, *Continuous (15 Stages)*, *Time Trial*, or Cannonball's *Classic Road Menu*.
  - **SAVES**: Persistent XML settings & arcade/Time Trial/Continuous high scores plus **3 Backup Profile Slots** and optional Auto-Mirroring.
  - **VIDEO**: TV Aspect Ratio (`AUTO`, `16:9 WIDE`, `4:3 CLASSIC`, `STRETCH`), Widescreen toggle, Hi-Res sprites (`640x448` / `320x224`), FPS mode (`30 FPS`, `Original 30/60`, `60 FPS`), CRT Scanlines (`0–100%`), Nearest / Bilinear filter, TV Overscan (`0–8%`), Shadow intensity, and FPS badge.
  - **AUDIO**: 48 kHz Cell Audio mixer, volume control, attract sound, radio preview, Fixed PCM ROM (`opr-10188.71f`) toggle, and **Live Jukebox**.
  - **ENGINE**: Course order (`World` / `Japanese`), live preview of Ferrari paint color (`Red`, `Blue`, `Yellow`, `Green`, `Cyan`), timer/traffic difficulty, Freeplay, bug fixes, level objects, and handling cheats (*Grippy Tyres*, *Off-Road*, *Turbo + Armor*).
  - **CONTROLS**: 3 DualShock 3 presets, analog/digital modes, gearbox styles, steering/pedal speed, and controller rumble intensity.
  - **ABOUT**: ROM rescan, USB-to-HDD ROM importer, screenshot capture, splash toggle, and full port credits.
- **In-Game PS3 Quick Pause Menu (`SELECT` or `L3 + R3`)**: Save settings & scores on the fly, change aspect/scanlines/filter/volume, capture PNG screenshots, open the Classic Road Menu, return to the PS3 Menu, or exit to XMB.
- **Missing Game Data Window**: Full 42-entry ROM & resource verifier with scrollable CRC32 table, `[X] Rescan Storage`, `[Square] Copy ROMs from USB`, and `[Start] Exit to XMB`.

---

## Installing ROMs (Manual User Setup)

Place your **OutRun Revision B** files (extracted or inside `outrun.zip`) in **one** of these locations:

1. **PS3 Internal HDD (Recommended)**:
   - `/dev_hdd0/cannonball/roms/`
   - or `/dev_hdd0/cannonball/roms/outrun/`
   - or `/dev_hdd0/ROMS/cannonball/`
2. **Inside Installed Game Folder**:
   - `/dev_hdd0/game/CNBLPS300/USRDIR/roms/`
3. **USB Drive (Auto-Detected & Importable)**:
   - `/dev_usb000/cannonball/roms/` (or `/dev_usb000/roms/outrun/`, `/dev_usb000/outrun/`, `/dev_usb000/outrun.zip`)
   - Press **Square** in the *Missing Game Data* window or *ABOUT* tab to copy ROMs from USB directly to `/dev_hdd0/cannonball/roms/`.

### Required & Optional Files

- **Required Engine Data** (`2` files, bundled inside `USRDIR/res/`):
  - `tilemap.bin`, `tilepatch.bin`
- **Required OutRun Rev B Arcade ROMs** (`31` files):
  - Program CPU A: `epr-10380b.133`, `epr-10382b.118`, `epr-10381b.132`, `epr-10383b.117`
  - Program CPU B: `epr-10327a.76`, `epr-10329a.58`, `epr-10328a.75`, `epr-10330a.57`
  - Tile GFX: `opr-10268.99`, `opr-10232.102`, `opr-10267.100`, `opr-10231.103`, `opr-10266.101`, `opr-10230.104`
  - Road GFX: `opr-10185.11`, `opr-10186.47`
  - Sprites GFX Bank 0 & 1: `mpr-10371.9`, `mpr-10373.10`, `mpr-10375.11`, `mpr-10377.12`, `mpr-10372.13`, `mpr-10374.14`, `mpr-10376.15`, `mpr-10378.16`
  - Z80 Sound Program: `epr-10187.88`
  - Sega PCM Samples: `opr-10193.66`, `opr-10192.67`, `opr-10191.68`, `opr-10190.69`, `opr-10189.70`, `opr-10188.71`
- **Optional ROMs**:
  - `8` Japanese Revision B ROMs (unlocks Japanese track order)
  - `opr-10188.71f` (optional fixed PCM sample ROM)

---

## DualShock 3 Controls

| Button / Stick | Action |
| :--- | :--- |
| **Left Stick / D-Pad** | Steer Ferrari |
| **R2** (Analog) / **Cross (`X`)** | Accelerate |
| **L2** (Analog) / **Square** | Brake |
| **R1 / L1 / Triangle** | Shift Gear (High / Low / Toggle) |
| **Circle (`O`)** | Change Camera View |
| **START** | Insert Coin / Start Race |
| **SELECT** or **L3 + R3** | Open PS3 In-Game Quick Pause Menu |
| **L2 + R2 + START** | Open Classic Cannonball Road Menu |
| **R3** | Capture PNG Screenshot (`/dev_hdd0/cannonball/screenshots/`) |

---

## HDD Storage & Saves Layout

- **ROMs**: `/dev_hdd0/cannonball/roms/`
- **Saves & Configuration**: `/dev_hdd0/cannonball/saves/`
  - `config.xml`, `ps3_settings.ini`
  - `hiscores.xml`, `hiscores_jap.xml`
  - `hiscores_timetrial.xml`, `hiscores_timetrial_jap.xml`
  - `hiscores_continuous.xml`, `hiscores_continuous_jap.xml`
  - Profile slots: `/dev_hdd0/cannonball/saves/slots/slot1..slot3/`
- **Custom Music**: `/dev_hdd0/cannonball/music/`
- **Screenshots (PNG)**: `/dev_hdd0/cannonball/screenshots/`
- **Debug Log**: `/dev_hdd0/cannonball/cannonball_ps3.log`

---

## Building from Source

Requires **PS3DEV / PSL1GHT** (`ppu-g++` + `portlibs/ppu` with `SDL2`, `SDL2_ttf`, `freetype`, `libpng`, `zlib`):

```bash
./build.sh
./package.sh
```

Output binaries are generated at:
- `cannonball_ps3.elf`
- `dist/Cannonball_PS3.pkg`
- `dist/Cannonball_PS3.gnpdrm.pkg`

See [CREDITS.md](CREDITS.md) and [docs/license.txt](docs/license.txt) for full attribution and license terms.
