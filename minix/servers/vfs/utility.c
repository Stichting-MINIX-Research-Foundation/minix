/* This file contains a few general purpose utility routines.
 *
 * The entry points into this file are
 *   clock_timespec: ask the clock task for the real time
 *   copy_path:	  copy a path name from a path request from userland
 *   fetch_name:  go get a path name from user space
 *   panic:       something awful has occurred;  MINIX cannot continue
 *   in_group:    determines if group 'grp' is in rfp->fp_sgroups[]
 */

#include "fs.h"
#include <minix/callnr.h>
#include <minix/endpoint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "file.h"
#include "vmnt.h"

/*===========================================================================*
 *				copy_path				     *
 *===========================================================================*/
int copy_path(char *dest, size_t size)
{
/* Go get the path for a path request. Put the result in in 'dest', which
 * should be at least PATH_MAX in size.
 */
  vir_bytes name;
  size_t len;

  assert(size >= PATH_MAX);

  name = job_m_in.m_lc_vfs_path.name;
  len = job_m_in.m_lc_vfs_path.len;

  if (len > size) {	/* 'len' includes terminating-nul */
	err_code = ENAMETOOLONG;
	return(EGENERIC);
  }

  /* Is the string contained in the message? If not, perform a normal copy. */
  if (len > M_PATH_STRING_MAX)
	return fetch_name(name, len, dest);

  /* Just copy the path from the message */
  strncpy(dest, job_m_in.m_lc_vfs_path.buf, len);

  if (dest[len - 1] != '\0') {
	err_code = ENAMETOOLONG;
	return(EGENERIC);
  }

  return(OK);
}

/*===========================================================================*
 *				fetch_name				     *
 *===========================================================================*/
int fetch_name(vir_bytes path, size_t len, char *dest)
{
/* Go get path and put it in 'dest'.  */
  int r;

  if (len > PATH_MAX) {	/* 'len' includes terminating-nul */
	err_code = ENAMETOOLONG;
	return(EGENERIC);
  }

  /* Check name length for validity. */
  if (len > SSIZE_MAX) {
	err_code = EINVAL;
	return(EGENERIC);
  }

  /* String is not contained in the message.  Get it from user space. */
  r = sys_datacopy_wrapper(who_e, path, VFS_PROC_NR, (vir_bytes) dest, len);
  if (r != OK) {
	err_code = EINVAL;
	return(r);
  }

  if (dest[len - 1] != '\0') {
	err_code = ENAMETOOLONG;
	return(EGENERIC);
  }

  return(OK);
}

/*===========================================================================*
 *				isokendpt_f				     *
 *===========================================================================*/
int isokendpt_f(const char *file, int line, endpoint_t endpoint, int *proc,
       int fatal)
{
  int failed = 0;
  endpoint_t ke;
  *proc = _ENDPOINT_P(endpoint);
  if (endpoint == NONE) {
	printf("VFS %s:%d: endpoint is NONE\n", file, line);
	failed = 1;
  } else if (*proc < 0 || *proc >= NR_PROCS) {
	printf("VFS %s:%d: proc (%d) from endpoint (%d) out of range\n",
		file, line, *proc, endpoint);
	failed = 1;
  } else if ((ke = fproc[*proc].fp_endpoint) != endpoint) {
	if(ke == NONE) {
		assert(fproc[*proc].fp_pid == PID_FREE);
	} else {
		printf("VFS %s:%d: proc (%d) from endpoint (%d) doesn't match "
			"known endpoint (%d)\n", file, line, *proc, endpoint,
			fproc[*proc].fp_endpoint);
		assert(fproc[*proc].fp_pid != PID_FREE);
	}
	failed = 1;
  }

  if(failed && fatal)
	panic("isokendpt_f failed");

  return(failed ? EDEADEPT : OK);
}


/*===========================================================================*
 *				clock_timespec				     *
 *===========================================================================*/
struct timespec clock_timespec(void)
{
/* This routine returns the time in seconds since 1.1.1970.  MINIX is an
 * astrophysically naive system that assumes the earth rotates at a constant
 * rate and that such things as leap seconds do not exist.
 */

  register int r;
  struct timespec tv;
  clock_t uptime;
  clock_t realtime;
  time_t boottime;

  r = getuptime(&uptime, &realtime, &boottime);
  if (r != OK)
	panic("clock_timespec err: %d", r);

  tv.tv_sec = boottime + (realtime/system_hz);
  /* We do not want to overflow, and system_hz can be as high as 50kHz */
  assert(system_hz < LONG_MAX/40000);
  tv.tv_nsec = (realtime%system_hz) * 40000 / system_hz * 25000;
  return tv;
}

/*===========================================================================*
 *                              in_group                                     *
 *===========================================================================*/
int in_group(struct fproc *rfp, gid_t grp)
{
  int i;

  for (i = 0; i < rfp->fp_ngroups; i++)
	if (rfp->fp_sgroups[i] == grp)
		return(OK);

  return(EINVAL);
}

/*===========================================================================*
 *                              sys_datacopy_wrapper                         *
 *===========================================================================*/
int sys_datacopy_wrapper(endpoint_t src, vir_bytes srcv,
	endpoint_t dst, vir_bytes dstv, size_t len)
{
	/* Safe function to copy data from or to a user buffer.
	 * VFS has to be a bit more careful as a regular copy
	 * might trigger VFS action needed by VM while it's
	 * blocked on the kernel call. This wrapper tries the
	 * copy, invokes VM itself asynchronously if necessary,
	 * then tries the copy again.
	 *
	 * This function assumes it's between VFS and a user process,
	 * and therefore one of the endpoints is SELF (VFS).
	 */
	int r;
	endpoint_t them = NONE;
	vir_bytes themv = -1;
	int writable = -1;

	r = sys_datacopy_try(src, srcv, dst, dstv, len);

	if(src == VFS_PROC_NR) src = SELF;
	if(dst == VFS_PROC_NR) dst = SELF;

	assert(src == SELF || dst == SELF);

	if(src == SELF) { them = dst; themv = dstv; writable = 1; }
	if(dst == SELF) { them = src; themv = srcv; writable = 0; }

	assert(writable >= 0);
	assert(them != SELF);

	if(r == EFAULT) {
		/* The copy has failed with EFAULT, this means the kernel has
		 * given up but it might not be a legitimate error. Ask VM.
		 */
		if((r=vm_vfs_procctl_handlemem(them, themv, len, writable)) != OK) {
			return r;
		}

		r = sys_datacopy_try(src, srcv, dst, dstv, len);
	}

	return r;
}

