#include "utils.h"
#include <string.h>

/*=========================================================================*
 *				env_panic				   *
 *=========================================================================*/
PUBLIC void env_panic(key)
char *key;		/* environment variable whose value is bogus */
{
  static char value[EP_BUF_SIZE] = "<unknown>";
  int s;
  if ((s=sys_getkenv(key, strlen(key), value, sizeof(value))) == 0) {
  	if (s != ESRCH)		/* only error allowed */
  	printf("WARNING: sys_getkenv() failed in env_panic(): %d\n", s);
  }
  printf("Bad environment setting: '%s = %s'\n", key, value);
  panic("","", NO_NUM);
}

