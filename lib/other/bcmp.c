#include <lib.h>
/* bcmp - Berklix equivalent of memcmp  */

#include <string.h>

int bcmp(s1, s2, length)	/* == 0 or != 0 for equality and inequality */ 
_CONST void *s1;
_CONST void *s2;
size_t length;
{
  return(memcmp(s1, s2, length));
}
