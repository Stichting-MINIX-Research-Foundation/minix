#include <lib.h>
#include <sys/types.h>
#include <unistd.h>
#include <minix/minlib.h>

void std_err(const char *s)
{
  register const char *p = s;

  while (*p != 0) p++;
  write(2, s, (int) (p - s));
}
