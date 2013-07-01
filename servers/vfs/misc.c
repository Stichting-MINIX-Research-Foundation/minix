/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_fcntl:	  perform the FCNTL system call
 *   do_sync:	  perform the SYNC system call
 *   do_fsync:	  perform the FSYNC system call
 *   pm_reboot:	  sync disks and prepare for shutdown
 *   pm_fork:	  adjust the tables after PM has performed a FORK system call
 *   do_exec:	  handle files with FD_CLOEXEC on after PM has done an EXEC
 *   do_exit:	  a process has exited; note that in the tables
 *   do_set:	  set uid or gid for some process
 *   do_revive:	  revive a process that was waiting for something (e.g. TTY)
 *   do_svrctl:	  file system control
 *   do_getsysinfo:	request copy of FS data structure
 *   pm_dumpcore: create a core dump
 */

#include "fs.h"
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <minix/callnr.h>
#include <minix/safecopies.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/sysinfo.h>
#include <minix/u64.h>
#include <sys/ptrace.h>
#include <sys/svrctl.h>
#include <sys/resource.h>
#include "file.h"
#include "fproc.h"
#include "scratchpad.h"
#include "dmap.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"
#include "param.h"

#define CORE_NAME	"core"
#define CORE_MODE	0777	/* mode to use on core image files */

#if ENABLE_SYSCALL_STATS
unsigned long calls_stats[NCALLS];
#endif

static void free_proc(struct fproc *freed, int flags);
/*
static int dumpcore(int proc_e, struct mem_map *seg_ptr);
static int write_bytes(struct inode *rip, off_t off, char *buf, size_t
	bytes);
static int write_seg(struct inode *rip, off_t off, int proc_e, int seg,
	off_t seg_off, phys_bytes seg_bytes);
*/

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
int do_getsysinfo()
{
  vir_bytes src_addr, dst_addr;
  size_t len, buf_size;
  int what;

  what = job_m_in.SI_WHAT;
  dst_addr = (vir_bytes) job_m_in.SI_WHERE;
  buf_size = (size_t) job_m_in.SI_SIZE;

  /* Only su may call do_getsysinfo. This call may leak information (and is not
   * stable enough to be part of the API/ABI). In the future, requests from
   * non-system processes should be denied.
   */

  if (!super_user) return(EPERM);

  switch(what) {
    case SI_PROC_TAB:
	src_addr = (vir_bytes) fproc;
	len = sizeof(struct fproc) * NR_PROCS;
	break;
    case SI_DMAP_TAB:
	src_addr = (vir_bytes) dmap;
	len = sizeof(struct dmap) * NR_DEVICES;
	break;
#if ENABLE_SYSCALL_STATS
    case SI_CALL_STATS:
	src_addr = (vir_bytes) calls_stats;
	len = sizeof(calls_stats);
	break;
#endif
    case SI_VMNT_TAB:
	fetch_vmnt_paths();
	src_addr = (vir_bytes) vmnt;
	len = sizeof(struct vmnt) * NR_MNTS;
	break;
    default:
	return(EINVAL);
  }

  if (len != buf_size)
	return(EINVAL);

  return sys_datacopy(SELF, src_addr, who_e, dst_addr, len);
}

/*===========================================================================*
 *				do_fcntl				     *
 *===========================================================================*/
