#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

PATCHED_GSPLUS_BIN="/Users/kevincarr/projects/AppleIIGS/GSplus-vintanetgs-fix/build/GSplus.app/Contents/MacOS/GSplus"
DEFAULT_GSPLUS_BIN="/Volumes/MEDIA/Applications/GSplus.app/Contents/MacOS/GSplus"
if [ "${GSPLUS_BIN:-}" ]; then
    GSPLUS_BIN="$GSPLUS_BIN"
elif [ -x "$PATCHED_GSPLUS_BIN" ]; then
    GSPLUS_BIN="$PATCHED_GSPLUS_BIN"
else
    GSPLUS_BIN="$DEFAULT_GSPLUS_BIN"
fi
GSPLUS_CONFIG="${GSPLUS_CONFIG:-$PROJECT_DIR/gsplus-vintanetgs.kegs}"
PRINTER_TCP_HOST="${PRINTER_TCP_HOST:-127.0.0.1}"
PRINTER_TCP_PORT="${PRINTER_TCP_PORT:-6501}"

gsplus_app_bundle() {
    case "$GSPLUS_BIN" in
        *.app/Contents/MacOS/*)
            printf '%s\n' "${GSPLUS_BIN%/Contents/MacOS/*}"
            ;;
        *)
            printf '\n'
            ;;
    esac
}

if [ ! -f "$GSPLUS_CONFIG" ]; then
    echo "GSPlus config not found: $GSPLUS_CONFIG" >&2
    exit 1
fi

if [ ! -x "$GSPLUS_BIN" ]; then
    echo "GSPlus executable not found or not executable: $GSPLUS_BIN" >&2
    exit 1
fi

echo "Launching GSPlus for VintaNetGS."
echo "Config: $GSPLUS_CONFIG"
echo "Printer slot-1 serial listener: ${PRINTER_TCP_HOST}:${PRINTER_TCP_PORT}"
echo
echo "After GSPlus starts, verify:"
echo "  lsof -nP -iTCP:${PRINTER_TCP_PORT} -sTCP:LISTEN"
echo
echo "Then start the live peer driver:"
echo "  cd /Users/kevincarr/projects/VintaNetTestDriver"
echo "  python3 vntest.py serial-peer --serial-port ${PRINTER_TCP_PORT}"
echo

APP_BUNDLE=$(gsplus_app_bundle)
if [ -n "$APP_BUNDLE" ] && [ -d "$APP_BUNDLE" ]; then
    exec /usr/bin/open -n "$APP_BUNDLE" --args -cfg "$GSPLUS_CONFIG"
fi

exec "$GSPLUS_BIN" -cfg "$GSPLUS_CONFIG"
