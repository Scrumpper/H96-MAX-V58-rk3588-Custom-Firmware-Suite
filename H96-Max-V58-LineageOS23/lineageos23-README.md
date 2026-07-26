# H96 Max V58 — LineageOS 23 / Android 16 (RK3588)

Bring-up **sources** to run a **LineageOS 23 (Android 16) GSI** on the **H96 Max V58**
TV box (Rockchip **RK3588**, Mali-G610). As far as any public record shows, this is
the **first Android 16 GSI to boot on any RK3588 device**.

**This repository does not redistribute Android.** It does not contain the GSI, the
vendor firmware, or a pre-built image. It contains the **RK3588 bring-up recipe** —
the patches, the TV-box fixes, the front-panel/input daemons, and a build script that
takes the **upstream LineageOS GSI you download yourself** and repacks it to boot on
this box. Everything here is source, under GPL-2.0. See
[`lineageos23-LICENSE`](lineageos23-LICENSE) and [`lineageos23-CREDITS.md`](lineageos23-CREDITS.md).

> **Unofficial & experimental.** Not affiliated with LineageOS, Rockchip, or the H96
> manufacturer. The GSI used is a **raw AOSP / "pure-userdebug" arm64 GSI** — a
> **vanilla Android 16 experience**, not a LineageOS-featured build. It boots and is
> fully functional on our test unit, but this is brand-new territory. **No warranty.**
> Keep a way to restore stock before you flash.

---

## Hardware status (verified on this box)

| Component | Status | Notes |
|---|:---:|---|
| Boot / `boot_completed` | ✅ | first A16 GSI to boot on RK3588 |
| Internet / networking | ✅ | no A16 netfilter regression on the 5.10 vendor kernel |
| WiFi + Ethernet + Bluetooth | ✅ | via the stock vendor HALs |
| Mali-G610 GPU | ✅ | GLES 3.2 |
| Hardware video decode | ✅ | `c2.rk.avc.decoder` (H.264 + HEVC) |
| HDMI + audio | ✅ | + **HDMI auto-detect** fix (reads the real EDID) |
| Front-panel VFD | ✅ | clock + eth/wifi/usb/play icons |
| Launcher UI | ✅ | **app_widgets** fix stops the Trebuchet crash-loop |
| Google Play | ❌ | uncertified GSI — use Aurora Store / F-Droid instead |
| Widevine | L3 | SD-only DRM (hardware limit of the box) |

---

## Why a GSI (and why this repo looks the way it does)

LineageOS builds a **GSI** — a Generic System Image that boots on top of a device's
existing **vendor** partition. This box already has a working RK3588 vendor image
(from its stock firmware), so we don't build Android at all — we take the upstream GSI
and:

1. Patch it so the TV-box UI doesn't crash (**app_widgets** feature) and so the box
   behaves like a set-top (never-sleep, HDMI auto-detect, cursor fix).
2. Bake small helper daemons (front-panel display, right-click=Back, Win+H=HOME).
3. Repack it into a `super.img` next to the box's own vendor/odm/product partitions.

