# Credits

This build stands on other people's work.

- **LineageOS** and the **LineageOS GSI project** — the Android 16 generic system
  image this recipe patches and repacks. <https://lineageos.org> — download the GSI
  from the official project; it is not redistributed here.
- **AOSP** / the **Android Open Source Project** — the base the GSI is built from.
- **Rockchip** — the RK3588 vendor image (WiFi/Ethernet/BT/HDMI/VPU/GPU HALs) that the
  GSI boots on top of; taken from the box's own stock firmware, not included here.
- The **Android GSI community** (AndyYan and others) building the `arm64` GSIs that make
  this possible.
- Bundled FOSS apps, if you choose to bake them: **F-Droid**, **Aurora Store**, **Kodi**,
  **VLC**, **NewPipe**, **Material Files**, **FLauncher**, **AdAway** — each the property
  of its authors, downloaded by you, not redistributed here.

The front-panel daemon (`lineageos23-frontpanel.c`) was reverse-engineered from the stock
Android kernel's front-display driver (TM1650 protocol). The input daemons are original.

Developed conversationally with **Claude Code** — every fix, daemon, and build step was
written and tested interactively on a real unit. Offered in good faith, no warranty.
