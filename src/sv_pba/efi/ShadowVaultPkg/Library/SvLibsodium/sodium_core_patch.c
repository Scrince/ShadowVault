

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