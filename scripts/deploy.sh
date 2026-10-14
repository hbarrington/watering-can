#!/usr/bin/env bash
# Sync a firmware project (and the rest of the repo, minus vendor/build/git
# cruft) to the Raspberry Pi ("watering-can") that has the ESP32/ESP32-P4
# boards attached over USB, then build and flash/OTA it there.
#
# Run this from your dev machine, not the Pi.
#
# Usage:
#   scripts/deploy.sh                          # src/ on esp32, build + flash over USB
#   scripts/deploy.sh --monitor                # ...then attach the serial monitor
#   scripts/deploy.sh --menuconfig             # ...run menuconfig first (interactive)
#   scripts/deploy.sh --port /dev/ttyACM0
#   scripts/deploy.sh --pot --monitor          # pot module (Unit PoE-P4), shorthand for the env vars below
#   scripts/deploy.sh --src                    # back to the main module (the default, spelled out)
#   scripts/deploy.sh --pot --flash-only --monitor   # skip the build, just reflash the already-built binary
#   PROJECT_DIR=pot TARGET=esp32p4 scripts/deploy.sh --monitor   # same as --pot, via env vars (handy for CI/scripting)
#   scripts/deploy.sh --ota --ota-host 192.168.3.185       # build, then push over WiFi instead of USB
#   scripts/deploy.sh --ota --ota-host 192.168.3.185 --flash-only   # skip the build, just re-push the last binary
#
# Override any of these via environment variables if your setup differs:
#   REMOTE_HOST   (default: hunter@watering-can)
#   REMOTE_PATH   (default: ~/watering-can)
#   PROJECT_DIR   (default: src)        -- which project folder to build (src, pot, ...)
#   TARGET        (default: esp32)      -- idf.py set-target value (esp32, esp32p4, ...)
#   IDF_EXPORT    (default: $REMOTE_PATH/vendor/esp-idf-v5.5/export.sh --
#                  ESP-IDF lives inside the synced repo on the Pi, so this
#                  tracks REMOTE_PATH unless you override it directly)
#   PORT          (default: /dev/ttyACM0 for --pot/esp32p4, /dev/ttyUSB0 otherwise --
#                  the esp32p4's native USB-Serial/JTAG shows up as ttyACM on
#                  Linux, unlike the esp32 boards' CP2102/CH340 ttyUSB bridges)
#   OTA_HOST      (default: none -- required when --ota is used) -- the
#                  device's current IP/hostname on the WiFi network. Same
#                  as --ota-host.
#   OTA_PORT      (default: 80) -- must match CONFIG_WC_HTTP_PORT if you've
#                  changed it from the default in menuconfig.
#
# --pot/--src are just shorthand that set PROJECT_DIR/TARGET for you, and
# take precedence over the env vars above if both are given for the same run.
#
# --flash-only skips `idf.py build` entirely and pushes/flashes whatever was
# built last time (still useful even though `idf.py build` is itself
# incremental -- e.g. when the last build succeeded and only the flash/OTA
# step failed, for a non-code reason like a bad cable, wrong port, or a
# stale --ota-host IP).
#
# --ota requires the web_api component's OTA endpoint to already be running
# on the device (src/ only, as of writing) -- i.e. you've already flashed
# it over USB at least once since the partition table switched to
# CONFIG_PARTITION_TABLE_TWO_OTA. It builds on the Pi as usual, then runs
# `curl -X POST --data-binary @<bin> http://$OTA_HOST:$OTA_PORT/ota` from
# the Pi instead of flashing over USB -- no need to unmount the box. The
# device reboots into the new firmware immediately on success. Find its
# current IP from the serial log's "IPv4 address: ..." line (or your
# router's DHCP client list) if you don't already know it -- it's not
# static unless you've set up a DHCP reservation.
#
# Note: the esp32p4 target (PROJECT_DIR=pot) needs its own toolchain
# (riscv32-esp-elf). If the remote build fails with a missing-toolchain
# error, run `install.sh esp32p4` there once (source the same export.sh
# first), then re-run this script.

set -euo pipefail

REMOTE_HOST="${REMOTE_HOST:-hunter@watering-can}"
REMOTE_PATH="${REMOTE_PATH:-~/watering-can}"
PROJECT_DIR="${PROJECT_DIR:-src}"
TARGET="${TARGET:-esp32}"
IDF_EXPORT="${IDF_EXPORT:-$REMOTE_PATH/vendor/esp-idf-v5.5/export.sh}"
PORT="${PORT:-}"  # resolved below, after --pot/--src are parsed (default depends on target)
OTA_HOST="${OTA_HOST:-}"
OTA_PORT="${OTA_PORT:-80}"

DO_MONITOR=0
DO_MENUCONFIG=0
FLASH_ONLY=0
OTA_MODE=0

while [ $# -gt 0 ]; do
    case "$1" in
        --monitor) DO_MONITOR=1 ;;
        --menuconfig) DO_MENUCONFIG=1 ;;
        --port) PORT="$2"; shift ;;
        --pot) PROJECT_DIR="pot"; TARGET="esp32p4" ;;
        --src) PROJECT_DIR="src"; TARGET="esp32" ;;
        --flash-only) FLASH_ONLY=1 ;;
        --ota) OTA_MODE=1 ;;
        --ota-host) OTA_HOST="$2"; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
    shift
