#include <sys/cdefs.h>

/* LSC  Import as-is the actual file (and implementation), and also
 * define a weak alias, so that everyone is happy about it.*/
#include "snprintf.c"

#if defined(__weak_alias)
__weak_alias(_snprintf, snprintf)
#endif
