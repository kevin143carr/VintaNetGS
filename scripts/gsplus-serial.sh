#!/bin/sh
set -eu

COMMAND="${1:-status}"

PRINTER_TCP_HOST="${PRINTER_TCP_HOST:-127.0.0.1}"
PRINTER_TCP_PORT="${PRINTER_TCP_PORT:-6501}"

cleanup_pty_artifacts() {
    for pid_file in \
        /tmp/vintanetgs-gsplus-serial.pid \
        /tmp/vintanetgs-gsplus-printer-serial.pid \
        /tmp/vintanetgs-gsplus-printer-serial.pid.child \
        /tmp/vintanetgs-gsplus-modem-serial.pid \
        /tmp/vintanetgs-gsplus-modem-serial.pid.child
    do
        if [ -f "$pid_file" ]; then
            pid=$(cat "$pid_file" 2>/dev/null || true)
            if [ -n "$pid" ]; then
                kill "$pid" 2>/dev/null || true
            fi
        fi
    done

    rm -f \
        /tmp/vintanetgs-gsplus-serial \
        /tmp/vintanetgs-host-serial \
        /tmp/vintanetgs-gsplus-serial.pid \
        /tmp/vintanetgs-gsplus-printer-serial \
        /tmp/vintanetgs-host-printer-serial \
        /tmp/vintanetgs-gsplus-printer-serial.pid \
        /tmp/vintanetgs-gsplus-printer-serial.pid.child \
        /tmp/vintanetgs-gsplus-printer-serial.log \
        /tmp/vintanetgs-gsplus-modem-serial \
        /tmp/vintanetgs-host-modem-serial \
        /tmp/vintanetgs-gsplus-modem-serial.pid \
        /tmp/vintanetgs-gsplus-modem-serial.pid.child \
        /tmp/vintanetgs-gsplus-modem-serial.log
}

case "$COMMAND" in
    start|status)
        echo "GSplus serial bridge is disabled."
        echo "Slot-1 printer capture uses GSplus incoming TCP ${PRINTER_TCP_HOST}:${PRINTER_TCP_PORT}."
        ;;
    stop|cleanup)
        cleanup_pty_artifacts
        echo "Removed obsolete VintaNetGS PTY/socat serial artifacts."
        ;;
    restart)
        cleanup_pty_artifacts
        echo "GSplus serial bridge is disabled."
        echo "Slot-1 printer capture uses GSplus incoming TCP ${PRINTER_TCP_HOST}:${PRINTER_TCP_PORT}."
        ;;
    *)
        echo "Usage: $0 [start|status|stop|cleanup|restart]" >&2
        exit 2
        ;;
esac
