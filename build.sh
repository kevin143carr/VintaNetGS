#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORKFLOW="$SCRIPT_DIR/../../../Devel_Ops/AppleIIGS/appleiigs-workflow.sh"
ENV_FILE="$SCRIPT_DIR/appleiigs-project.env"
STAGE="${1:-build}"

cd "$SCRIPT_DIR"

# shellcheck disable=SC1090
. "$ENV_FILE"

run() {
    printf '+'
    for arg in "$@"; do
        printf ' %s' "$arg"
    done
    printf '\n'
    "$@"
}

workflow_stage() {
    run "$WORKFLOW" --env "$ENV_FILE" "$1"
}

require_gsplus_config() {
    [ -n "${GSPLUS_BIN-}" ] || {
        echo "GSPLUS_BIN is not set" >&2
        exit 1
    }
    [ -n "${GSPLUS_CONFIG-}" ] || {
        echo "GSPLUS_CONFIG is not set" >&2
        exit 1
    }
    [ -x "$GSPLUS_BIN" ] || {
        echo "GSPlus executable not found or not executable: $GSPLUS_BIN" >&2
        exit 1
    }
    [ -f "$GSPLUS_CONFIG" ] || {
        echo "GSPlus config not found: $GSPLUS_CONFIG" >&2
        exit 1
    }
}

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

launch_gsplus() {
    app_bundle=$(gsplus_app_bundle)
    if [ -n "$app_bundle" ] && [ -d "$app_bundle" ]; then
        run /usr/bin/open -n "$app_bundle" --args -cfg "$GSPLUS_CONFIG"
    else
        run "$GSPLUS_BIN" -cfg "$GSPLUS_CONFIG" &
    fi
}

stage_emulator() {
    require_gsplus_config

    case "$RUN_SECONDS" in
        -*)
            launch_gsplus
            echo "GSPlus is starting with config: $GSPLUS_CONFIG"
            echo "Printer slot-1 serial capture: TCP ${PRINTER_TCP_HOST:-127.0.0.1}:${PRINTER_TCP_PORT:-6501}"
            ;;
        *)
            run "$GSPLUS_BIN" -cfg "$GSPLUS_CONFIG" &
            gsplus_pid=$!
            echo "GSPlus is running for $RUN_SECONDS seconds with config: $GSPLUS_CONFIG"
            sleep "$RUN_SECONDS"
            kill "$gsplus_pid" 2>/dev/null || true
            ;;
    esac
}

case "$STAGE" in
    emulator)
        stage_emulator
        ;;
    run-gsos)
        workflow_stage build
        workflow_stage import
        stage_emulator
        ;;
    run-both)
        workflow_stage build
        workflow_stage test-iix
        workflow_stage import
        stage_emulator
        ;;
    all)
        workflow_stage build
        workflow_stage test-iix
        workflow_stage import
        stage_emulator
        ;;
    *)
        exec "$WORKFLOW" --env "$ENV_FILE" "$STAGE"
        ;;
esac
