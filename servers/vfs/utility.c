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
#include "file.h"
#include "fproc.h"
#include "param.h"
#include "vmnt.h"

PRIVATE int panicking;		/* inhibits recursive panics during sync */

/*===========================================================================*
 *				fetch_name				     *
 *===========================================================================*/
PUBLIC int fetch_name(path, len, flag)
char *path;			/* pointer to the path in user space */
int len;			/* path length, including 0 byte */
int flag;			/* M3 means path may be in message */
{
/* Go get path and put it in 'user_path'.
 * If 'flag' = M3 and 'len' <= M3_STRING, the path is present in 'message'.
 * If it is not, go copy it from user space.
 */
  register char *rpu, *rpm;
  int r;

  if (len > PATH_MAX) {
	err_code = ENAMETOOLONG;
	return(EGENERIC);
  }

  if(len >= sizeof(user_fullpath)) {
	panic(__FILE__, "fetch_name: len too much for user_fullpath", len);
  }

  /* Check name length for validity. */
  if (len <= 0) {
	err_code = EINVAL;
	printf("vfs: fetch_name: len %d?\n", len);
	return(EGENERIC);
  }

  if (flag == M3 && len <= M3_STRING) {
	/* Just copy the path from the message to 'user_path'. */
	rpu = &user_fullpath[0];
	rpm = m_in.pathname;		/* contained in input message */
	do { *rpu++ = *rpm++; } while (--len);
	r = OK;
  } else {
	/* String is not contained in the message.  Get it from user space. */
	r = sys_datacopy(who_e, (vir_bytes) path,
		FS_PROC_NR, (vir_bytes) user_fullpath, (phys_bytes) len);
  }

  if(user_fullpath[len-1] != '\0') {
	int i;
	printf("vfs: fetch_name: name not null-terminated: ");
	for(i = 0; i < len; i++) {
		printf("%c", user_fullpath[i]);
	}
	printf("\n");
	user_fullpath[len-1] = '\0';
  }

  return(r);
}

/*===========================================================================*
 *				no_sys					     *
 *===========================================================================*/
PUBLIC int no_sys()
{
/* Somebody has used an illegal system call number */
  printf("VFSno_sys: call %d from %d\n", call_nr, who_e);
  return(SUSPEND);
}

/*===========================================================================*
 *				panic					     *
 *===========================================================================*/
PUBLIC void panic(who, mess, num)
char *who;			/* who caused the panic */
char *mess;			/* panic message string */
int num;			/* number to go with it */
{
/* Something awful has happened.  Panics are caused when an internal
 * inconsistency is detected, e.g., a programming error or illegal value of a
 * defined constant.
 */
  if (panicking) return;	/* do not panic during a sync */
  panicking = TRUE;		/* prevent another panic during the sync */

  printf("VFS panic (%s): %s ", who, mess);
  if (num != NO_NUM) printf("%d",num); 
  (void) do_sync();		/* flush everything to the disk */
  sys_exit(SELF);
}


/*===========================================================================*
 *				isokendpt_f				     *
 *===========================================================================*/
PUBLIC int isokendpt_f(char *file, int line, int endpoint, int *proc, int fatal)
{
    int failed = 0;
    *proc = _ENDPOINT_P(endpoint);
    if(*proc < 0 || *proc >= NR_PROCS) {
        printf("vfs:%s:%d: proc (%d) from endpoint (%d) out of range\n",
                file, line, *proc, endpoint);
        failed = 1;
    } else if(fproc[*proc].fp_endpoint != endpoint) {
        printf("vfs:%s:%d: proc (%d) from endpoint (%d) doesn't match "
                "known endpoint (%d)\n",
                file, line, *proc, endpoint, fproc[*proc].fp_endpoint);
        failed = 1;
    }

    if(failed && fatal)
        panic(__FILE__, "isokendpt_f failed", NO_NUM);

    return failed ? EDEADSRCDST : OK;
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

  register int k;
  clock_t uptime;

  if ( (k=getuptime(&uptime)) != OK) panic(__FILE__,"clock_time err", k);
  return( (time_t) (boottime + (uptime/HZ)));
}

