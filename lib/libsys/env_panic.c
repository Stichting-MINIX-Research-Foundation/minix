#include "sysutil.h"
#include <string.h>

/*=========================================================================*
 *				env_panic				   *
 *=========================================================================*/
void env_panic(const char *key)
{
  static char value[EP_BUF_SIZE] = "<unknown>";
  int s;
  if ((s=env_get_param(key, value, sizeof(value))) == 0) {
  	if (s != ESRCH)		/* only error allowed */
  	printf("WARNING: env_get_param() failed in env_panic(): %d\n", s);
  }
  panic("Bad environment setting: '%s = %s'\n", key, value);
}

