#!/bin/bash
set -e

UE_CMD="/opt/UnrealEngine/Engine/Binaries/Linux/UnrealEditor-Cmd"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Find .uproject (assumes Plugins/DataBridge/Examples/Scripts/)
PROJECT=$(find "$SCRIPT_DIR/../../../.." -maxdepth 1 -name "*.uproject" | head -1)

if [ -z "$PROJECT" ]; then
    echo "ERROR: Could not find .uproject file"
    exit 1
fi

SOURCE="${1:-All}"
ENVIRONMENT="${2:-}"
DRY_RUN="${3:-}"

ARGS=("$PROJECT" "-run=DataBridgeUpdate" "-unattended" "-nopause")

if [ "$SOURCE" = "All" ]; then
    ARGS+=("-All")
else
    ARGS+=("-SourceName=$SOURCE")
fi

if [ -n "$ENVIRONMENT" ]; then
    ARGS+=("-Environment=$ENVIRONMENT")
fi

if [ "$DRY_RUN" = "--dry-run" ]; then
    ARGS+=("-DryRun")
fi

echo "Running DataBridge update: $SOURCE"
"$UE_CMD" "${ARGS[@]}"