int do_fcntl(message *UNUSED(m_out))
{
/* Perform the fcntl(fd, request, ...) system call. */

  register struct filp *f;
  int new_fd, fl, r = OK, fcntl_req, fcntl_argx;
  tll_access_t locktype;

  scratch(fp).file.fd_nr = job_m_in.fd;
  scratch(fp).io.io_buffer = job_m_in.buffer;
  scratch(fp).io.io_nbytes = job_m_in.nbytes;	/* a.k.a. m_in.request */
  fcntl_req = job_m_in.request;
  fcntl_argx = job_m_in.addr;

  /* Is the file descriptor valid? */
  locktype = (fcntl_req == F_FREESP) ? VNODE_WRITE : VNODE_READ;
  if ((f = get_filp(scratch(fp).file.fd_nr, locktype)) == NULL)
	return(err_code);

  switch (fcntl_req) {
    case F_DUPFD:
	/* This replaces the old dup() system call. */
	if (fcntl_argx < 0 || fcntl_argx >= OPEN_MAX) r = EINVAL;
	else if ((r = get_fd(fp, fcntl_argx, 0, &new_fd, NULL)) == OK) {
		f->filp_count++;
		fp->fp_filp[new_fd] = f;
		FD_SET(new_fd, &fp->fp_filp_inuse);
		r = new_fd;
	}
	break;

    case F_GETFD:
	/* Get close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	r = 0;
	if (FD_ISSET(scratch(fp).file.fd_nr, &fp->fp_cloexec_set))
		r = FD_CLOEXEC;
	break;

    case F_SETFD:
	/* Set close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	if (fcntl_argx & FD_CLOEXEC)
		FD_SET(scratch(fp).file.fd_nr, &fp->fp_cloexec_set);
	else
		FD_CLR(scratch(fp).file.fd_nr, &fp->fp_cloexec_set);
	break;

    case F_GETFL:
	/* Get file status flags (O_NONBLOCK and O_APPEND). */
	fl = f->filp_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
	r = fl;
	break;

    case F_SETFL:
	/* Set file status flags (O_NONBLOCK and O_APPEND). */
	fl = O_NONBLOCK | O_APPEND | O_REOPEN;
	f->filp_flags = (f->filp_flags & ~fl) | (fcntl_argx & fl);
	break;

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
	/* Set or clear a file lock. */
	r = lock_op(f, fcntl_req);
	break;

    case F_FREESP:
     {
	/* Free a section of a file */
	off_t start, end;
	struct flock flock_arg;
	signed long offset;

	/* Check if it's a regular file. */
	if (!S_ISREG(f->filp_vno->v_mode)) r = EINVAL;
	else if (!(f->filp_mode & W_BIT)) r = EBADF;
	else
		/* Copy flock data from userspace. */
		r = sys_datacopy(who_e, (vir_bytes) scratch(fp).io.io_buffer,
				 SELF, (vir_bytes) &flock_arg,
				 sizeof(flock_arg));

	if (r != OK) break;

	/* Convert starting offset to signed. */
	offset = (signed long) flock_arg.l_start;

	/* Figure out starting position base. */
	switch(flock_arg.l_whence) {
	  case SEEK_SET: start = 0; break;
	  case SEEK_CUR:
		if (ex64hi(f->filp_pos) != 0)
			panic("do_fcntl: position in file too high");
		start = ex64lo(f->filp_pos);
		break;
	  case SEEK_END: start = f->filp_vno->v_size; break;
	  default: r = EINVAL;
	}
	if (r != OK) break;

	/* Check for overflow or underflow. */
	if (offset > 0 && start + offset < start) r = EINVAL;
	else if (offset < 0 && start + offset > start) r = EINVAL;
	else {
		start += offset;
		if (start < 0) r = EINVAL;
	}
	if (r != OK) break;

	if (flock_arg.l_len != 0) {
		if (start >= f->filp_vno->v_size) r = EINVAL;
		else if ((end = start + flock_arg.l_len) <= start) r = EINVAL;
		else if (end > f->filp_vno->v_size) end = f->filp_vno->v_size;
	} else {
                end = 0;
	}
	if (r != OK) break;

	r = req_ftrunc(f->filp_vno->v_fs_e, f->filp_vno->v_inode_nr,start,end);

	if (r == OK && flock_arg.l_len == 0)
		f->filp_vno->v_size = start;

	break;
     }
    case F_GETNOSIGPIPE:
	/* POSIX: return value other than -1 is flag is set, else -1 */
	r = -1;
	if (f->filp_flags & O_NOSIGPIPE)
		r = 0;
	break;
    case F_SETNOSIGPIPE:
	fl = (O_NOSIGPIPE);
	f->filp_flags = (f->filp_flags & ~fl) | (fcntl_argx & fl);
	break;
    default:
	r = EINVAL;
  }

  unlock_filp(f);
  return(r);
}

