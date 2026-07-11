/** @file
  UEFI platform hooks for vendored libsodium (malloc + RNG).

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: MIT
**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

void *
malloc (
  size_t  size
  )
{
  if (size == 0) {
    return NULL;
  }

  return AllocatePool (size);
}

void
free (
  void  *ptr
  )
{
  if (ptr != NULL) {
    FreePool (ptr);
  }
}

void *
calloc (
  size_t  count,
  size_t  size
  )
{
  size_t  total;
  void    *ptr;

  if ((count != 0) && (size > (SIZE_MAX / count))) {
    return NULL;
  }

  total = count * size;
  ptr   = malloc (total);
  if (ptr != NULL) {
    ZeroMem (ptr, total);
  }

  return ptr;
}

