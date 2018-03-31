/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_fcntl:	  perform the FCNTL system call
 *   do_sync:	  perform the SYNC system call
 *   do_fsync:	  perform the FSYNC system call
 *   pm_setsid:	  perform VFS's side of setsid system call
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
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"

#define CORE_NAME	"core"
#define CORE_MODE	0777	/* mode to use on core image files */

#if ENABLE_SYSCALL_STATS
unsigned long calls_stats[NR_VFS_CALLS];
#endif

static void free_proc(int flags);

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
int do_getsysinfo(void)
{
  struct fproc *rfp;
  struct fproc_light *rfpl;
  struct smap *sp;
  vir_bytes src_addr, dst_addr;
  size_t len, buf_size;
  int what;

  what = job_m_in.m_lsys_getsysinfo.what;
  dst_addr = job_m_in.m_lsys_getsysinfo.where;
  buf_size = job_m_in.m_lsys_getsysinfo.size;

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
    case SI_PROCLIGHT_TAB:
	/* Fill the light process table for the MIB service upon request. */
	rfpl = &fproc_light[0];
	for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++, rfpl++) {
		rfpl->fpl_tty = rfp->fp_tty;
		rfpl->fpl_blocked_on = rfp->fp_blocked_on;
		if (rfp->fp_blocked_on == FP_BLOCKED_ON_CDEV)
			rfpl->fpl_task = rfp->fp_cdev.endpt;
		else if (rfp->fp_blocked_on == FP_BLOCKED_ON_SDEV &&
		    (sp = get_smap_by_dev(rfp->fp_sdev.dev, NULL)) != NULL)
			rfpl->fpl_task = sp->smap_endpt;
		else
			rfpl->fpl_task = NONE;
	}
	src_addr = (vir_bytes) fproc_light;
	len = sizeof(fproc_light);
	break;
#if ENABLE_SYSCALL_STATS
    case SI_CALL_STATS:
	src_addr = (vir_bytes) calls_stats;
	len = sizeof(calls_stats);
	break;
#endif
    default:
	return(EINVAL);
  }

  if (len != buf_size)
	return(EINVAL);

  return sys_datacopy_wrapper(SELF, src_addr, who_e, dst_addr, len);
}

/*===========================================================================*
 *				do_fcntl				     *
 *===========================================================================*/
