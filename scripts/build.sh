#!/usr/bin/env bash

set -Eeuo pipefail

# ============================================================
# Ethos ISO Build Script
# Builds a bootable BIOS + UEFI ISO from existing components.
# Does NOT download, extract, or compile Limine.
# ============================================================

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

KERNEL="$ROOT_DIR/bin/ethos"
COMMAND_DIR="$ROOT_DIR/bin/commands"
LIMINE_DIR="$ROOT_DIR/boot/limine/limine-binary"
LIMINE_CONFIG="$ROOT_DIR/boot/limine/limine.conf"
ISO_ROOT="$ROOT_DIR/root"
BUILD_DIR="$ROOT_DIR/build"
ISO_IMAGE="$BUILD_DIR/ethos.iso"
WIFI_FIRMWARE="${ETHOS_IWLWIFI_FIRMWARE:-}"

# ============================================================
# Logging
# ============================================================

info() {
  printf '\033[1;34m[INFO]\033[0m %s\n' "$*"
}

error() {
  printf '\033[1;31m[ERROR]\033[0m %s\n' "$*" >&2
}

die() {
  error "$*"
  exit 1
}

# ============================================================
# Validate required tools
# ============================================================

for cmd in xorriso; do
  command -v "$cmd" >/dev/null 2>&1 ||
    die "Required command not found: $cmd"
done

if command -v bear >/dev/null 2>&1; then
  bear -- make
else
  make
fi

# ============================================================
# Validate existing build artifacts
# ============================================================

[[ -f "$KERNEL" ]] ||
  die "Kernel not found: bin/ethos. Build the kernel first."

[[ -d "$COMMAND_DIR" ]] ||
  die "Command directory not found: bin/commands. Build the project first."

