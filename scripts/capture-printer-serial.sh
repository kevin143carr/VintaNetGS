#!/bin/sh
set -eu

HOST="${PRINTER_TCP_HOST:-127.0.0.1}"
PORT="${PRINTER_TCP_PORT:-6501}"
OUTPUT="${VINTANETGS_PRINTER_CAPTURE:-/tmp/vintanetgs-printer.bin}"

if ! lsof -nP -iTCP:"$PORT" -sTCP:LISTEN >/dev/null 2>&1; then
    echo "GSplus is not listening on TCP port $PORT." >&2
    echo "Launch GSplus with g_serial_cfg[0] = 3, then verify:" >&2
    echo "  lsof -nP -iTCP:$PORT -sTCP:LISTEN" >&2
    exit 1
fi

: > "$OUTPUT"
done_report=0

report_capture() {
    [ "$done_report" -eq 0 ] || return 0
    done_report=1
    echo
    echo "capture-file=$OUTPUT"
    wc -c "$OUTPUT"
    if [ -s "$OUTPUT" ]; then
        xxd -g 1 -u "$OUTPUT"
    else
        echo "hex="
    fi
}

stop_capture() {
    trap - INT TERM
    if [ -n "${nc_pid-}" ]; then
        kill "$nc_pid" 2>/dev/null || true
        wait "$nc_pid" 2>/dev/null || true
    fi
    report_capture
    exit 130
}

trap stop_capture INT TERM
trap report_capture EXIT

echo "Capturing slot-1 printer TCP from $HOST:$PORT"
echo "Raw output file: $OUTPUT"
echo "Stop with Ctrl-C after the requested VintaNetGS diagnostic completes."

nc -d "$HOST" "$PORT" > "$OUTPUT" &
nc_pid=$!
sleep 0.2
lsof -nP -iTCP:"$PORT" || true
wait "$nc_pid"
