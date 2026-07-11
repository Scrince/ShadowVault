/** @file
  Minimal utils subset for UEFI libsodium (avoids desktop utils.c).

  Copyright (c) 2026 ShadowVault contributors. SPDX-License-Identifier: MIT
**/

#include <stddef.h>
#include <stdint.h>

#include "core.h"

void
sodium_memzero (
  void        *pnt,
  const size_t  len
  )
{
  volatile unsigned char  *p;
  size_t                  i;

  if ((pnt == NULL) || (len == 0)) {
    return;
  }

  p = (volatile unsigned char *)pnt;
  for (i = 0; i < len; ++i) {
    p[i] = 0U;
  }
}

int
sodium_memcmp (
  const void  *b1_,
  const void  *b2_,
  size_t      len
  )
{
  const volatile unsigned char  *b1;
  const volatile unsigned char  *b2;
  volatile uint_fast16_t        d;
  size_t                        i;

  if ((b1_ == NULL) || (b2_ == NULL)) {
    return -1;
  }

  b1 = (const volatile unsigned char *)b1_;
  b2 = (const volatile unsigned char *)b2_;
  d  = 0U;

  for (i = 0; i < len; ++i) {
    d |= (uint_fast16_t)(b1[i] ^ b2[i]);
  }

  return (int)(((d - 1U) >> 8) & 1U) - 1;
}

void
_sodium_alloc_init (
  void
  )
{
}