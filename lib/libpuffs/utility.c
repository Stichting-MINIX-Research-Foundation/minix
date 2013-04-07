/* Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 */

#include "fs.h"

#include <assert.h>
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
 *				clock_timespec				     *
 *===========================================================================*/
struct timespec clock_timespec()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */
  static long system_hz = 0;

  register int k;
  struct timespec tv;
  clock_t uptime;
  clock_t realtime;
  time_t boottime;

  if (system_hz == 0) system_hz = sys_hz();
  if ((k=getuptime(&uptime, &realtime, &boottime)) != OK)
	panic("clock_timespec: getuptime failed: %d", k);

  tv.tv_sec = (time_t) (boottime + (realtime/system_hz));
  /* We do not want to overflow, and system_hz can be as high as 50kHz */
  assert(system_hz < LONG_MAX/40000);
  tv.tv_nsec = (realtime%system_hz) * 40000 / system_hz * 25000;
  return tv;
}


/*===========================================================================*
 *				update_timens				     *
 *===========================================================================*/
int update_timens(struct puffs_node *pn, int flags, struct timespec *t)
{
  int r;
  struct vattr va;
  struct timespec new_time;
  PUFFS_MAKECRED(pcr, &global_kcred);

  if (!flags)
	return 0;

  if (global_pu->pu_ops.puffs_node_setattr == NULL)
	return EINVAL;

  new_time = t != NULL ? *t : clock_timespec();
  
  puffs_vattr_null(&va);
  /* librefuse modifies atime and mtime together,
   * so set old values to avoid setting either one
   * to PUFFS_VNOVAL (set by puffs_vattr_null).
   */
  va.va_atime = pn->pn_va.va_atime;
  va.va_mtime = pn->pn_va.va_mtime;

  if (flags & ATIME)
	va.va_atime = new_time;
  if (flags & MTIME)
	va.va_mtime = new_time;
  if (flags & CTIME)
	va.va_ctime = new_time;

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