That means the reproducible **source** is the patch set + the build script here — not a
multi-gigabyte binary. You bring the GSI (from LineageOS) and the vendor side (from the
box's stock firmware); this repo supplies the RK3588 glue.

---

## Repository layout

```
lineageos23-build-super.sh       the recipe: patch the GSI + repack into super.img
lineageos23-FLASH.md             flashing (rkdeveloptool, Maskrom)
lineageos23-parameter_gsi.txt    GPT / partition layout (enlarged super)
fixes/
  lineageos23-appwidgets.xml     THE crash fix — declares android.software.app_widgets
  lineageos23-tvbox.rc           init service (runs the boot script)
  lineageos23-tvbox.sh           never-sleep + HDMI auto-detect + cursor fix (boot)
  lineageos23-frontpanel.c       front-panel VFD daemon (TM1650 via /dev/digital_dis)
  lineageos23-mouseback.c        right-click = Back (short=Back, long=real right-click)
  lineageos23-homekey.c          Win+H = HOME
  lineageos23-Makefile           builds the three daemons (aarch64 static)
lineageos23-CHANGELOG.md  lineageos23-CREDITS.md  lineageos23-LICENSE
```

---

## Build

You provide two inputs (neither is redistributed here):

1. **The GSI** — download the LineageOS 23 (Android 16) **arm64 / `arm64_bvN`** GSI
   from the LineageOS GSI project, gzip it (or keep the `.img.gz`), and place it as
   `./lineage-23-gsi_arm64.img.gz`.
2. **The stock super** — the H96 Max V58 factory `super.img` (its vendor/odm/product
   partitions), placed as `./stock-super.img` (or `.xz`).

Then:

```bash
# 1. build the helper daemons (aarch64 cross toolchain; set CROSS= if your prefix differs)
make -C fixes                    # or: make -C fixes CROSS=aarch64-linux-android31-

# 2. build the patched super (TMPDIR must be on real disk — ~7 GB of intermediates)
TMPDIR=./build-tmp  bash lineageos23-build-super.sh
#   -> ./new_super.img

# optional: drop *.apk into ./apps/ first to bake FOSS apps as system apps
#   (the reference build used F-Droid, Aurora Store, Kodi, VLC, NewPipe,
#    Material Files, FLauncher, AdAway). None are redistributed here.
```

By default the build is **release-clean** (`DEV_ADB=0`). For headless debugging you can
build with `DEV_ADB=1` to bake network adb on TCP 5555 — do **not** ship that.

Flash `new_super.img` per [`lineageos23-FLASH.md`](lineageos23-FLASH.md).

---

## What each fix does

- **`lineageos23-appwidgets.xml`** — the big one. This TV vendor omits the
  `android.software.app_widgets` feature, so the GSI launcher's `AppWidgetManager` is
  null and Trebuchet/Eleven crash-loop (the screen "blinks"). Declaring the feature via
  `/system/etc/permissions/` fixes it. This is what makes the A16 GSI usable here.
- **`lineageos23-tvbox.sh`** (run by `lineageos23-tvbox.rc` at boot):
  - **Never-sleep** — the box has no working suspend/resume, so screen-off looks like
    power-off; forces always-on.
  - **HDMI auto-detect** — the DDC hardware works but the kernel's boot-time EDID read
    races the HDMI PHY; the script forces a hotplug re-detect so the compositor reads the
    display's **real EDID** and switches to its native mode (any TV up to 4K / 8K30).
  - **Cursor fix** — forces GPU composition (`SurfaceFlinger 1008`) to bypass a Rockchip
    HWC cursor-plane bug that blanks HDMI on mouse-move.
- **`lineageos23-frontpanel.c`** — drives the TM1650 front panel (`/dev/digital_dis`):
  clock + live eth/wifi/usb/play icons. Reads Android local time via `/system/bin/date`
  (a static glibc binary's own `localtime()` would show UTC).
- **`lineageos23-mouseback.c`** / **`lineageos23-homekey.c`** — passive evdev→uinput
  input daemons: right-click→Back, and Win+H→HOME. They don't grab devices, so normal
  input is untouched.

---

## Flashing & limits

See [`lineageos23-FLASH.md`](lineageos23-FLASH.md). In brief: flash over USB with
`rkdeveloptool` in Maskrom mode, writing the box's stock small partitions (from the
factory firmware) + this `new_super.img`. The USB port is **host-only** once Android is
up, so runtime debugging is over **network adb** (if you built with `DEV_ADB=1`).

Known limits: **no Google Play** (uncertified GSI → use Aurora/F-Droid), **Widevine L3**
(SD DRM — a hardware limit of the box), HDMI-CEC may not work, no touchscreen (USB mouse
works). Vanilla-AOSP rough edges apply.

---

## License & credits

GPL-2.0-only for the code in this repo. The LineageOS/AOSP GSI is under its own licenses
and is **not** included — download it from LineageOS. See
[`lineageos23-LICENSE`](lineageos23-LICENSE) and
[`lineageos23-CREDITS.md`](lineageos23-CREDITS.md).