int do_fcntl(void)
{
/* Perform the fcntl(fd, cmd, ...) system call. */
  struct filp *f;
  int fd, new_fd, fl, r = OK, fcntl_req, fcntl_argx;
  vir_bytes addr;
  tll_access_t locktype;

  fd = job_m_in.m_lc_vfs_fcntl.fd;
  fcntl_req = job_m_in.m_lc_vfs_fcntl.cmd;
  fcntl_argx = job_m_in.m_lc_vfs_fcntl.arg_int;
  addr = job_m_in.m_lc_vfs_fcntl.arg_ptr;

  /* Is the file descriptor valid? */
  locktype = (fcntl_req == F_FREESP) ? VNODE_WRITE : VNODE_READ;
  if ((f = get_filp(fd, locktype)) == NULL)
	return(err_code);

  switch (fcntl_req) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
	/* This replaces the old dup() system call. */
	if (fcntl_argx < 0 || fcntl_argx >= OPEN_MAX) r = EINVAL;
	else if ((r = get_fd(fp, fcntl_argx, 0, &new_fd, NULL)) == OK) {
		f->filp_count++;
		fp->fp_filp[new_fd] = f;
		assert(!FD_ISSET(new_fd, &fp->fp_cloexec_set));
		if (fcntl_req == F_DUPFD_CLOEXEC)
			FD_SET(new_fd, &fp->fp_cloexec_set);
		r = new_fd;
	}
	break;

    case F_GETFD:
	/* Get close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	r = 0;
	if (FD_ISSET(fd, &fp->fp_cloexec_set))
		r = FD_CLOEXEC;
	break;

    case F_SETFD:
	/* Set close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	if (fcntl_argx & FD_CLOEXEC)
		FD_SET(fd, &fp->fp_cloexec_set);
	else
		FD_CLR(fd, &fp->fp_cloexec_set);
	break;

    case F_GETFL:
	/* Get file status flags (O_NONBLOCK and O_APPEND). */
	fl = f->filp_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
	r = fl;
	break;

    case F_SETFL:
	/* Set file status flags (O_NONBLOCK and O_APPEND). */
	fl = O_NONBLOCK | O_APPEND;
	f->filp_flags = (f->filp_flags & ~fl) | (fcntl_argx & fl);
	break;

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
	/* Set or clear a file lock. */
	r = lock_op(fd, fcntl_req, addr);
	break;

    case F_FREESP:
     {
	/* Free a section of a file */
	off_t start, end, offset;
	struct flock flock_arg;

	/* Check if it's a regular file. */
	if (!S_ISREG(f->filp_vno->v_mode)) r = EINVAL;
	else if (!(f->filp_mode & W_BIT)) r = EBADF;
	else {
		/* Copy flock data from userspace. */
		r = sys_datacopy_wrapper(who_e, addr, SELF,
		    (vir_bytes)&flock_arg, sizeof(flock_arg));
	}

	if (r != OK) break;

	/* Convert starting offset to signed. */
	offset = (off_t) flock_arg.l_start;

	/* Figure out starting position base. */
	switch(flock_arg.l_whence) {
	  case SEEK_SET: start = 0; break;
	  case SEEK_CUR: start = f->filp_pos; break;
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
	r = !!(f->filp_flags & O_NOSIGPIPE);
	break;
    case F_SETNOSIGPIPE:
	if (fcntl_argx)
		f->filp_flags |= O_NOSIGPIPE;
	else
		f->filp_flags &= ~O_NOSIGPIPE;
	break;
    case F_FLUSH_FS_CACHE:
    {
	struct vnode *vn = f->filp_vno;
	mode_t mode = f->filp_vno->v_mode;
	if (!super_user) {
		r = EPERM;
	} else if (S_ISBLK(mode)) {
		/* Block device; flush corresponding device blocks. */
		r = req_flush(vn->v_bfs_e, vn->v_sdev);
	} else if (S_ISREG(mode) || S_ISDIR(mode)) {
		/* Directory or regular file; flush hosting FS blocks. */
		r = req_flush(vn->v_fs_e, vn->v_dev);
	} else {
		/* Remaining cases.. Meaning unclear. */
		r = ENODEV;
	}
	break;
    }
    default:
	r = EINVAL;
  }

  unlock_filp(f);
  return(r);
}

/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
int do_sync(void)
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
 *				do_fsync				     *
 *===========================================================================*/
int do_fsync(void)
{
/* Perform the fsync() system call. */
  struct filp *rfilp;
  struct vmnt *vmp;
  dev_t dev;
  int fd, r = OK;

  fd = job_m_in.m_lc_vfs_fsync.fd;

  if ((rfilp = get_filp(fd, VNODE_READ)) == NULL)
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
	struct fproc *vmf = fproc_addr(VM_PROC_NR);

	*newfilp = NULL;

	if ((f = get_filp2(rfp, pfd, VNODE_READ)) == NULL) {
		printf("VFS dupvm: get_filp2 failed\n");
		return EBADF;
	}

	if(!(f->filp_vno->v_vmnt->m_fs_flags & RES_HASPEEK)) {
		unlock_filp(f);
#if 0	/* Noisy diagnostic for mmap() by ld.so */
		printf("VFS dupvm: no peek available\n");
#endif
		return EINVAL;
	}

	assert(f->filp_vno);
	assert(f->filp_vno->v_vmnt);

	if (!S_ISREG(f->filp_vno->v_mode) && !S_ISBLK(f->filp_vno->v_mode)) {
		printf("VFS: mmap regular/blockdev only; dev 0x%llx ino %llu has mode 0%o\n",
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

	*newfilp = f;

	return OK;
}

/*===========================================================================*
 *				do_vm_call				     *
 *===========================================================================*/
int do_vm_call(void)
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
	u64_t offset = job_m_in.VFS_VMCALL_OFFSET;
	u32_t length = job_m_in.VFS_VMCALL_LENGTH;
	int result = OK;
	int slot;
	struct fproc *rfp;
#if !defined(NDEBUG)
	struct fproc *vmf;
#endif /* !defined(NDEBUG) */
	struct filp *f = NULL;
	int r;

	if(job_m_in.m_source != VM_PROC_NR)
		return ENOSYS;

	if(isokendpt(ep, &slot) != OK) rfp = NULL;
	else rfp = &fproc[slot];

#if !defined(NDEBUG)
	vmf = fproc_addr(VM_PROC_NR);
#endif /* !defined(NDEBUG) */
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
#if 0   /* Noisy diagnostic for mmap() by ld.so */
				printf("vfs: dupvm failed\n");
#endif
				goto reqdone;
			}

			if(S_ISBLK(f->filp_vno->v_mode)) {
				assert(f->filp_vno->v_sdev != NO_DEV);
				job_m_out.VMV_DEV = f->filp_vno->v_sdev;
				job_m_out.VMV_INO = VMC_NO_INODE;
				job_m_out.VMV_SIZE_PAGES = LONG_MAX;
			} else {
				job_m_out.VMV_DEV = f->filp_vno->v_dev;
				job_m_out.VMV_INO = f->filp_vno->v_inode_nr;
				job_m_out.VMV_SIZE_PAGES =
					roundup(f->filp_vno->v_size,
						PAGE_SIZE)/PAGE_SIZE;
			}

			job_m_out.VMV_FD = procfd;

			result = OK;

			break;
		}
		case VMVFSREQ_FDCLOSE:
		{
			result = close_fd(fp, req_fd, FALSE /*may_suspend*/);
			if(result != OK) {
				printf("VFS: VM fd close for fd %d, %d (%d)\n",
					req_fd, fp->fp_endpoint, result);
			}
			break;
		}
		case VMVFSREQ_FDIO:
		{
			result = actual_lseek(fp, req_fd, SEEK_SET, offset,
				NULL);

			if(result == OK) {
				result = actual_read_write_peek(fp, PEEKING,
					req_fd, /* vir_bytes */ 0, length);
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
	job_m_out.VMV_ENDPOINT = ep;
	job_m_out.VMV_RESULT = result;
	job_m_out.VMV_REQID = req_id;

	/* Reply asynchronously as VM may not be able to receive
	 * an ipc_sendnb() message.
	 */
	job_m_out.m_type = VM_VFS_REPLY;
	r = asynsend3(VM_PROC_NR, &job_m_out, 0);
	if(r != OK) printf("VFS: couldn't asynsend3() to VM\n");

	/* VFS does not reply any further */
	return SUSPEND;
}

/*===========================================================================*
 *				pm_reboot				     *
 *===========================================================================*/
void
pm_reboot(void)
{
/* Perform the VFS side of the reboot call. This call is performed from the PM
 * process context.
 */
  message m_out;
  int i, r;
  struct fproc *rfp, *pmfp;

  pmfp = fp;

  do_sync();

  /* Do exit processing for all leftover processes and servers, but don't
   * actually exit them (if they were really gone, PM will tell us about it).
   * Skip processes that handle parts of the file system; we first need to give
   * them the chance to unmount (which should be possible as all normal
   * processes have no open files anymore).
   */
  /* This is the only place where we allow special modification of "fp". The
   * reboot procedure should really be implemented as a PM message broadcasted
   * to all processes, so that each process will be shut down cleanly by a
   * thread operating on its behalf. Doing everything here is simpler, but it
   * requires an exception to the strict model of having "fp" be the process
   * that owns the current worker thread.
   */
  for (i = 0; i < NR_PROCS; i++) {
	rfp = &fproc[i];

	/* Don't just free the proc right away, but let it finish what it was
	 * doing first */
	if (rfp != fp) lock_proc(rfp);
	if (rfp->fp_endpoint != NONE && find_vmnt(rfp->fp_endpoint) == NULL) {
		worker_set_proc(rfp);	/* temporarily fake process context */
		free_proc(0);
		worker_set_proc(pmfp);	/* restore original process context */
	}
	if (rfp != fp) unlock_proc(rfp);
  }

  do_sync();
  unmount_all(0 /* Don't force */);

  /* Try to exit all processes again including File Servers */
  for (i = 0; i < NR_PROCS; i++) {
	rfp = &fproc[i];

	/* Don't just free the proc right away, but let it finish what it was
	 * doing first */
	if (rfp != fp) lock_proc(rfp);
	if (rfp->fp_endpoint != NONE) {
		worker_set_proc(rfp);	/* temporarily fake process context */
		free_proc(0);
		worker_set_proc(pmfp);	/* restore original process context */
	}
	if (rfp != fp) unlock_proc(rfp);
  }

  do_sync();
  unmount_all(1 /* Force */);

  /* Reply to PM for synchronization */
  memset(&m_out, 0, sizeof(m_out));

  m_out.m_type = VFS_PM_REBOOT_REPLY;

  if ((r = ipc_send(PM_PROC_NR, &m_out)) != OK)
	panic("pm_reboot: ipc_send failed: %d", r);
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
  struct fproc *cp;
#if !defined(NDEBUG)
  struct fproc *pp;
#endif /* !defined(NDEBUG) */
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
#if !defined(NDEBUG)
  pp = &fproc[parentno];
#endif /* !defined(NDEBUG) */

  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NULL) cp->fp_filp[i]->filp_count++;

  /* Fill in new process and endpoint id. */
  cp->fp_pid = cpid;
  cp->fp_endpoint = cproc;

#if !defined(NDEBUG)
  /* A forking process cannot possibly be suspended on anything. */
  assert(pp->fp_blocked_on == FP_BLOCKED_ON_NONE);
#endif /* !defined(NDEBUG) */

  /* A child is not a process leader, not being revived, etc. */
  cp->fp_flags = FP_NOFLAGS;

  /* Record the fact that both root and working dir have another user. */
  if (cp->fp_rd) dup_vnode(cp->fp_rd);
  if (cp->fp_wd) dup_vnode(cp->fp_wd);
}

/*===========================================================================*
 *				free_proc				     *
 *===========================================================================*/
static void free_proc(int flags)
{
  int i;
  register struct fproc *rfp;
  register struct filp *rfilp;
  register struct vnode *vp;
  dev_t dev;

  if (fp->fp_endpoint == NONE)
	panic("free_proc: already free");

  if (fp_is_blocked(fp))
	unpause();

  /* Loop on file descriptors, closing any that are open. */
  for (i = 0; i < OPEN_MAX; i++) {
	(void) close_fd(fp, i, FALSE /*may_suspend*/);
  }

  /* Release root and working directories. */
  if (fp->fp_rd) { put_vnode(fp->fp_rd); fp->fp_rd = NULL; }
  if (fp->fp_wd) { put_vnode(fp->fp_wd); fp->fp_wd = NULL; }

  /* The rest of these actions is only done when processes actually exit. */
  if (!(flags & FP_EXITING)) return;

  fp->fp_flags |= FP_EXITING;

  /* Check if any process is SUSPENDed on this driver.
   * If a driver exits, unmap its entries in the dmap table.
   * (unmapping has to be done after the first step, because the
   * dmap/smap tables are used in the first step.)
   */
  unsuspend_by_endpt(fp->fp_endpoint);
  dmap_unmap_by_endpt(fp->fp_endpoint);
  smap_unmap_by_endpt(fp->fp_endpoint);

  worker_stop_by_endpt(fp->fp_endpoint); /* Unblock waiting threads */
  vmnt_unmap_by_endpt(fp->fp_endpoint); /* Invalidate open files if this
					     * was an active FS */

  /* If a session leader exits and it has a controlling tty, then revoke
   * access to its controlling tty from all other processes using it.
   */
  if ((fp->fp_flags & FP_SESLDR) && fp->fp_tty != 0) {
      dev = fp->fp_tty;
      for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	  if(rfp->fp_pid == PID_FREE) continue;
          if (rfp->fp_tty == dev) rfp->fp_tty = 0;

          for (i = 0; i < OPEN_MAX; i++) {
		if ((rfilp = rfp->fp_filp[i]) == NULL) continue;
		if (rfilp->filp_mode == FILP_CLOSED) continue;
		vp = rfilp->filp_vno;
		if (!S_ISCHR(vp->v_mode)) continue;
		if (vp->v_sdev != dev) continue;
		lock_filp(rfilp, VNODE_READ);
		(void) cdev_close(dev); /* Ignore any errors. */
		/* FIXME: missing select check */
		rfilp->filp_mode = FILP_CLOSED;
		unlock_filp(rfilp);
          }
      }
  }

  /* Exit done. Mark slot as free. */
  fp->fp_endpoint = NONE;
  fp->fp_pid = PID_FREE;
  fp->fp_flags = FP_NOFLAGS;
}

