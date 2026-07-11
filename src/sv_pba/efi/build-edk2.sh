#!/usr/bin/env bash
# Build ShadowVaultBoot.efi with EDK2 (GCC5 / X64).
set -euo pipefail

EDK2_ROOT="${1:-${EDK2_ROOT:-}}"
BUILD_TARGET="${BUILD_TARGET:-RELEASE}"
TOOLCHAIN="${TOOLCHAIN:-GCC5}"
ARCH="${ARCH:-X64}"

if [[ -z "${EDK2_ROOT}" ]]; then
  echo "usage: EDK2_ROOT=/path/to/edk2 $0" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_EFI="${SCRIPT_DIR}"
export PACKAGES_PATH="${PACKAGES_PATH:-${REPO_EFI}}"
export WORKSPACE="${EDK2_ROOT}"

if [[ ! -f "${WORKSPACE}/edksetup.sh" ]]; then
  echo "EDK2 root not found: ${WORKSPACE}" >&2
  exit 1
fi

cd "${WORKSPACE}"
source edksetup.sh
make -C BaseTools

build -p ShadowVaultPkg/ShadowVaultPkg.dsc \
  -m ShadowVaultPkg/Application/ShadowVaultBoot/ShadowVaultBoot.inf \
  -a "${ARCH}" \
  -t "${TOOLCHAIN}" \
  -b "${BUILD_TARGET}"

EFI_OUT="${WORKSPACE}/Build/ShadowVaultPkg/${BUILD_TARGET}_${TOOLCHAIN}/${ARCH}/ShadowVaultBoot.efi"
if [[ ! -f "${EFI_OUT}" ]]; then
  echo "Expected output not found: ${EFI_OUT}" >&2
  exit 1
fi

echo "Built: ${EFI_OUT}"