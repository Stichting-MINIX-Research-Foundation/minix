#include <lib.h>
/* bcopy - Berklix equivalent of memcpy  */

#include <string.h>

void bcopy(src, dst, length)
_CONST void *src;
void *dst;
size_t length;
{
  (void) memcpy(dst, src, length);
}
