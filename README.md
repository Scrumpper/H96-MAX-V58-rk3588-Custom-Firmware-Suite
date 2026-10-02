# Scrumpper's H96 Max V58 Firmware Suite

One-stop installer for custom firmware on **H96 Max V58** (Rockchip RK3588) TV box.
Run one script, pick build, flash it.

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

Pick build from menu → confirm → it flashes.

## Layout

REQUIRED: Extract all .zip files into one folder as listed below.

Each build lives in **its own sub-folder** next to `install.py`, named after build.
Menu auto-detects whatever folders are present, so you can add or remove builds freely:

```
Scrumpper's H96 Max V58 Firmware Suite/
├── install.py                              ← run this
├── WHICH BUILD SHOULD I PICK.md
├── README.md
├── H96 MAX V58 Stripped Stock Android/     ← Android 12, Google/Play (recommended)
├── LineageOS 23 (Android 16)/              ← newest Android
└── Unofficial Armbian v6.2/                ← Linux, console base, desktop on demand
```

Each build folder is **self-contained**: it holds its own image(s) + `flash.py` (plus
`MiniLoaderAll.bin` and any partition files it needs). Menu runs that folder's flasher.
Folder names are matched loosely (case/punctuation don't matter); any folder with `flash*.py`
or image will still be offered.

## Requirements

- **Python 3.6+**
- **rkdeveloptool** on your PATH (flashers talk to box over USB)
  - Arch: `yay -S rkdeveloptool` · Debian/Ubuntu: build from `github.com/rockchip-linux/rkdeveloptool`
- **USB-A ↔ USB-A** cable to box, and box in **Maskrom** mode (flashers walk you
  through it. Power off, hold pinhole reset button (rear panel, in gap between two WiFi antenna posts), plug USB, connect power, release).
- Linux or macOS. (Windows: use flashers from WSL, or Rockchip's RKDevTool GUI.)

## ⚠️ Flashing wipes box

Every build fully erases and replaces what's on box. That's expected. All of them are
recoverable. RK3588 Maskrom lives in ROM, so paperclip on reset button always gets
you back to flashable state.

---


