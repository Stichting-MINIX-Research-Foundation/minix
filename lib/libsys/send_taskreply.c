
#include <string.h>

#include "syslib.h"

/*===========================================================================*
 *				sys_taskreply				     *
 *===========================================================================*/
int send_taskreply(endpoint_t who, endpoint_t endpoint, int status)
{
  message m;

  memset(&m, 0, sizeof(m));

  m.REP_ENDPT = endpoint;
  m.REP_STATUS = status;

  return _sendcall(who, TASK_REPLY, &m);
}

