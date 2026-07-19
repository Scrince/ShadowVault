

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <sodium.h>

void *
sodium_malloc (size_t size) {
  return AllocatePool (size);
}

void
sodium_free (void *ptr) {
  if (ptr != NULL) {
    FreePool (ptr);
  }
}

void
sodium_mlock (void *addr, size_t len) {
  (void)addr;
  (void)len;
}

void
sodium_munlock (void *addr, size_t len) {
  (void)addr;
  (void)len;
}