static int
sync_fses(void)
{
  struct vmnt *vmp;
  int r = OK;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	if ((r = lock_vmnt(vmp, VMNT_READ)) != OK)
		break;
	if (vmp->m_dev != NO_DEV && vmp->m_fs_e != NONE &&
		 vmp->m_root_node != NULL) {
		req_sync(vmp->m_fs_e);
	}
	unlock_vmnt(vmp);
  }

  return(r);
}

/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
int do_sync(message *UNUSED(m_out))
{
  return sync_fses();
}

/*===========================================================================*
 *				do_fsync				     *
 *===========================================================================*/
int do_fsync(message *UNUSED(m_out))
{
/* Perform the fsync() system call. */
  struct filp *rfilp;
  struct vmnt *vmp;
  dev_t dev;
  int r = OK;

  scratch(fp).file.fd_nr = job_m_in.fd;

  if ((rfilp = get_filp(scratch(fp).file.fd_nr, VNODE_READ)) == NULL)
	return(err_code);

  dev = rfilp->filp_vno->v_dev;
  unlock_filp(rfilp);

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	if (vmp->m_dev != dev) continue;
	if ((r = lock_vmnt(vmp, VMNT_READ)) != OK)
		break;
	if (vmp->m_dev != NO_DEV && vmp->m_dev == dev &&
		vmp->m_fs_e != NONE && vmp->m_root_node != NULL) {

		req_sync(vmp->m_fs_e);
	}
	unlock_vmnt(vmp);
  }

  return(r);
}

int dupvm(struct fproc *rfp, int pfd, int *vmfd, struct filp **newfilp)
{
	int result, procfd;
	struct filp *f = NULL;
	struct fproc *vmf = &fproc[VM_PROC_NR];

	*newfilp = NULL;

	if ((f = get_filp2(rfp, pfd, VNODE_READ)) == NULL) {
		printf("VFS dupvm: get_filp2 failed\n");
		return EBADF;
	}

	if(!f->filp_vno->v_vmnt->m_haspeek) {
		unlock_filp(f);
		printf("VFS dupvm: no peek available\n");
		return EINVAL;
	}

	assert(f->filp_vno);
	assert(f->filp_vno->v_vmnt);

	if (!S_ISREG(f->filp_vno->v_mode) && !S_ISBLK(f->filp_vno->v_mode)) {
		printf("VFS: mmap regular/blockdev only; dev 0x%x ino %d has mode 0%o\n",
			f->filp_vno->v_dev, f->filp_vno->v_inode_nr, f->filp_vno->v_mode);
		unlock_filp(f);
		return EINVAL;
	}

	/* get free FD in VM */
	if((result=get_fd(vmf, 0, 0, &procfd, NULL)) != OK) {
		unlock_filp(f);
		printf("VFS dupvm: getfd failed\n");
		return result;
	}

	*vmfd = procfd;

	f->filp_count++;
	assert(f->filp_count > 0);
	vmf->fp_filp[procfd] = f;

	/* mmap FD's are inuse */
	FD_SET(procfd, &vmf->fp_filp_inuse);

	*newfilp = f;

	return OK;
}

/*===========================================================================*
 *				do_vm_call				     *
 *===========================================================================*/
