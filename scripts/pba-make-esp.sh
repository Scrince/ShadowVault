#!/usr/bin/env bash

set -euo pipefail

EFI_PATH="${1:-}"
OUTPUT_IMAGE="${OUTPUT_IMAGE:-/tmp/shadowvault-pba-esp.img}"
SIZE_MIB="${SIZE_MIB:-64}"

if [[ -z "${EFI_PATH}" || ! -f "${EFI_PATH}" ]]; then
  echo "usage: $0 /path/to/ShadowVaultBoot.efi" >&2
  exit 1
fi

command -v mformat >/dev/null || { echo "mtools required (mformat, mcopy)" >&2; exit 1; }

rm -f "${OUTPUT_IMAGE}"
dd if=/dev/zero of="${OUTPUT_IMAGE}" bs=1M count="${SIZE_MIB}" status=none
mformat -i "${OUTPUT_IMAGE}" -F -v SV_PBA_ESP ::
mmd -i "${OUTPUT_IMAGE}" ::/EFI ::/EFI/ShadowVault
mcopy -i "${OUTPUT_IMAGE}" "${EFI_PATH}" ::/EFI/ShadowVault/ShadowVaultBoot.efi

echo "ESP image: ${OUTPUT_IMAGE}"
echo "Layout: EFI/ShadowVault/ShadowVaultBoot.efi"