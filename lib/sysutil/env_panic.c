#include "sysutil.h"
#include <string.h>

/*=========================================================================*
 *				env_panic				   *
 *=========================================================================*/
PUBLIC void env_panic(key)
char *key;		/* environment variable whose value is bogus */
{
  static char value[EP_BUF_SIZE] = "<unknown>";
  int s;
  if ((s=env_get_param(key, value, sizeof(value))) == 0) {
  	if (s != ESRCH)		/* only error allowed */
  	printf("WARNING: get_mon_param() failed in env_panic(): %d\n", s);
  }
  printf("Bad environment setting: '%s = %s'\n", key, value);
  panic("","", NO_NUM);
}