int do_vm_call(message *m_out)
{
/* A call that VM does to VFS.
 * We must reply with the fixed type VM_VFS_REPLY (and put our result info
 * in the rest of the message) so VM can tell the difference between a
 * request from VFS and a reply to this call.
 */
	int req = job_m_in.VFS_VMCALL_REQ;
	int req_fd = job_m_in.VFS_VMCALL_FD;
	u32_t req_id = job_m_in.VFS_VMCALL_REQID;
	endpoint_t ep = job_m_in.VFS_VMCALL_ENDPOINT;
	u64_t offset = make64(job_m_in.VFS_VMCALL_OFFSET_LO,
		job_m_in.VFS_VMCALL_OFFSET_HI);
	u32_t length = job_m_in.VFS_VMCALL_LENGTH;
	int result = OK;
	int slot;
	struct fproc *rfp, *vmf;
	struct filp *f = NULL;
	int r;

	if(job_m_in.m_source != VM_PROC_NR)
		return ENOSYS;

	if(isokendpt(ep, &slot) != OK) rfp = NULL;
	else rfp = &fproc[slot];

	vmf = &fproc[VM_PROC_NR];
	assert(fp == vmf);
	assert(rfp != vmf);

	switch(req) {
		case VMVFSREQ_FDLOOKUP:
		{
			int procfd;

			/* Lookup fd in referenced process. */

			if(!rfp) {
				printf("VFS: why isn't ep %d here?!\n", ep);
				result = ESRCH;
				goto reqdone;
			}

			if((result = dupvm(rfp, req_fd, &procfd, &f)) != OK) {
				printf("vfs: dupvm failed\n");
				goto reqdone;
			}

			if(S_ISBLK(f->filp_vno->v_mode)) {
				assert(f->filp_vno->v_sdev != NO_DEV);
				m_out->VMV_DEV = f->filp_vno->v_sdev;
				m_out->VMV_INO = VMC_NO_INODE;
				m_out->VMV_SIZE_PAGES = LONG_MAX;
			} else {
				m_out->VMV_DEV = f->filp_vno->v_dev;
				m_out->VMV_INO = f->filp_vno->v_inode_nr;
				m_out->VMV_SIZE_PAGES =
					roundup(f->filp_vno->v_size,
						PAGE_SIZE)/PAGE_SIZE;
			}

			m_out->VMV_FD = procfd;

			result = OK;

			break;
		}
		case VMVFSREQ_FDCLOSE:
		{
			result = close_fd(fp, req_fd);
			if(result != OK) {
				printf("VFS: VM fd close for fd %d, %d (%d)\n",
					req_fd, fp->fp_endpoint, result);
			}
			break;
		}
		case VMVFSREQ_FDIO:
		{
			message dummy_out;

			result = actual_llseek(fp, &dummy_out, req_fd,
				SEEK_SET, offset);

			if(result == OK) {
				result = actual_read_write_peek(fp, PEEKING,
					req_fd, NULL, length);
			}

			break;
		}
		default:
			panic("VFS: bad request code from VM\n");
			break;
	}

reqdone:
	if(f)
		unlock_filp(f);

	/* fp is VM still. */
	assert(fp == vmf);
	m_out->VMV_ENDPOINT = ep;
	m_out->VMV_RESULT = result;
	m_out->VMV_REQID = req_id;

	/* reply asynchronously as VM may not be able to receive
	 * a sendnb() message
	 */

	m_out->m_type = VM_VFS_REPLY;
	r = asynsend3(VM_PROC_NR, m_out, 0);
	if(r != OK) printf("VFS: couldn't asynsend3() to VM\n");

	/* VFS does not reply any further */
	return SUSPEND;
}

/*===========================================================================*
 *				pm_reboot				     *
 *===========================================================================*/
