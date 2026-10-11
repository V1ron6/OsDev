#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="$ROOT_DIR/output"
ISO_IMAGE="$OUTPUT_DIR/bytebandit.iso"
PERSISTENT_DISK="$OUTPUT_DIR/bytebandit-data.img"
build_required=1

select_iso_image() {
    if [[ -f "$ISO_IMAGE" ]]; then
        return
    fi
    shopt -s nullglob
    local versioned_images=("$OUTPUT_DIR"/bytebandit-v*.iso)
    shopt -u nullglob
    if [[ ${#versioned_images[@]} -gt 0 ]]; then
        ISO_IMAGE="${versioned_images[-1]}"
    fi
}

case "${1:-}" in
    --help|-h)
        cat <<'USAGE'
Usage: ./run.sh [options] [qemu arguments]

Options:
  --reset-disk  Remove the persistent ByteFS image before booting
  --no-build    Boot the existing ISO and disk without rebuilding
  --help        Show this help

The persistent disk is output/bytebandit-data.img and survives normal builds.
USAGE
        exit 0
        ;;
    --reset-disk)
        rm -f "$PERSISTENT_DISK"
        shift
        ;;
    --no-build)
        build_required=0
        shift
        ;;
esac

if [[ $build_required -eq 1 ]]; then
    make -C "$ROOT_DIR" all
fi

select_iso_image

if [[ ! -f "$ISO_IMAGE" || ! -f "$PERSISTENT_DISK" ]]; then
    printf 'run.sh: required image is missing\n' >&2
    exit 1
fi

exec qemu-system-i386 \
    -boot order=d \
    -display "${QEMU_DISPLAY:-none}" \
    -serial "${QEMU_SERIAL:-stdio}" \
    -m "${QEMU_MEMORY:-64}" \
    -cdrom "$ISO_IMAGE" \
    -drive "format=raw,file=$PERSISTENT_DISK" \
    "$@"
