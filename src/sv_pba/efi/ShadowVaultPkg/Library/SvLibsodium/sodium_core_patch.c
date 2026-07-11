/** @file
  Override libsodium core helpers that assume a desktop CRT.

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: MIT
**/

#include <Library/BaseLib.h>

void
abort (
  void
  )
{
  while (TRUE) {
    CpuDeadLoop ();
  }
}