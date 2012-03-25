/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"

#include <stdarg.h>

#include "puffs.h"
#include "puffs_priv.h"


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
int no_sys()
{
/* Somebody has used an illegal system call number */
  lpuffs_debug("no_sys: invalid call %d\n", req_nr);
  return(EINVAL);
}


/*===========================================================================*
 *                              mfs_nul                                      *
 *===========================================================================*/
void mfs_nul_f(const char *file, int line, char *str, unsigned int len,
                      unsigned int maxlen)
{
  if (len < maxlen && str[len-1] != '\0') {
	lpuffs_debug("%s:%d string (length %d,maxlen %d) not null-terminated\n",
                file, line, len, maxlen);
  }
}


/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  register int k;
  clock_t uptime;
  time_t boottime;

  if ((k=getuptime2(&uptime, &boottime)) != OK)
	panic("clock_time: getuptme2 failed: %d", k);

  return( (time_t) (boottime + (uptime/sys_hz())));
}


/*===========================================================================*
 *				update_times				     *
 *===========================================================================*/
int update_times(struct puffs_node *pn, int flags, time_t t)
{
  int r;
  struct vattr va;
  time_t new_time;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (!flags)
	return 0;

  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return EINVAL;

  new_time = t != 0 ? t : clock_time();
  
  puffs_vattr_null(&va);
  /* librefuse modifies atime and mtime together,
   * so set old values to avoid setting either one
   * to PUFFS_VNOVAL (set by puffs_vattr_null).
   */
  va.va_atime.tv_sec = pn->pn_va.va_atime.tv_sec;
  va.va_mtime.tv_sec = pn->pn_va.va_mtime.tv_sec;

  if (flags & ATIME) {
	va.va_atime.tv_sec = new_time;
	va.va_atime.tv_nsec = 0;
  }
  if (flags & MTIME) {
	va.va_mtime.tv_sec = new_time;
	va.va_mtime.tv_nsec = 0;
  }
  if (flags & CTIME) {
	va.va_ctime.tv_sec = new_time;
	va.va_ctime.tv_nsec = 0;
  }

  r = global_pu->pu_ops.puffs_node_setattr(global_pu, pn, &va, pcr);

  return(r);
}


/*===========================================================================*
 *				lpuffs_debug				     *
 *===========================================================================*/
void lpuffs_debug(const char *format, ...)
{   
  char buffer[256];
  va_list args;
  va_start (args, format);
  vsprintf (buffer,format, args);
  printf("%s: %s", fs_name, buffer);
  va_end (args);
}
