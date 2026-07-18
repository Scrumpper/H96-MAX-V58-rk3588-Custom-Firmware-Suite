# Scrumpper's H96 Max V58 Firmware Suite

A one-stop installer for custom firmware on the **H96 Max V58** (Rockchip RK3588) TV box.
Run one script, pick a build, flash it.

```
   ██╗  ██╗ █████╗  ██████╗    ██╗   ██╗███████╗ █████╗
   ██║  ██║██╔══██╗██╔════╝    ██║   ██║██╔════╝██╔══██╗
   ███████║╚██████║███████╗    ██║   ██║███████╗╚█████╔╝
   ██╔══██║ ╚═══██║██╔═══██╗    ╚██╗ ██╔╝╚════██║██╔══██╗
   ██║  ██║ █████╔╝╚██████╔╝     ╚████╔╝ ███████║╚█████╔╝
   ╚═╝  ╚═╝ ╚════╝  ╚═════╝       ╚═══╝  ╚══════╝ ╚════╝
```

## Use it

```bash
python3 install.py
```

Pick a build from the menu → confirm → it flashes.

## Layout

REQUIRED: Extract all .zip files into one folder as listed below.

Each build lives in **its own sub-folder** next to `install.py`, named after the build.
The menu auto-detects whatever folders are present, so you can add or remove builds freely:

```
Scrumpper's H96 Max V58 Firmware Suite/
├── install.py                              ← run this
├── WHICH BUILD SHOULD I PICK.md
├── README.md
├── H96 MAX V58 Stripped Stock Android/     ← Android 12, Google/Play (recommended)
├── LineageOS 23 (Android 16)/              ← newest Android
├── Unofficial Armbian v3.1/                ← desktop Linux
└── Unofficial Armbian v3/                  ← desktop Linux (older)
```

Each build folder is **self-contained** it holds its own image(s) + a `flash.py` (plus the
`MiniLoaderAll.bin` and any partition files it needs). The menu just runs that folder's flasher.
Folder names are matched loosely (case/punctuation don't matter); any folder with a `flash*.py`
or an image will still be offered.

## Requirements

- **Python 3.6+**
- **rkdeveloptool** on your PATH (the flashers talk to the box over USB)
  - Arch: `yay -S rkdeveloptool`  ·  Debian/Ubuntu: build from `github.com/rockchip-linux/rkdeveloptool`
- A **USB-A ↔ USB-A** cable to the box, and the box in **Maskrom** mode (the flashers walk you
  through it. Power off, hold the pinhole reset in the AV jack, plug USB, connect power, release).
- Linux or macOS. (Windows: use the flashers from WSL, or Rockchip's RKDevTool GUI.)

## ⚠️ Flashing wipes the box

Every build fully erases and replaces what's on the box. That's expected. All of them are
recoverable. The RK3588 Maskrom lives in ROM, so a paperclip on the reset button always gets
you back to a flashable state.

---


