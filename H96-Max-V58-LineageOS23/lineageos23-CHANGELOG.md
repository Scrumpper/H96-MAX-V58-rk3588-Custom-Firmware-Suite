# Changelog — LineageOS 23 (Android 16) on the H96 Max V58

## Current
- **First Android 16 GSI to boot on an RK3588 box.** Boots to a stable UI with the
  `app_widgets` fix; internet, WiFi/Ethernet/Bluetooth, HDMI audio, hardware video
  decode, and the Mali-G610 GPU all work via the stock vendor HALs.
- **`app_widgets` crash fix** — declares `android.software.app_widgets` so the GSI
  launcher stops crash-looping on this TV vendor image.
- **HDMI auto-detect** — forces a post-boot hotplug re-detect so the compositor reads
  the display's real EDID and switches to its native mode (any TV up to 4K / 8K30),
  plus a cursor-plane workaround (force GPU composition).
- **Never-sleep** boot service (this box's suspend looks like a power-off).
- **Front-panel VFD daemon** — clock + eth/wifi/usb/play icons on the TM1650 panel,
  with correct Android-local time.
- **Input daemons** — right-click = Back, Win+H = HOME (passive evdev→uinput, so normal
  input is untouched).
- Optional **FOSS app baking** (F-Droid, Aurora Store, Kodi, VLC, NewPipe, Material
  Files, FLauncher, AdAway) — drop APKs in `apps/` and rebuild. None are redistributed.
- Release-clean by default (`DEV_ADB=0`); opt-in headless network adb for debugging.

## Notes
- The GSI is a raw AOSP / "pure-userdebug" arm64 image — a **vanilla Android 16**
  experience, not a LineageOS-featured build.
- No Google Play (uncertified GSI). Widevine L3 (SD DRM) — a hardware limit of the box.
