#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$ROOT_DIR/output"

mkdir -p "$OUTPUT_DIR"

if [[ $# -gt 0 ]]; then
    make -C "$ROOT_DIR" "$@"
    exit 0
fi

version=1
while [[ -e "$OUTPUT_DIR/bytebandit-v$(printf '%03d' "$version").iso" ]]; do
    version=$((version + 1))
done

if [[ -f "$OUTPUT_DIR/bytebandit.iso" ]]; then
    mv "$OUTPUT_DIR/bytebandit.iso" \
       "$OUTPUT_DIR/bytebandit-v$(printf '%03d' "$version").iso"
    version=$((version + 1))
fi

image_name="bytebandit-v$(printf '%03d' "$version").iso"
make -C "$ROOT_DIR" ISO_NAME="$image_name" all

if [[ -f "$OUTPUT_DIR/$image_name" ]]; then
    printf 'ISO ready: %s\n' "$OUTPUT_DIR/$image_name"
fi