done

if [ "$OTA_MODE" -eq 1 ] && [ -z "$OTA_HOST" ]; then
    echo "--ota requires --ota-host <device IP> (or the OTA_HOST env var)." >&2
    exit 1
fi

# Default serial port depends on target: esp32p4's native USB-Serial/JTAG
# (used over its OTG/firmware-download port) shows up as /dev/ttyACM0 on
# Linux, while the esp32 boards' CP2102/CH340 USB-UART bridges show up as
# /dev/ttyUSBx. Only applied if neither the PORT env var nor --port was given.
if [ -z "$PORT" ]; then
    case "$TARGET" in
        esp32p4) PORT="/dev/ttyACM0" ;;
        *) PORT="/dev/ttyUSB0" ;;
    esac
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Syncing $REPO_ROOT to $REMOTE_HOST:$REMOTE_PATH"
rsync -avz --delete \
    --exclude='.git/' \
    --exclude='vendor/' \
    --exclude='build/' \
    --exclude='src/sdkconfig' \
    --exclude='src/sdkconfig.old' \
    --exclude='pot/sdkconfig' \
    --exclude='pot/sdkconfig.old' \
    --exclude='samples/relay-demo/build/' \
    "$REPO_ROOT/" "$REMOTE_HOST:$REMOTE_PATH/"

# idf.py set-target always runs a fullclean first (wiping sdkconfig and the
# whole build cache), even if the target isn't actually changing -- so only
# call it when the project isn't already configured for $TARGET. Otherwise
# every single deploy (including --flash-only) forces a full rebuild.
REMOTE_CMDS="set -euo pipefail; cd $REMOTE_PATH/$PROJECT_DIR; source $IDF_EXPORT"
REMOTE_CMDS="$REMOTE_CMDS; if [ -f sdkconfig ] && grep -qFx 'CONFIG_IDF_TARGET=\"$TARGET\"' sdkconfig; then echo '(already targeting $TARGET, skipping set-target/fullclean)'; else idf.py set-target $TARGET; fi"
if [ "$DO_MENUCONFIG" -eq 1 ]; then
    REMOTE_CMDS="$REMOTE_CMDS && idf.py menuconfig"
fi
if [ "$FLASH_ONLY" -eq 0 ]; then
    # `idf.py build` is *supposed* to re-run config generation on its own
    # whenever a Kconfig file changes (picking up new/changed CONFIG_*
    # options into sdkconfig/sdkconfig.h), but that detection didn't fire
    # reliably here in practice (likely rsync-preserved mtimes vs. the
    # Pi's clock) -- it silently kept building against a stale sdkconfig.h
    # missing a newly-added option, a confusing "undeclared identifier"
    # compile error far from the actual cause. `idf.py reconfigure` forces
    # that regeneration explicitly, every build; unlike `set-target` it
    # does NOT fullclean or wipe existing sdkconfig settings, so it's
    # cheap and safe to always run.
    REMOTE_CMDS="$REMOTE_CMDS && idf.py reconfigure && idf.py build"
fi
if [ "$OTA_MODE" -eq 1 ]; then
    # `ls build/*.bin` rather than hardcoding the project's binary name, so
    # this doesn't need updating if that ever changes.
    REMOTE_CMDS="$REMOTE_CMDS && BIN=\$(ls build/*.bin | head -1) && echo \"==> Pushing \$BIN to http://$OTA_HOST:$OTA_PORT/ota (device reboots into it on success)\" && curl -sS -X POST --data-binary @\"\$BIN\" \"http://$OTA_HOST:$OTA_PORT/ota\" && echo"
else
    REMOTE_CMDS="$REMOTE_CMDS && idf.py -p $PORT flash"
fi
if [ "$DO_MONITOR" -eq 1 ]; then
    REMOTE_CMDS="$REMOTE_CMDS && idf.py -p $PORT monitor"
fi

if [ "$OTA_MODE" -eq 1 ]; then
    if [ "$FLASH_ONLY" -eq 1 ]; then
        echo "==> Re-pushing $PROJECT_DIR (target: $TARGET) to $OTA_HOST over OTA (skipping build)"
    else
        echo "==> Building and pushing $PROJECT_DIR (target: $TARGET) to $OTA_HOST over OTA"
    fi
elif [ "$FLASH_ONLY" -eq 1 ]; then
    echo "==> Flashing $PROJECT_DIR (target: $TARGET) on $REMOTE_HOST (skipping build)"
else
    echo "==> Building and flashing $PROJECT_DIR (target: $TARGET) on $REMOTE_HOST"
fi
if [ "$DO_MENUCONFIG" -eq 1 ] || [ "$DO_MONITOR" -eq 1 ]; then
    # menuconfig and monitor are interactive; need a real tty
    ssh -t "$REMOTE_HOST" "$REMOTE_CMDS"
else
    ssh "$REMOTE_HOST" "$REMOTE_CMDS"
fi
