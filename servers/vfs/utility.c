/* This file contains a few general purpose utility routines.
 *
 * The entry points into this file are
 *   clock_time:  ask the clock task for the real time
 *   copy:	  copy a block of data
 *   fetch_name:  go get a path name from user space
 *   no_sys:      reject a system call that FS does not handle
 *   panic:       something awful has occurred;  MINIX cannot continue
 *   conv2:	  do byte swapping on a 16-bit int
 *   conv4:	  do byte swapping on a 32-bit long
 */

#include "fs.h"
#include <minix/com.h>
#include <minix/endpoint.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include "file.h"
#include "fproc.h"
#include "param.h"
#include "vmnt.h"

/*===========================================================================*
 *				fetch_name				     *
 *===========================================================================*/
PUBLIC int fetch_name(path, len, flag)
char *path;			/* pointer to the path in user space */
int len;			/* path length, including 0 byte */
int flag;			/* M3 means path may be in message */
{
/* Go get path and put it in 'user_fullpath'.
 * If 'flag' = M3 and 'len' <= M3_STRING, the path is present in 'message'.
 * If it is not, go copy it from user space.
 */
  register char *rpu, *rpm;
  int r, count;

  if (len > PATH_MAX) {
	err_code = ENAMETOOLONG;
	return(EGENERIC);
  }

  if(len >= sizeof(user_fullpath)) 
	panic(__FILE__, "fetch_name: len too much for user_fullpath", len);

  /* Check name length for validity. */
  if (len <= 0) {
	err_code = EINVAL;
	return(EGENERIC);
  }

  if (flag == M3 && len <= M3_STRING) {
	/* Just copy the path from the message to 'user_fullpath'. */
	rpu = &user_fullpath[0];
	rpm = m_in.pathname;		/* contained in input message */
	count = len;
	do { *rpu++ = *rpm++; } while (--count);
	r = OK;
  } else {
	/* String is not contained in the message.  Get it from user space. */
	r = sys_datacopy(who_e, (vir_bytes) path,
		FS_PROC_NR, (vir_bytes) user_fullpath, (phys_bytes) len);
  }

  if (user_fullpath[len - 1] != '\0') {
  	err_code = ENAMETOOLONG;
  	return(EGENERIC);
  }

  return(r);
}


/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */
  printf("VFS no_sys: call %d from %d (pid %d)\n", call_nr, who_e, who_p);
  return(ENOSYS);
}


/*===========================================================================*
 *				isokendpt_f				     *
 *===========================================================================*/
PUBLIC int isokendpt_f(char *file, int line, int endpoint, int *proc, int fatal)
{
  int failed = 0;
  endpoint_t ke;
  *proc = _ENDPOINT_P(endpoint);
  if(endpoint == NONE) {
	printf("vfs:%s: endpoint is NONE\n", file, line, endpoint);
	failed = 1;
  } else if(*proc < 0 || *proc >= NR_PROCS) {
	printf("vfs:%s:%d: proc (%d) from endpoint (%d) out of range\n",
		file, line, *proc, endpoint);
	failed = 1;
  } else if((ke=fproc[*proc].fp_endpoint) != endpoint) {
	if(ke == NONE) {
		printf("vfs:%s:%d: endpoint (%d) points to NONE slot (%d)\n",
                	file, line, endpoint, *proc);
		assert(fproc[*proc].fp_pid == PID_FREE);
  	} else {
		printf("vfs:%s:%d: proc (%d) from endpoint (%d) doesn't match "
			"known endpoint (%d)\n", file, line, *proc, endpoint,
			fproc[*proc].fp_endpoint);
		assert(fproc[*proc].fp_pid != PID_FREE);
	}
	failed = 1;
  }

  if(failed && fatal)
	panic(__FILE__, "isokendpt_f failed", NO_NUM);

  return(failed ? EDEADSRCDST : OK);
}


/*===========================================================================*
 *				clock_time				     *
 *===========================================================================*/
PUBLIC time_t clock_time()
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  register int r;
  clock_t uptime;
  time_t boottime;

  r = getuptime2(&uptime, &boottime);
  if (r != OK)
	panic(__FILE__,"clock_time err", r);

  return( (time_t) (boottime + (uptime/system_hz)));
}