void pm_reboot()
{
/* Perform the VFS side of the reboot call. */
  int i;
  struct fproc *rfp;

  sync_fses();

  /* Do exit processing for all leftover processes and servers, but don't
   * actually exit them (if they were really gone, PM will tell us about it).
   * Skip processes that handle parts of the file system; we first need to give
   * them the chance to unmount (which should be possible as all normal
   * processes have no open files anymore).
   */
  for (i = 0; i < NR_PROCS; i++) {
	rfp = &fproc[i];

	/* Don't just free the proc right away, but let it finish what it was
	 * doing first */
	lock_proc(rfp, 0);
	if (rfp->fp_endpoint != NONE && find_vmnt(rfp->fp_endpoint) == NULL)
		free_proc(rfp, 0);
	unlock_proc(rfp);
  }

  sync_fses();
  unmount_all(0 /* Don't force */);

  /* Try to exit all processes again including File Servers */
  for (i = 0; i < NR_PROCS; i++) {
	rfp = &fproc[i];

	/* Don't just free the proc right away, but let it finish what it was
	 * doing first */
	lock_proc(rfp, 0);
	if (rfp->fp_endpoint != NONE)
		free_proc(rfp, 0);
	unlock_proc(rfp);
  }

  sync_fses();
  unmount_all(1 /* Force */);

}

/*===========================================================================*
 *				pm_fork					     *
 *===========================================================================*/
void pm_fork(endpoint_t pproc, endpoint_t cproc, pid_t cpid)
{
/* Perform those aspects of the fork() system call that relate to files.
 * In particular, let the child inherit its parent's file descriptors.
 * The parent and child parameters tell who forked off whom. The file
 * system uses the same slot numbers as the kernel.  Only PM makes this call.
 */

  struct fproc *cp, *pp;
  int i, parentno, childno;
  mutex_t c_fp_lock;

  /* Check up-to-dateness of fproc. */
  okendpt(pproc, &parentno);

  /* PM gives child endpoint, which implies process slot information.
   * Don't call isokendpt, because that will verify if the endpoint
   * number is correct in fproc, which it won't be.
   */
  childno = _ENDPOINT_P(cproc);
  if (childno < 0 || childno >= NR_PROCS)
	panic("VFS: bogus child for forking: %d", cproc);
  if (fproc[childno].fp_pid != PID_FREE)
	panic("VFS: forking on top of in-use child: %d", childno);

  /* Copy the parent's fproc struct to the child. */
  /* However, the mutex variables belong to a slot and must stay the same. */
  c_fp_lock = fproc[childno].fp_lock;
  fproc[childno] = fproc[parentno];
  fproc[childno].fp_lock = c_fp_lock;

  /* Increase the counters in the 'filp' table. */
  cp = &fproc[childno];
  pp = &fproc[parentno];

  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NULL) cp->fp_filp[i]->filp_count++;

  /* Fill in new process and endpoint id. */
  cp->fp_pid = cpid;
  cp->fp_endpoint = cproc;

  /* A forking process never has an outstanding grant, as it isn't blocking on
   * I/O. */
  if (GRANT_VALID(pp->fp_grant)) {
	panic("VFS: fork: pp (endpoint %d) has grant %d\n", pp->fp_endpoint,
	       pp->fp_grant);
  }
  if (GRANT_VALID(cp->fp_grant)) {
	panic("VFS: fork: cp (endpoint %d) has grant %d\n", cp->fp_endpoint,
	       cp->fp_grant);
  }

  /* A child is not a process leader, not being revived, etc. */
  cp->fp_flags = FP_NOFLAGS;

  /* Record the fact that both root and working dir have another user. */
  if (cp->fp_rd) dup_vnode(cp->fp_rd);
  if (cp->fp_wd) dup_vnode(cp->fp_wd);
}

/*===========================================================================*
 *				free_proc				     *
 *===========================================================================*/
