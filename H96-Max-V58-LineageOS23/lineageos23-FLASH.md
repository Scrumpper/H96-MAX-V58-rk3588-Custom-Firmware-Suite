# Flashing LineageOS 23 (Android 16) to the H96 Max V58 (RK3588)

Flash over USB with **`rkdeveloptool`** in **Maskrom** mode. You write the box's
**stock small partitions** (from its factory firmware) plus the **`new_super.img`**
you built with `lineageos23-build-super.sh`.

> ⚠️ This **erases the box** (a full userdata wipe is required for the enlarged super).
> Make sure you can restore the stock firmware before you start.

## You need
- `rkdeveloptool` installed.
- `new_super.img` — built per the README.
- The **stock small partitions** from the H96 Max V58 factory firmware, in one folder:
  `MiniLoaderAll.bin`, `uboot.img`, `misc.img`, `dtbo.img`, `vbmeta.img`, `boot.img`,
  `recovery.img`, `baseparameter.img`.
- `lineageos23-parameter_gsi.txt` (the enlarged-super GPT, in this repo).

## Reach Maskrom
From a running box: `adb reboot loader`. Then:
```bash
rkdeveloptool ld           # shows 'Loader' or 'Maskrom'
rkdeveloptool ef           # erase (also the required userdata wipe)
rkdeveloptool rd           # reset -> Maskrom
rkdeveloptool ld           # confirm 'Maskrom'
```
(If USB never appears, enter Maskrom by hand: power off, hold the recessed pinhole
reset — often inside the AV jack — plug USB, release after ~3 s.)

## Write
Run from the folder holding the stock small partitions; point the vars at your files.
```bash
STOCK=.                                   # folder with the stock small partitions
SUPER=/path/to/new_super.img
PARAM=/path/to/lineageos23-parameter_gsi.txt

rkdeveloptool db  "$STOCK/MiniLoaderAll.bin"
rkdeveloptool gpt "$PARAM"
rkdeveloptool wl 0x4000   "$STOCK/uboot.img"
rkdeveloptool wl 0x8000   "$STOCK/misc.img"
rkdeveloptool wl 0xa000   "$STOCK/dtbo.img"
rkdeveloptool wl 0xc000   "$STOCK/vbmeta.img"
rkdeveloptool wl 0xc800   "$STOCK/boot.img"
rkdeveloptool wl 0x24800  "$STOCK/recovery.img"
rkdeveloptool wl 0x1dc800 "$STOCK/baseparameter.img"
rkdeveloptool wl 0x1e5000 "$SUPER"        # the A16 super (~4.5 GB) — do NOT interrupt
rkdeveloptool ul "$STOCK/MiniLoaderAll.bin"
rkdeveloptool rd
```

> **Do not interrupt the `wl 0x1e5000` (super) write.** Killing a bulk USB write
> mid-transfer can wedge the USB endpoint; if that happens, re-enter Maskrom and retry.
> Write it uninterrupted (allow several minutes for ~4.5 GB).

> **Sub-1080p displays:** the HDMI auto-detect fix re-detects the real EDID a few
> seconds after boot, but during the pre-detect window a very small monitor can be
> black. If needed, use a `boot.img` whose kernel cmdline sets a safe fallback
> (`video=HDMI-A-1:1280x720@60`); the stock `boot.img` works on normal TVs.

## Verify (if you built with DEV_ADB=1)
```bash
# find it on the LAN, then:
adb connect <ip>:5555 && adb root && adb connect <ip>:5555
adb shell getprop ro.build.version.release   # -> 16
```

## Rollback
Re-flash the stock firmware (or any other build) with the same `rkdeveloptool` flow.
The Maskrom loader in mask ROM cannot be erased, so the box is always recoverable via
the pinhole-reset method above.