/*===========================================================================*
 *				pm_exit					     *
 *===========================================================================*/
void pm_exit(void)
{
/* Perform the file system portion of the exit(status) system call.
 * This function is called from the context of the exiting process.
 */

  free_proc(FP_EXITING);
}

/*===========================================================================*
 *				pm_setgid				     *
 *===========================================================================*/
void
pm_setgid(endpoint_t proc_e, int egid, int rgid)
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
void
pm_setgroups(endpoint_t proc_e, int ngroups, gid_t *groups)
{
  struct fproc *rfp;
  int slot;

  okendpt(proc_e, &slot);
  rfp = &fproc[slot];
  if (ngroups * sizeof(gid_t) > sizeof(rfp->fp_sgroups))
	panic("VFS: pm_setgroups: too much data to copy");
  if (sys_datacopy_wrapper(who_e, (vir_bytes) groups, SELF, (vir_bytes) rfp->fp_sgroups,
		   ngroups * sizeof(gid_t)) == OK) {
	rfp->fp_ngroups = ngroups;
  } else
	panic("VFS: pm_setgroups: datacopy failed");
}


/*===========================================================================*
 *				pm_setuid				     *
 *===========================================================================*/
void
pm_setuid(endpoint_t proc_e, int euid, int ruid)
{
  struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effuid =  euid;
  tfp->fp_realuid = ruid;
}

