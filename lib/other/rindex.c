#include <lib.h>
/* rindex - find last occurrence of a character in a string  */

#include <string.h>

char *rindex(s, charwanted)	/* found char, or NULL if none */
_CONST char *s;
char charwanted;
{
  return(strrchr(s, charwanted));
}
