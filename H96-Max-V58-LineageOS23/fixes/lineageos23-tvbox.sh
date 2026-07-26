#!/system/bin/sh
# androidboox TV-box adaptations, applied at every boot (boot_completed trigger).
# Runs SYNCHRONOUSLY — a backgrounded ( ) & gets killed when this oneshot init
# service's main process exits, so everything must run inline.

# 1) Never sleep: this box has no working suspend/resume, so screen-off == appears
#    powered off. Force always-on.
settings put system screen_off_timeout 2147483647
settings put global stay_on_while_plugged_in 7

# 2) HDMI auto-detect (reads whatever display is plugged in).
#    The DDC hardware works, but the kernel's boot-time EDID read races the HDMI PHY
#    and fails, so the box comes up on a fallback mode. Force a hotplug re-detect: the
#    HWComposer reads the display's REAL EDID and switches the output to its native
#    mode. Works for any monitor/TV, up to what RK3588 can drive (4K / 8K30).
C=/sys/class/drm/card0-HDMI-A-1
for i in 1 2 3 4 5 6 7 8; do
  echo off > $C/status 2>/dev/null
  sleep 2
  echo detect > $C/status 2>/dev/null
  sleep 1
  echo change > $C/uevent 2>/dev/null
  sleep 3
  n=$(wc -c < $C/edid 2>/dev/null)
  [ "${n:-0}" -gt 0 ] && break
done

# 3) Cursor fix — MUST run AFTER auto-detect. The Rockchip HWC cursor plane blanks
#    HDMI on every mouse move (the real EDID did NOT fix this on its own). Force all
#    composition through the Mali GPU (backdoor 1008 = mDebugDisableHWC) — this
#    bypasses the broken cursor plane. 1004 = force a repaint. This disables HWC
#    LAYER composition only; the HWC still manages displays/modes, so a later TV swap
#    still hotplug-switches resolution. Fine on a TV box (GPU composites the whole
#    screen; HW video DECODE is unaffected).
service call SurfaceFlinger 1008 i32 1
service call SurfaceFlinger 1004 i32 1
