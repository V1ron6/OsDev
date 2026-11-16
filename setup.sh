#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WRAPPER_DIR="/usr/local/bin"
PACKAGES=(
    build-essential
    gcc-multilib
    binutils
    nasm
    qemu-system-x86
)

log() {
    printf '[setup] %s\n' "$*"
}

fail() {
    printf '[setup] error: %s\n' "$*" >&2
    exit 1
}

if [[ ! -f /etc/os-release ]]; then
    fail "cannot identify the operating system; install the prerequisites manually"
fi

# shellcheck disable=SC1091
. /etc/os-release
case "${ID:-}" in
    ubuntu|debian)
        ;;
    *)
        fail "unsupported distribution '${ID:-unknown}'; this script supports Debian and Ubuntu"
        ;;
esac

if [[ $EUID -eq 0 ]]; then
    SUDO=()
else
    command -v sudo >/dev/null 2>&1 ||
        fail "sudo is required when setup.sh is not run as root"
    SUDO=(sudo)
fi

missing_packages=()
for package in "${PACKAGES[@]}"; do
    if ! dpkg-query -W -f='${db:Status-Abbrev}' "$package" 2>/dev/null |
        grep -q '^ii '; then
        missing_packages+=("$package")
    fi
done

if [[ ${#missing_packages[@]} -gt 0 ]]; then
    log "installing missing packages: ${missing_packages[*]}"
    "${SUDO[@]}" apt-get update
    "${SUDO[@]}" apt-get install -y "${missing_packages[@]}"
else
    log "all Debian/Ubuntu packages are already installed"
fi

install_wrapper() {
    local name="$1"
    local body="$2"
    local path="$WRAPPER_DIR/$name"

    if [[ -e "$path" && ! -f "$path" ]]; then
        fail "$path exists but is not a regular file"
    fi

    if [[ -f "$path" ]]; then
        return
    fi

    log "installing $path"
    printf '%s\n' "$body" | "${SUDO[@]}" tee "$path" >/dev/null
    "${SUDO[@]}" chmod 755 "$path"
}

install_wrapper i686-elf-gcc \
    '#!/usr/bin/env bash
exec gcc -m32 "$@"'
install_wrapper i686-elf-ld \
    '#!/usr/bin/env bash
exec ld -m elf_i386 "$@"'
install_wrapper i686-elf-objcopy \
    '#!/usr/bin/env bash
exec objcopy "$@"'
install_wrapper i686-elf-objdump \
    '#!/usr/bin/env bash
exec objdump "$@"'

required_tools=(make gcc ld objcopy objdump nasm qemu-system-i386
    i686-elf-gcc i686-elf-ld i686-elf-objcopy i686-elf-objdump)
for tool in "${required_tools[@]}"; do
    command -v "$tool" >/dev/null 2>&1 ||
        fail "required command '$tool' is still unavailable"
done

log "toolchain is ready"
log "build with: $ROOT_DIR/builder.sh"
log "run with:   $ROOT_DIR/run.sh"
