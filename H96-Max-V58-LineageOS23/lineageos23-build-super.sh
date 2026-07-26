#!/usr/bin/env bash
# H96 Max V58 (RK3588) — build a flashable LineageOS 23 (Android 16) super.img.
#
# This does NOT build Android. It takes the upstream LineageOS 23 arm64 GSI (which
# you download yourself — see lineageos23-README.md), patches it to boot and run on
# this TV box (the app_widgets crash fix, TV-box init tweaks, front-panel/input
# daemons, optional FOSS apps), then repacks it into a super.img alongside the box's
# own vendor partitions (taken from the stock firmware).
#
# Needs: simg2img, lpunpack, lpmake, debugfs/e2fsck/resize2fs (e2fsprogs), unzip, gzip.
#        An aarch64 C toolchain to build the fixes/ daemons (see fixes/lineageos23-Makefile).
#        rkdeveloptool to flash (see lineageos23-FLASH.md).
#
# RUN WITH TMPDIR ON REAL DISK (not a small tmpfs): the ~7 GB of intermediates overflow
#   /tmp on many systems.  e.g.   TMPDIR=./build-tmp bash lineageos23-build-super.sh
#
# Usage:
#   lineageos23-build-super.sh [GSI.img.gz] [stock-super.img] [out.img]
# Inputs (override by arg or env):
#   GSI_GZ      upstream LineageOS 23 arm64 GSI, gzipped   (default ./lineage-23-gsi_arm64.img.gz)
#   STOCK_SUPER the box's stock super.img (for vendor side) (default ./stock-super.img[.xz])
#   OUT         output image                                (default ./new_super.img)
#   APPDIR      optional dir of *.apk to bake as system apps (default ./apps, skipped if empty)
#   DEV_ADB     1 = bake headless network adb for debugging (default 0 = clean release)
#   ADB_PUBKEY  host adb key to pre-authorize when DEV_ADB=1 (default ~/.android/adbkey.pub)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
GSI_GZ="${1:-${GSI_GZ:-$HERE/lineage-23-gsi_arm64.img.gz}}"
STOCK_SUPER="${2:-${STOCK_SUPER:-$HERE/stock-super.img}}"
OUT="${3:-${OUT:-$HERE/new_super.img}}"
APPDIR="${APPDIR:-$HERE/apps}"
DEV_ADB="${DEV_ADB:-0}"
ADB_PUBKEY="${ADB_PUBKEY:-$HOME/.android/adbkey.pub}"
FIXDIR="$HERE/fixes"

[ -f "$GSI_GZ" ]     || { echo "missing GSI: $GSI_GZ  (download the LineageOS 23 arm64 GSI — see README)"; exit 1; }
[ -f "$STOCK_SUPER" ] || { [ -f "$STOCK_SUPER.xz" ] && xz -dk "$STOCK_SUPER.xz"; }
[ -f "$STOCK_SUPER" ] || { echo "missing stock super: $STOCK_SUPER  (from the box's factory firmware)"; exit 1; }

# Build the fixes/ daemons if the compiled binaries aren't present yet.
for bin in androidboox-frontpanel androidboox-mouseback androidboox-homekey; do
  [ -x "$FIXDIR/$bin" ] || { echo "== building fixes/ daemons =="; make -C "$FIXDIR"; break; }
done

WORK="$(mktemp -d)"; echo "work dir: $WORK"
round4k(){ echo $(( ($1 + 4095) / 4096 * 4096 )); }

echo "== unpack stock super (vendor-side partitions) =="
simg2img "$STOCK_SUPER" "$WORK/super.raw" 2>/dev/null || cp "$STOCK_SUPER" "$WORK/super.raw"
mkdir -p "$WORK/parts"; lpunpack "$WORK/super.raw" "$WORK/parts" >/dev/null

echo "== decompress LineageOS 23 GSI (new system) =="
gunzip -c "$GSI_GZ" > "$WORK/system.img"
GSI="$WORK/system.img"
printf 'u:object_r:system_file:s0\0'    > "$WORK/sysctx.bin"
printf 'u:object_r:adb_keys_file:s0\0'  > "$WORK/adbctx.bin"

