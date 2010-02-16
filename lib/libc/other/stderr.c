#include <lib.h>
#include <sys/types.h>
#include <unistd.h>

_PROTOTYPE( void std_err, (char *s));

void std_err(s)
char *s;
{
  register char *p = s;

  while (*p != 0) p++;
  write(2, s, (int) (p - s));
}
