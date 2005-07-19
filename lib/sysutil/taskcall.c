/* _taskcall() is the same as _syscall() except it returns negative error
 * codes directly and not in errno.  This is a better interface for MM and
 * FS.
 */

#include <lib.h>
#include <minix/syslib.h>

PUBLIC int _taskcall(who, syscallnr, msgptr)
int who;
int syscallnr;
register message *msgptr;
{
  int status;

  msgptr->m_type = syscallnr;
  status = _sendrec(who, msgptr);
  if (status != 0) return(status);
  return(msgptr->m_type);
}
