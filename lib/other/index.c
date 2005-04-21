#include <lib.h>
/* index - find first occurrence of a character in a string */

#include <string.h>

char *index(s, charwanted)	/* found char, or NULL if none */
_CONST char *s;
char charwanted;
{
  return(strchr(s, charwanted));
}
