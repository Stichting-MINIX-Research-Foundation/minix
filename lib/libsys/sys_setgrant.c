
#include "syslib.h"

#include <minix/safecopies.h>

int sys_setgrant(cp_grant_t *grants, int ngrants)
{
  message m;

  m.SG_ADDR = (char *) grants;
  m.SG_SIZE = ngrants;

  return _kernel_call(SYS_SETGRANT, &m);
}
