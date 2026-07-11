#!/usr/bin/env bash
# Boot OVMF + ShadowVault PBA ESP in QEMU (Linux/macOS).
set -euo pipefail

ESP_IMAGE="${ESP_IMAGE:-/tmp/shadowvault-pba-esp.img}"
OVMF_CODE="${OVMF_CODE:-/usr/share/OVMF/OVMF_CODE.fd}"
OVMF_VARS="${OVMF_VARS:-/tmp/shadowvault-pba-OVMF_VARS.fd}"
QEMU="${QEMU:-qemu-system-x86_64}"
MEMORY_MIB="${MEMORY_MIB:-512}"

if [[ ! -f "${ESP_IMAGE}" ]]; then
  echo "ESP image not found: ${ESP_IMAGE}" >&2
  echo "Run scripts/pba-make-esp.sh first." >&2
  exit 1
fi

if [[ ! -f "${OVMF_CODE}" ]]; then
  echo "OVMF code FD not found: ${OVMF_CODE}" >&2
  exit 1
fi

if [[ ! -f "${OVMF_VARS}" ]]; then
  cp "${OVMF_CODE}" "${OVMF_VARS}"
fi

echo "Passphrase scaffold accepts: shadowvault-lab"
exec "${QEMU}" \
  -machine q35 \
  -m "${MEMORY_MIB}" \
  -cpu max \
  -drive if=pflash,format=raw,readonly=on,file="${OVMF_CODE}" \
  -drive if=pflash,format=raw,file="${OVMF_VARS}" \
  -drive file="${ESP_IMAGE}",format=raw,if=virtio \
  -serial stdio \
  -display none