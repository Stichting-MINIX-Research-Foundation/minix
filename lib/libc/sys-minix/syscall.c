#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#ifdef __weak_alias
__weak_alias(syscall, _syscall)
#endif

int _syscall(endpoint_t who, int syscallnr, message *msgptr)
{
  int status;

  msgptr->m_type = syscallnr;
  status = ipc_sendrec(who, msgptr);
  if (status != 0) {
	/* 'ipc_sendrec' itself failed. */
	/* XXX - strerror doesn't know all the codes */
	msgptr->m_type = status;
  }
  if (msgptr->m_type < 0) {
	errno = -msgptr->m_type;
	return(-1);
  }
  return(msgptr->m_type);
}
