#include <lib.h>
#include <errno.h>
#include <sys/ucred.h>
#include <unistd.h>

int getnucred(endpoint_t proc_ep, struct ucred *ucred)
{
  message m;
  pid_t pid;

  if (ucred == NULL) {
    errno = EFAULT;
    return -1;
  }

  m.m1_i1 = proc_ep;		/* search for this process */

  pid = _syscall(PM_PROC_NR, GETEPINFO, &m);
  if (pid < 0) {
     return -1;
  }

  ucred->pid = pid;
  ucred->uid = m.PM_NUID;
  ucred->gid = m.PM_NGID;

  return 0;
}
