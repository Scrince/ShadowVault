

#include <Library/BaseMemoryLib.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

static int  sv_sodium_errno;

int *
_errno (
  void
  )
{
  return &sv_sodium_errno;
}

void *
memcpy (
  void        *dest,
  const void  *src,
  size_t      len
  )
{
  CopyMem (dest, src, len);
  return dest;
}

void *
memset (
  void   *dest,
  int    value,
  size_t  len
  )
{
  SetMem (dest, len, (UINT8)value);
  return dest;
}

size_t
strlen (
  const char  *str
  )
{
  size_t  len;

  if (str == NULL) {
    return 0;
  }

  for (len = 0; str[len] != '\0'; ++len) {
  }

  return len;
}

int
strncmp (
  const char  *a,
  const char  *b,
  size_t      len
  )
{
  size_t  i;

  for (i = 0; i < len; ++i) {
    if (a[i] != b[i]) {
      return (unsigned char)a[i] - (unsigned char)b[i];
    }
    if (a[i] == '\0') {
      return 0;
    }
  }

  return 0;
}

char *
strchr (
  const char  *str,
  int         ch
  )
{
  char  c;

  if (str == NULL) {
    return NULL;
  }

  c = (char)ch;
  while (*str != '\0') {
    if (*str == c) {
      return (char *)str;
    }
    ++str;
  }

  if (c == '\0') {
    return (char *)str;
  }

  return NULL;
}