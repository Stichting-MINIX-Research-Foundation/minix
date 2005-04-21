#include <lib.h>
/* bzero - Berklix subset of memset  */

#include <string.h>

void bzero(dst, length)
void *dst;
size_t length;
{
  (void) memset(dst, 0, length);
}