bake(){ # <src> <destpath> <mode> [ctxfile]
  local ctx="${4:-$WORK/sysctx.bin}"
  debugfs -w "$GSI" >/dev/null 2>&1 <<EOF
rm $2
write $1 $2
sif $2 mode $3
sif $2 uid 0
sif $2 gid 0
ea_set -f $ctx $2 security.selinux
EOF
}

# bake a system APK at /system/app/<Name>/<Name>.apk + extract its arm64-v8a libs
# (system-app APKs are NOT auto-unpacked -> native apps hit UnsatisfiedLinkError).
bakeapp(){ # <src-apk> <AppName>
  local src="$1" name="$2" d="/system/app/$2"
  debugfs -w "$GSI" >/dev/null 2>&1 <<EOF
mkdir $d
sif $d mode 040755
sif $d uid 0
sif $d gid 0
ea_set -f $WORK/sysctx.bin $d security.selinux
write $src $d/$name.apk
sif $d/$name.apk mode 0100644
sif $d/$name.apk uid 0
sif $d/$name.apk gid 0
ea_set -f $WORK/sysctx.bin $d/$name.apk security.selinux
EOF
  local lt="$WORK/lib_$name"; rm -rf "$lt"; mkdir -p "$lt"
  if unzip -o -j "$src" 'lib/arm64-v8a/*.so' -d "$lt" >/dev/null 2>&1 && ls "$lt"/*.so >/dev/null 2>&1; then
    debugfs -w "$GSI" >/dev/null 2>&1 <<EOF
mkdir $d/lib
sif $d/lib mode 040755
sif $d/lib uid 0
sif $d/lib gid 0
ea_set -f $WORK/sysctx.bin $d/lib security.selinux
mkdir $d/lib/arm64
sif $d/lib/arm64 mode 040755
sif $d/lib/arm64 uid 0
sif $d/lib/arm64 gid 0
ea_set -f $WORK/sysctx.bin $d/lib/arm64 security.selinux
EOF
    local so base
    for so in "$lt"/*.so; do
      base=$(basename "$so")
      debugfs -w "$GSI" >/dev/null 2>&1 <<EOF
write $so $d/lib/arm64/$base
sif $d/lib/arm64/$base mode 0100644
sif $d/lib/arm64/$base uid 0
sif $d/lib/arm64/$base gid 0
ea_set -f $WORK/sysctx.bin $d/lib/arm64/$base security.selinux
EOF
    done
  fi
}

echo "== bake fixes (DEV_ADB=$DEV_ADB) =="
if [ "$DEV_ADB" = 1 ]; then
  # DEV ONLY: headless network adb (raw AOSP GSI ships no ro.adb.secure=0). Strip for release.
  debugfs -R "cat /system/build.prop" "$GSI" 2>/dev/null > "$WORK/build.prop"
  cat >> "$WORK/build.prop" <<'EOF'

# dev: headless network adb (DEV ONLY, strip for release)
persist.adb.tcp.port=5555
ro.adb.secure=0
persist.sys.usb.config=adb
EOF
  bake "$WORK/build.prop" /system/build.prop 0100600
  [ -f "$ADB_PUBKEY" ] && { cp "$ADB_PUBKEY" "$WORK/adb_keys"; bake "$WORK/adb_keys" /adb_keys 0100644 "$WORK/adbctx.bin"; }
fi

# TV-box fixes. NOTE: repo source files are lineageos23-*; they bake to the runtime
# paths the init .rc expects (androidboox-* — internal to the image).
bake "$FIXDIR/lineageos23-appwidgets.xml" /system/etc/permissions/androidboox-appwidgets.xml 0100644
bake "$FIXDIR/lineageos23-tvbox.rc"       /system/etc/init/androidboox-tvbox.rc 0100644
bake "$FIXDIR/lineageos23-tvbox.sh"       /system/etc/androidboox-tvbox.sh 0100755
bake "$FIXDIR/androidboox-frontpanel"     /system/bin/androidboox-frontpanel 0100755   # front-panel VFD: clock + eth/wifi/usb/play icons
bake "$FIXDIR/androidboox-mouseback"      /system/bin/androidboox-mouseback 0100755   # right-click = Back (short=Back, long=real right-click)
bake "$FIXDIR/androidboox-homekey"        /system/bin/androidboox-homekey 0100755   # Win+H = HOME

# Optional: bake every APK in $APPDIR as a /system/app system app (vanilla AOSP ships none).
if [ -d "$APPDIR" ] && ls "$APPDIR"/*.apk >/dev/null 2>&1; then
  echo "== bake FOSS apps from $APPDIR =="
  e2fsck -f -y "$GSI" >/dev/null 2>&1 || true
  LIBB=0; for a in "$APPDIR"/*.apk; do LIBB=$((LIBB + $(unzip -l "$a" 2>/dev/null | awk '/lib\/arm64-v8a\/.*\.so/{s+=$1} END{print s+0}'))); done
  APKMB=$(( ($(cat "$APPDIR"/*.apk | wc -c) + LIBB) / 1048576 + 128 ))
  resize2fs "$GSI" $(( $(stat -c%s "$GSI")/1048576 + APKMB ))M >/dev/null 2>&1 || true
  for apk in "$APPDIR"/*.apk; do bakeapp "$apk" "$(basename "$apk" .apk)"; done
fi
e2fsck -f -y "$GSI" >/dev/null 2>&1 || true

echo "== verify bakes (fail loud) =="
if [ "$DEV_ADB" = 1 ]; then
  debugfs -R "cat /system/build.prop" "$GSI" 2>/dev/null | grep -q 'persist.adb.tcp.port=5555' || { echo FATAL adb; exit 1; }
else
  debugfs -R "cat /system/build.prop" "$GSI" 2>/dev/null | grep -q 'persist.adb.tcp.port' && { echo "FATAL: dev adb NOT stripped"; exit 1; }
  echo "  (DEV_ADB=0: no network adb — clean release)"
fi
for f in /system/etc/permissions/androidboox-appwidgets.xml \
         /system/etc/init/androidboox-tvbox.rc /system/etc/androidboox-tvbox.sh \
         /system/bin/androidboox-frontpanel /system/bin/androidboox-mouseback /system/bin/androidboox-homekey; do
  debugfs -R "stat $f" "$GSI" 2>/dev/null | grep -q 'Inode:' || { echo "FATAL missing $f"; exit 1; }
done

echo "== rebuild super (A16 system fits the 4.5 GiB container) =="
G=rockchip_dynamic_partitions
SYS=$(round4k $(stat -c%s "$GSI"))
SE=$(round4k $(stat -c%s "$WORK/parts/system_ext.img"));  VN=$(round4k $(stat -c%s "$WORK/parts/vendor.img"))
VD=$(round4k $(stat -c%s "$WORK/parts/vendor_dlkm.img")); OD=$(round4k $(stat -c%s "$WORK/parts/odm.img"))
ODD=$(round4k $(stat -c%s "$WORK/parts/odm_dlkm.img"));   PR=$(round4k $(stat -c%s "$WORK/parts/product.img"))
lpmake --device-size=4831838208 --metadata-size=65536 --metadata-slots=2 --group="$G:4827643904" \
  --partition="system:readonly:$SYS:$G"       --image="system=$GSI" \
  --partition="system_ext:readonly:$SE:$G"    --image="system_ext=$WORK/parts/system_ext.img" \
  --partition="vendor:readonly:$VN:$G"        --image="vendor=$WORK/parts/vendor.img" \
  --partition="vendor_dlkm:readonly:$VD:$G"   --image="vendor_dlkm=$WORK/parts/vendor_dlkm.img" \
  --partition="odm:readonly:$OD:$G"           --image="odm=$WORK/parts/odm.img" \
  --partition="odm_dlkm:readonly:$ODD:$G"     --image="odm_dlkm=$WORK/parts/odm_dlkm.img" \
  --partition="product:readonly:$PR:$G"       --image="product=$WORK/parts/product.img" \
  --force-full-image --output="$OUT"
echo "== done: $OUT ($(stat -c%s "$OUT") bytes). Flash per lineageos23-FLASH.md. rm -rf $WORK when done. =="
