#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <string.h>

void _loadname(const char *name, message *msgptr)
{
/* This function is used to load a string into a type m3 message. If the
 * string fits in the message, it is copied there.  If not, a pointer to
 * it is passed.
 */

  register size_t k;

  k = strlen(name) + 1;
  msgptr->m3_i1 = k;
  msgptr->m3_p1 = (char *) __UNCONST(name);
  if (k <= M3_STRING) strcpy(msgptr->m3_ca1, name);
}
