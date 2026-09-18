#!/usr/bin/env bash
# Sync src/ (and the rest of the repo, minus vendor/build/git cruft) to the
# Raspberry Pi ("watering-can") that has the ESP32 attached over USB, then
# build and flash it there.
#
# Run this from your dev machine, not the Pi.
#
# Usage:
#   scripts/deploy.sh                # rsync + build + flash
#   scripts/deploy.sh --monitor      # ...then attach the serial monitor
#   scripts/deploy.sh --menuconfig   # ...run menuconfig first (interactive)
#   scripts/deploy.sh --port /dev/ttyACM0
#
# Override any of these via environment variables if your setup differs:
#   REMOTE_HOST   (default: hunter@watering-can)
#   REMOTE_PATH   (default: ~/watering-can)
#   IDF_EXPORT    (default: ~/esp/esp-idf/export.sh -- wherever ESP-IDF v5.5
#                  actually lives on the Pi; edit this if that's wrong)
#   PORT          (default: /dev/ttyUSB0)

set -euo pipefail

REMOTE_HOST="${REMOTE_HOST:-hunter@watering-can}"
REMOTE_PATH="${REMOTE_PATH:-~/watering-can}"
IDF_EXPORT="${IDF_EXPORT:-~/esp/esp-idf/export.sh}"
PORT="${PORT:-/dev/ttyUSB0}"

DO_MONITOR=0
DO_MENUCONFIG=0

while [ $# -gt 0 ]; do
    case "$1" in
        --monitor) DO_MONITOR=1 ;;
        --menuconfig) DO_MENUCONFIG=1 ;;
        --port) PORT="$2"; shift ;;
        *) echo "Unknown argument: $1" >&2; exit 1 ;;
    esac
    shift
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "==> Syncing $REPO_ROOT to $REMOTE_HOST:$REMOTE_PATH"
rsync -avz --delete \
    --exclude='.git/' \
    --exclude='vendor/' \
    --exclude='build/' \
    --exclude='src/sdkconfig' \
    --exclude='src/sdkconfig.old' \
    --exclude='samples/relay-demo/build/' \
    "$REPO_ROOT/" "$REMOTE_HOST:$REMOTE_PATH/"

REMOTE_CMDS="set -euo pipefail; cd $REMOTE_PATH/src; source $IDF_EXPORT; idf.py set-target esp32"
if [ "$DO_MENUCONFIG" -eq 1 ]; then
    REMOTE_CMDS="$REMOTE_CMDS && idf.py menuconfig"
fi
REMOTE_CMDS="$REMOTE_CMDS && idf.py build && idf.py -p $PORT flash"
if [ "$DO_MONITOR" -eq 1 ]; then
    REMOTE_CMDS="$REMOTE_CMDS monitor"
fi

echo "==> Building and flashing on $REMOTE_HOST"
if [ "$DO_MENUCONFIG" -eq 1 ] || [ "$DO_MONITOR" -eq 1 ]; then
    # menuconfig and monitor are interactive; need a real tty
    ssh -t "$REMOTE_HOST" "$REMOTE_CMDS"
else
    ssh "$REMOTE_HOST" "$REMOTE_CMDS"
fi