shopt -s nullglob
COMMAND_BINARIES=("$COMMAND_DIR"/*.apx)

[[ -f "$LIMINE_CONFIG" ]] ||
  die "Limine configuration not found: boot/limine/limine.conf"

[[ -x "$LIMINE_DIR/limine" ]] ||
  die "Limine utility not found or not executable: $LIMINE_DIR/limine"

for file in \
  limine-bios.sys \
  limine-bios-cd.bin \
  limine-uefi-cd.bin \
  BOOTX64.EFI; do
  [[ -f "$LIMINE_DIR/$file" ]] ||
    die "Required Limine file not found: $file"
done

# ============================================================
# Prepare directories
# ============================================================

info "Preparing build directories..."

mkdir -p "$BUILD_DIR" "$ISO_ROOT"

# Remove previous staging files without touching Limine binaries.
rm -rf "$ISO_ROOT/boot" "$ISO_ROOT/EFI"

mkdir -p \
  "$ISO_ROOT/boot/limine" \
  "$ISO_ROOT/boot/firmware" \
  "$ISO_ROOT/EFI/BOOT" \
  "$ISO_ROOT/bin" \
  "$ISO_ROOT/sbin" \
  "$ISO_ROOT/lib/kernel" \
  "$ISO_ROOT/usr/bin" \
  "$ISO_ROOT/usr/lib" \
  "$ISO_ROOT/home/ethos/Desktop" \
  "$ISO_ROOT/home/ethos/Documents" \
  "$ISO_ROOT/home/ethos/Downloads" \
  "$ISO_ROOT/home/ethos/Media/Music" \
  "$ISO_ROOT/home/ethos/Media/Pictures" \
  "$ISO_ROOT/home/ethos/Media/Videos" \
  "$ISO_ROOT/home/ethos/Projects" \
  "$ISO_ROOT/home/ethos/Public" \
  "$ISO_ROOT/home/ethos/Templates" \
  "$ISO_ROOT/home/ethos/.cache" \
  "$ISO_ROOT/home/ethos/.config" \
  "$ISO_ROOT/home/ethos/.local/bin" \
  "$ISO_ROOT/home/ethos/.local/lib" \
  "$ISO_ROOT/home/ethos/.local/share" \
  "$ISO_ROOT/home/ethos/Temporary"

# ============================================================
# Stage Ethos kernel
# ============================================================

info "Copying Ethos kernel..."

install -m 0644 \
  "$KERNEL" \
  "$ISO_ROOT/boot/ethos"

if [[ ${#COMMAND_BINARIES[@]} -gt 0 ]]; then
  install -m 0644 "${COMMAND_BINARIES[@]}" "$ISO_ROOT/usr/bin/"
fi
rm -f "$ISO_ROOT/usr/bin/"*.elf

# ============================================================
# Stage Limine configuration and boot files
# ============================================================

info "Copying boot configuration and Limine files..."

install -m 0644 \
  "$LIMINE_CONFIG" \
  "$ISO_ROOT/boot/limine/limine.conf"

if [[ -n "$WIFI_FIRMWARE" ]]; then
  [[ -f "$WIFI_FIRMWARE" ]] ||
    die "AX201 firmware file not found: $WIFI_FIRMWARE"
  WIFI_FIRMWARE_SOURCE="$(readlink -f -- "$WIFI_FIRMWARE")"
  if [[ "$WIFI_FIRMWARE_SOURCE" == *.zst ]]; then
    command -v zstd >/dev/null 2>&1 ||
      die "zstd is required to stage compressed AX201 firmware."
    zstd -q -d -c "$WIFI_FIRMWARE_SOURCE" > \
      "$ISO_ROOT/boot/firmware/iwlwifi-QuZ-a0-jf-b0-77.ucode"
  else
    install -m 0644 "$WIFI_FIRMWARE_SOURCE" \
      "$ISO_ROOT/boot/firmware/iwlwifi-QuZ-a0-jf-b0-77.ucode"
  fi
  printf '    module_path: boot():/boot/firmware/iwlwifi-QuZ-a0-jf-b0-77.ucode\n    module_string: iwlwifi-QuZ-a0-jf-b0-77.ucode\n' \
    >> "$ISO_ROOT/boot/limine/limine.conf"
  info "Staged AX201 firmware module."
fi

for command_binary in "${COMMAND_BINARIES[@]}"; do
  command_name="$(basename -- "$command_binary" .apx)"
  [[ "$command_name" =~ ^[A-Za-z0-9._-]+$ ]] ||
    die "Invalid command binary name: $command_name"
  printf '    module_path: boot():/usr/bin/%s.apx\n    module_string: %s\n' \
    "$command_name" "$command_name" \
    >> "$ISO_ROOT/boot/limine/limine.conf"
done

install -m 0644 \
  "$LIMINE_DIR/limine-bios.sys" \
  "$LIMINE_DIR/limine-bios-cd.bin" \
  "$LIMINE_DIR/limine-uefi-cd.bin" \
  "$ISO_ROOT/boot/limine/"

install -m 0644 \
  "$LIMINE_DIR/BOOTX64.EFI" \
  "$ISO_ROOT/EFI/BOOT/BOOTX64.EFI"

# Optional 32-bit EFI bootloader.
if [[ -f "$LIMINE_DIR/BOOTIA32.EFI" ]]; then
  install -m 0644 \
    "$LIMINE_DIR/BOOTIA32.EFI" \
    "$ISO_ROOT/EFI/BOOT/BOOTIA32.EFI"
fi

# ============================================================
# Create bootable ISO
# ============================================================

info "Creating BIOS + UEFI ISO..."

rm -f "$ISO_IMAGE"

xorriso -as mkisofs \
  -R -r -J \
  -b boot/limine/limine-bios-cd.bin \
  -no-emul-boot \
  -boot-load-size 4 \
  -boot-info-table \
  -hfsplus \
  -apm-block-size 2048 \
  --efi-boot boot/limine/limine-uefi-cd.bin \
  -efi-boot-part \
  --efi-boot-image \
  --protective-msdos-label \
  "$ISO_ROOT" \
  -o "$ISO_IMAGE"

# ============================================================
# Install BIOS boot stages into the ISO
# This modifies the generated ISO, not the host bootloader.
# ============================================================

info "Installing Limine BIOS boot stages into ISO..."

"$LIMINE_DIR/limine" bios-install "$ISO_IMAGE"

# ============================================================
# Verify output
# ============================================================

[[ -s "$ISO_IMAGE" ]] ||
  die "ISO creation failed: output is missing or empty."

info "Ethos ISO build completed successfully."

printf '\n'
printf 'Kernel : %s\n' "$KERNEL"
printf 'ISO    : %s\n' "$ISO_IMAGE"
printf 'Size   : '
du -h "$ISO_IMAGE" | cut -f1
