#!/bin/bash
set -euo pipefail

# =============================================================================
# Generate prebuilt AppIcon.icns in resources/ from resources/icon.png
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

SOURCE_ICON="${PROJECT_ROOT}/resources/icon.png"
OUTPUT_ICNS="${PROJECT_ROOT}/resources/AppIcon.icns"
ICONSET_DIR="/tmp/Diana.iconset"

log() {
    echo "[$(date '+%H:%M:%S')] $*"
}

if [[ ! -f "$SOURCE_ICON" ]]; then
    echo "[ERROR] Source icon not found at ${SOURCE_ICON}" >&2
    exit 1
fi

log "Generating AppIcon.icns from ${SOURCE_ICON}"

rm -rf "$ICONSET_DIR"
mkdir -p "$ICONSET_DIR"
rm -f "$OUTPUT_ICNS"

# macOS .icns requires specific sizes: 16, 32, 128, 256, 512 (and @2x variants)
sizes=(16 32 128 256 512)
for size in "${sizes[@]}"; do
    sips -z "$size" "$size" "$SOURCE_ICON" --out "${ICONSET_DIR}/icon_${size}x${size}.png" >/dev/null
    size_2x=$((size * 2))
    if [[ $size_2x -le 1024 ]]; then
        sips -z "$size_2x" "$size_2x" "$SOURCE_ICON" --out "${ICONSET_DIR}/icon_${size}x${size}@2x.png" >/dev/null
    fi
done

iconutil -c icns "$ICONSET_DIR" -o "$OUTPUT_ICNS"
rm -rf "$ICONSET_DIR"

if [[ -f "$OUTPUT_ICNS" ]]; then
    log "Generated: $OUTPUT_ICNS"
else
    echo "[ERROR] Failed to generate AppIcon.icns" >&2
    exit 1
fi