static void free_proc(struct fproc *exiter, int flags)
{
  int i;
  register struct fproc *rfp;
  register struct filp *rfilp;
  register struct vnode *vp;
  dev_t dev;

  if (exiter->fp_endpoint == NONE)
	panic("free_proc: already free");

  if (fp_is_blocked(exiter))
	unpause(exiter->fp_endpoint);

  /* Loop on file descriptors, closing any that are open. */
  for (i = 0; i < OPEN_MAX; i++) {
	(void) close_fd(exiter, i);
  }

  /* Release root and working directories. */
  if (exiter->fp_rd) { put_vnode(exiter->fp_rd); exiter->fp_rd = NULL; }
  if (exiter->fp_wd) { put_vnode(exiter->fp_wd); exiter->fp_wd = NULL; }

  /* The rest of these actions is only done when processes actually exit. */
  if (!(flags & FP_EXITING)) return;

  exiter->fp_flags |= FP_EXITING;

  /* Check if any process is SUSPENDed on this driver.
   * If a driver exits, unmap its entries in the dmap table.
   * (unmapping has to be done after the first step, because the
   * dmap table is used in the first step.)
   */
  unsuspend_by_endpt(exiter->fp_endpoint);
  dmap_unmap_by_endpt(exiter->fp_endpoint);

  worker_stop_by_endpt(exiter->fp_endpoint); /* Unblock waiting threads */
  vmnt_unmap_by_endpt(exiter->fp_endpoint); /* Invalidate open files if this
					     * was an active FS */

  /* Invalidate endpoint number for error and sanity checks. */
  exiter->fp_endpoint = NONE;

  /* If a session leader exits and it has a controlling tty, then revoke
   * access to its controlling tty from all other processes using it.
   */
  if ((exiter->fp_flags & FP_SESLDR) && exiter->fp_tty != 0) {
      dev = exiter->fp_tty;
      for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	  if(rfp->fp_pid == PID_FREE) continue;
          if (rfp->fp_tty == dev) rfp->fp_tty = 0;

          for (i = 0; i < OPEN_MAX; i++) {
		if ((rfilp = rfp->fp_filp[i]) == NULL) continue;
		if (rfilp->filp_mode == FILP_CLOSED) continue;
		vp = rfilp->filp_vno;
		if (!S_ISCHR(vp->v_mode)) continue;
		if ((dev_t) vp->v_sdev != dev) continue;
		lock_filp(rfilp, VNODE_READ);
		(void) dev_close(dev, rfilp-filp); /* Ignore any errors, even
						    * SUSPEND. */

		rfilp->filp_mode = FILP_CLOSED;
		unlock_filp(rfilp);
          }
      }
  }

  /* Exit done. Mark slot as free. */
  exiter->fp_pid = PID_FREE;
  if (exiter->fp_flags & FP_PENDING)
	pending--;	/* No longer pending job, not going to do it */
  exiter->fp_flags = FP_NOFLAGS;
}

/*===========================================================================*
 *				pm_exit					     *
 *===========================================================================*/
void pm_exit(proc)
endpoint_t proc;
{
/* Perform the file system portion of the exit(status) system call. */
  int exitee_p;

  /* Nevertheless, pretend that the call came from the user. */
  okendpt(proc, &exitee_p);
  fp = &fproc[exitee_p];
  free_proc(fp, FP_EXITING);
}

/*===========================================================================*
 *				pm_setgid				     *
 *===========================================================================*/
void pm_setgid(proc_e, egid, rgid)
endpoint_t proc_e;
int egid;
int rgid;
{
  register struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effgid =  egid;
  tfp->fp_realgid = rgid;
}


/*===========================================================================*
 *				pm_setgroups				     *
 *===========================================================================*/
void pm_setgroups(proc_e, ngroups, groups)
endpoint_t proc_e;
int ngroups;
gid_t *groups;
{
  struct fproc *rfp;
  int slot;

  okendpt(proc_e, &slot);
  rfp = &fproc[slot];
  if (ngroups * sizeof(gid_t) > sizeof(rfp->fp_sgroups))
	panic("VFS: pm_setgroups: too much data to copy");
  if (sys_datacopy(who_e, (vir_bytes) groups, SELF, (vir_bytes) rfp->fp_sgroups,
		   ngroups * sizeof(gid_t)) == OK) {
	rfp->fp_ngroups = ngroups;
  } else
	panic("VFS: pm_setgroups: datacopy failed");
}


