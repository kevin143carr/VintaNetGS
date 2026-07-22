#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORKFLOW="$SCRIPT_DIR/../../../Devel_Ops/AppleIIGS/appleiigs-workflow.sh"
ENV_FILE="$SCRIPT_DIR/appleiigs-project.env"
STAGE="${1:-build}"

cd "$SCRIPT_DIR"
exec "$WORKFLOW" --env "$ENV_FILE" "$STAGE"