/*===========================================================================*
 *				pm_setsid				     *
 *===========================================================================*/
void pm_setsid(endpoint_t proc_e)
{
/* Perform the VFS side of the SETSID call, i.e. get rid of the controlling
 * terminal of a process, and make the process a session leader.
 */
  struct fproc *rfp;
  int slot;

  /* Make the process a session leader with no controlling tty. */
  okendpt(proc_e, &slot);
  rfp = &fproc[slot];
  rfp->fp_flags |= FP_SESLDR;
  rfp->fp_tty = 0;
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
int do_svrctl(void)
{
  unsigned long svrctl;
  vir_bytes ptr;

  svrctl = job_m_in.m_lc_svrctl.request;
  ptr = job_m_in.m_lc_svrctl.arg;

  if (IOCGROUP(svrctl) != 'F') return(EINVAL);

  switch (svrctl) {
    case VFSSETPARAM:
    case VFSGETPARAM:
	{
		struct sysgetenv sysgetenv;
		char search_key[64];
		char val[64];
		int r, s;

		/* Copy sysgetenv structure to VFS */
		if (sys_datacopy_wrapper(who_e, ptr, SELF, (vir_bytes) &sysgetenv,
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
		if ((s = sys_datacopy_wrapper(who_e, (vir_bytes) sysgetenv.key,
				      SELF, (vir_bytes) search_key,
				      sysgetenv.keylen)) != OK)
			return(s);
		search_key[sysgetenv.keylen] = '\0'; /* Limit string */

		/* Is it a parameter we know? */
		if (svrctl == VFSSETPARAM) {
			if (!strcmp(search_key, "verbose")) {
				int verbose_val;
				if ((s = sys_datacopy_wrapper(who_e,
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
			} else if (!strcmp(search_key, "print_select")) {
				select_dump();
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
				if ((s = sys_datacopy_wrapper(SELF,
				    (vir_bytes) &sysgetenv, who_e, ptr,
				    sizeof(sysgetenv))) != OK)
					return(s);
				if (sysgetenv.val != 0) {
					if ((s = sys_datacopy_wrapper(SELF,
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
int pm_dumpcore(int csig, vir_bytes exe_name)
{
  int r, core_fd;
  struct filp *f;
  char core_path[PATH_MAX];
  char proc_name[PROC_NAME_LEN];

  /* In effect, the coredump is generated through the use of calls as if made
   * by the process itself.  As such, the process must not be doing anything
   * else.  Therefore, if the process was blocked on anything, unblock it
   * first.  This step is the reason we cannot use this function to generate a
   * core dump of a process while it is still running (i.e., without
   * terminating it), as it changes the state of the process.
   */
  if (fp_is_blocked(fp))
          unpause();

  /* open core file */
  snprintf(core_path, PATH_MAX, "%s.%d", CORE_NAME, fp->fp_pid);
  r = core_fd = common_open(core_path, O_WRONLY | O_CREAT | O_TRUNC,
	CORE_MODE, FALSE /*for_exec*/);
  if (r < 0) goto core_exit;

  /* get process name */
  r = sys_datacopy_wrapper(PM_PROC_NR, exe_name, VFS_PROC_NR,
	(vir_bytes) proc_name, PROC_NAME_LEN);
  if (r != OK) goto core_exit;
  proc_name[PROC_NAME_LEN - 1] = '\0';

  /* write the core dump */
  f = get_filp(core_fd, VNODE_WRITE);
  assert(f != NULL);
  write_elf_core_file(f, csig, proc_name);
  unlock_filp(f);

core_exit:
  /* The core file descriptor will be closed as part of the process exit. */
  free_proc(FP_EXITING);

  return(r);
}

/*===========================================================================*
 *				 ds_event				     *
 *===========================================================================*/
void
ds_event(void)
{
  char key[DS_MAX_KEYLEN];
  char *blkdrv_prefix = "drv.blk.";
  char *chrdrv_prefix = "drv.chr.";
  char *sckdrv_prefix = "drv.sck.";
  u32_t value;
  int type, ftype, r;
  endpoint_t owner_endpoint;

  /* Get the event and the owner from DS. */
  while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {
	/* Only check for block, character, socket driver up events. */
	if (!strncmp(key, blkdrv_prefix, strlen(blkdrv_prefix))) {
		ftype = S_IFBLK;
	} else if (!strncmp(key, chrdrv_prefix, strlen(chrdrv_prefix))) {
		ftype = S_IFCHR;
	} else if (!strncmp(key, sckdrv_prefix, strlen(sckdrv_prefix))) {
		ftype = S_IFSOCK;
	} else {
		continue;
	}

	if ((r = ds_retrieve_u32(key, &value)) != OK) {
		printf("VFS: ds_event: ds_retrieve_u32 failed\n");
		break;
	}
	if (value != DS_DRIVER_UP) continue;

	/* Perform up. */
	if (ftype == S_IFBLK || ftype == S_IFCHR)
		dmap_endpt_up(owner_endpoint, (ftype == S_IFBLK));
	else
		smap_endpt_up(owner_endpoint);
  }

  if (r != ENOENT) printf("VFS: ds_event: ds_check failed: %d\n", r);
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
int do_getrusage(void)
{
	/* Obsolete vfs_getrusage(2) call from userland. The getrusage call is
	 * now fully handled by PM, and for any future fields that should be
	 * supplied by VFS, VFS should be queried by PM rather than by the user
	 * program directly.  TODO: remove this call after the next release.
	 */
	return OK;
}