/*===========================================================================*
 *				pm_setuid				     *
 *===========================================================================*/
void pm_setuid(proc_e, euid, ruid)
endpoint_t proc_e;
int euid;
int ruid;
{
  struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effuid =  euid;
  tfp->fp_realuid = ruid;
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
int do_svrctl(message *UNUSED(m_out))
{
  unsigned int svrctl;
  vir_bytes ptr;

  svrctl = job_m_in.svrctl_req;
  ptr = (vir_bytes) job_m_in.svrctl_argp;
  if (((svrctl >> 8) & 0xFF) != 'M') return(EINVAL);

  switch (svrctl) {
    case VFSSETPARAM:
    case VFSGETPARAM:
	{
		struct sysgetenv sysgetenv;
		char search_key[64];
		char val[64];
		int r, s;

		/* Copy sysgetenv structure to VFS */
		if (sys_datacopy(who_e, ptr, SELF, (vir_bytes) &sysgetenv,
				 sizeof(sysgetenv)) != OK)
			return(EFAULT);

		/* Basic sanity checking */
		if (svrctl == VFSSETPARAM) {
			if (sysgetenv.keylen <= 0 ||
			    sysgetenv.keylen > (sizeof(search_key) - 1) ||
			    sysgetenv.vallen <= 0 ||
			    sysgetenv.vallen >= sizeof(val)) {
				return(EINVAL);
			}
		}

		/* Copy parameter "key" */
		if ((s = sys_datacopy(who_e, (vir_bytes) sysgetenv.key,
				      SELF, (vir_bytes) search_key,
				      sysgetenv.keylen)) != OK)
			return(s);
		search_key[sysgetenv.keylen] = '\0'; /* Limit string */

		/* Is it a parameter we know? */
		if (svrctl == VFSSETPARAM) {
			if (!strcmp(search_key, "verbose")) {
				int verbose_val;
				if ((s = sys_datacopy(who_e,
				    (vir_bytes) sysgetenv.val, SELF,
				    (vir_bytes) &val, sysgetenv.vallen)) != OK)
					return(s);
				val[sysgetenv.vallen] = '\0'; /* Limit string */
				verbose_val = atoi(val);
				if (verbose_val < 0 || verbose_val > 4) {
					return(EINVAL);
				}
				verbose = verbose_val;
				r = OK;
			} else {
				r = ESRCH;
			}
		} else { /* VFSGETPARAM */
			char small_buf[60];

			r = ESRCH;
			if (!strcmp(search_key, "print_traces")) {
				mthread_stacktraces();
				sysgetenv.val = 0;
				sysgetenv.vallen = 0;
				r = OK;
			} else if (!strcmp(search_key, "active_threads")) {
				int active = NR_WTHREADS - worker_available();
				snprintf(small_buf, sizeof(small_buf) - 1,
					 "%d", active);
				sysgetenv.vallen = strlen(small_buf);
				r = OK;
			}

			if (r == OK) {
				if ((s = sys_datacopy(SELF,
				    (vir_bytes) &sysgetenv, who_e, ptr,
				    sizeof(sysgetenv))) != OK)
					return(s);
				if (sysgetenv.val != 0) {
					if ((s = sys_datacopy(SELF,
					    (vir_bytes) small_buf, who_e,
					    (vir_bytes) sysgetenv.val,
					    sysgetenv.vallen)) != OK)
						return(s);
				}
			}
		}

		return(r);
	}
    default:
	return(EINVAL);
  }
}

/*===========================================================================*
 *				pm_dumpcore				     *
 *===========================================================================*/
int pm_dumpcore(endpoint_t proc_e, int csig, vir_bytes exe_name)
{
  int slot, r = OK, core_fd;
  struct filp *f;
  char core_path[PATH_MAX];
  char proc_name[PROC_NAME_LEN];

  okendpt(proc_e, &slot);
  fp = &fproc[slot];

  /* if a process is blocked, scratch(fp).file.fd_nr holds the fd it's blocked
   * on. free it up for use by common_open().
   */
  if (fp_is_blocked(fp))
          unpause(fp->fp_endpoint);

  /* open core file */
  snprintf(core_path, PATH_MAX, "%s.%d", CORE_NAME, fp->fp_pid);
  core_fd = common_open(core_path, O_WRONLY | O_CREAT | O_TRUNC, CORE_MODE);
  if (core_fd < 0) { r = core_fd; goto core_exit; }

  /* get process' name */
  r = sys_datacopy(PM_PROC_NR, exe_name, VFS_PROC_NR, (vir_bytes) proc_name,
			PROC_NAME_LEN);
  if (r != OK) goto core_exit;
  proc_name[PROC_NAME_LEN - 1] = '\0';

  if ((f = get_filp(core_fd, VNODE_WRITE)) == NULL) { r=EBADF; goto core_exit; }
  write_elf_core_file(f, csig, proc_name);
  unlock_filp(f);
  (void) close_fd(fp, core_fd);	        /* ignore failure, we're exiting anyway */

core_exit:
  if(csig)
	  free_proc(fp, FP_EXITING);
  return(r);
}

/*===========================================================================*
 *				 ds_event				     *
 *===========================================================================*/
void *
ds_event(void *arg)
{
  char key[DS_MAX_KEYLEN];
  char *blkdrv_prefix = "drv.blk.";
  char *chrdrv_prefix = "drv.chr.";
  u32_t value;
  int type, r, is_blk;
  endpoint_t owner_endpoint;

  struct job my_job;

  my_job = *((struct job *) arg);
  fp = my_job.j_fp;

  /* Get the event and the owner from DS. */
  while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {
	/* Only check for block and character driver up events. */
	if (!strncmp(key, blkdrv_prefix, strlen(blkdrv_prefix))) {
		is_blk = TRUE;
	} else if (!strncmp(key, chrdrv_prefix, strlen(chrdrv_prefix))) {
		is_blk = FALSE;
	} else {
		continue;
	}

	if ((r = ds_retrieve_u32(key, &value)) != OK) {
		printf("VFS: ds_event: ds_retrieve_u32 failed\n");
		break;
	}
	if (value != DS_DRIVER_UP) continue;

	/* Perform up. */
	dmap_endpt_up(owner_endpoint, is_blk);
  }

  if (r != ENOENT) printf("VFS: ds_event: ds_check failed: %d\n", r);

  thread_cleanup(NULL);
  return(NULL);
}

/* A function to be called on panic(). */
void panic_hook(void)   
{               
  printf("VFS mthread stacktraces:\n");
  mthread_stacktraces(); 
}         

/*===========================================================================*
 *				do_getrusage				     *
 *===========================================================================*/
int do_getrusage(message *UNUSED(m_out))
{
	int res;
	struct rusage r_usage;

	if ((res = sys_datacopy(who_e, (vir_bytes) m_in.RU_RUSAGE_ADDR, SELF,
		(vir_bytes) &r_usage, (vir_bytes) sizeof(r_usage))) < 0)
		return res;

	r_usage.ru_inblock = 0;
	r_usage.ru_oublock = 0;
	r_usage.ru_ixrss = fp->text_size;
	r_usage.ru_idrss = fp->data_size;
	r_usage.ru_isrss = DEFAULT_STACK_LIMIT;

	return sys_datacopy(SELF, (vir_bytes) &r_usage, who_e,
		(vir_bytes) m_in.RU_RUSAGE_ADDR, (phys_bytes) sizeof(r_usage));
}
