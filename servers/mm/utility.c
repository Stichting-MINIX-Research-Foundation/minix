/* This file contains some utility routines for MM.
 *
 * The entry points are:
 *   allowed:	see if an access is permitted
 *   no_sys:	this routine is called for invalid system call numbers
 *   panic:	MM has run aground of a fatal error and cannot continue
 *   tell_fs:	interface to FS
 */

#include "mm.h"
#include <sys/stat.h>
#include <minix/callnr.h>
#include <minix/com.h>
#include <fcntl.h>
#include <signal.h>		/* needed only because mproc.h needs it */
#include "mproc.h"
#include "param.h"

/*===========================================================================*
 *				allowed					     *
 *===========================================================================*/
PUBLIC int allowed(name_buf, s_buf, mask)
char *name_buf;			/* pointer to file name to be EXECed */
struct stat *s_buf;		/* buffer for doing and returning stat struct*/
int mask;			/* R_BIT, W_BIT, or X_BIT */
{
/* Check to see if file can be accessed.  Return EACCES or ENOENT if the access
 * is prohibited.  If it is legal open the file and return a file descriptor.
 */

  int fd;
  int save_errno;

  /* Use the fact that mask for access() is the same as the permissions mask.
   * E.g., X_BIT in <minix/const.h> is the same as X_OK in <unistd.h> and
   * S_IXOTH in <sys/stat.h>.  tell_fs(DO_CHDIR, ...) has set MM's real ids
   * to the user's effective ids, so access() works right for setuid programs.
   */
  if (access(name_buf, mask) < 0) return(-errno);

  /* The file is accessible but might not be readable.  Make it readable. */
  tell_fs(SETUID, MM_PROC_NR, (int) SUPER_USER, (int) SUPER_USER);

  /* Open the file and fstat it.  Restore the ids early to handle errors. */
  fd = open(name_buf, O_RDONLY | O_NONBLOCK);
  save_errno = errno;		/* open might fail, e.g. from ENFILE */
  tell_fs(SETUID, MM_PROC_NR, (int) mp->mp_effuid, (int) mp->mp_effuid);
  if (fd < 0) return(-save_errno);
  if (fstat(fd, s_buf) < 0) panic("allowed: fstat failed", NO_NUM);

  /* Only regular files can be executed. */
  if (mask == X_BIT && (s_buf->st_mode & I_TYPE) != I_REGULAR) {
	close(fd);
	return(EACCES);
  }
  return(fd);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* A system call number not implemented by MM has been requested. */

  return(EINVAL);
}


/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(format, num)
char *format;			/* format string */
int num;			/* number to go with format string */
{
/* Something awful has happened.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant.
 */

  printf("Memory manager panic: %s ", format);
  if (num != NO_NUM) printf("%d",num);
  printf("\n");
  tell_fs(SYNC, 0, 0, 0);	/* flush the cache to the disk */
  sys_abort(RBT_PANIC);
}


/*===========================================================================*
 *				tell_fs					     *
 *===========================================================================*/
PUBLIC void tell_fs(what, p1, p2, p3)
int what, p1, p2, p3;
{
/* This routine is only used by MM to inform FS of certain events:
 *      tell_fs(CHDIR, slot, dir, 0)
 *      tell_fs(EXEC, proc, 0, 0)
 *      tell_fs(EXIT, proc, 0, 0)
 *      tell_fs(FORK, parent, child, pid)
 *      tell_fs(SETGID, proc, realgid, effgid)
 *      tell_fs(SETSID, proc, 0, 0)
 *      tell_fs(SETUID, proc, realuid, effuid)
 *      tell_fs(SYNC, 0, 0, 0)
 *      tell_fs(UNPAUSE, proc, signr, 0)
 */
  message m;

  m.tell_fs_arg1 = p1;
  m.tell_fs_arg2 = p2;
  m.tell_fs_arg3 = p3;
  _taskcall(FS_PROC_NR, what, &m);
}
