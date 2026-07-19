

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <Library/BaseMemoryLib.h>

#include "core.h"
#include "crypto_stream_chacha20.h"
#include "private/common.h"
#include "randombytes.h"

static const char *
sv_randombytes_name (
  void
  )
{
  return "uefi-stub";
}

uint32_t
randombytes_random (
  void
  )
{
  return 0;
}

void
randombytes_stir (
  void
  )
{
}

uint32_t
randombytes_uniform (
  const uint32_t  upper_bound
  )
{
  uint32_t  min;
  uint32_t  r;

  if (upper_bound < 2) {
    return 0;
  }

  min = (1U + ~upper_bound) % upper_bound;
  do {
    r = randombytes_random ();
  } while (r < min);

  return r % upper_bound;
}

void
randombytes_buf (
  void        *buf,
  const size_t  size
  )
{
  if ((buf != NULL) && (size > 0)) {
    ZeroMem (buf, size);
  }
}

void
randombytes_buf_deterministic (
  void                    *buf,
  const size_t            size,
  const unsigned char     seed[randombytes_SEEDBYTES]
  )
{
  static const unsigned char  nonce[crypto_stream_chacha20_ietf_NONCEBYTES] = {
    'L', 'i', 'b', 's', 'o', 'd', 'i', 'u', 'm', 'D', 'R', 'G'
  };

  COMPILER_ASSERT (randombytes_SEEDBYTES == crypto_stream_chacha20_ietf_KEYBYTES);
  crypto_stream_chacha20_ietf ((unsigned char *)buf, (unsigned long long)size, nonce, seed);
}

size_t
randombytes_seedbytes (
  void
  )
{
  return randombytes_SEEDBYTES;
}

int
randombytes_close (
  void
  )
{
  return 0;
}

int
randombytes_set_implementation (
  const randombytes_implementation  *impl
  )
{
  (void)impl;
  return 0;
}

const char *
randombytes_implementation_name (
  void
  )
{
  return sv_randombytes_name ();
}

void
randombytes (
  unsigned char        *buf,
  const unsigned long long  buf_len
  )
{
  randombytes_buf (buf, (size_t)buf_len);
}