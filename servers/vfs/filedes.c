/* This file contains the procedures that manipulate file descriptors.
 *
 * The entry points into this file are
 *   get_fd:	    look for free file descriptor and free filp slots
 *   get_filp:	    look up the filp entry for a given file descriptor
 *   find_filp:	    find a filp slot that points to a given vnode
 *   inval_filp:    invalidate a filp and associated fd's, only let close()
 *                  happen on it
 *   do_verify_fd:  verify whether the given file descriptor is valid for
 *                  the given endpoint.
 *   do_set_filp:   marks a filp as in-flight.
 *   do_copy_filp:  copies a filp to another endpoint.
 *   do_put_filp:   marks a filp as not in-flight anymore.
 *   do_cancel_fd:  cancel the transaction when something goes wrong for
 *                  the receiver.
 *   do_dupfrom:    copies a filp from another endpoint.
 */

#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/u64.h>
#include <assert.h>
#include <sys/stat.h>
#include "fs.h"
#include "file.h"
#include "vnode.h"


static filp_id_t verify_fd(endpoint_t ep, int fd);

#if LOCK_DEBUG
/*===========================================================================*
 *				check_filp_locks			     *
 *===========================================================================*/
void check_filp_locks_by_me(void)
{
/* Check whether this thread still has filp locks held */
  struct filp *f;
  int r;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	r = mutex_trylock(&f->filp_lock);
	if (r == -EDEADLK)
		panic("Thread %d still holds filp lock on filp %p call_nr=%d\n",
		      mthread_self(), f, job_call_nr);
	else if (r == 0) {
		/* We just obtained the lock, release it */
		mutex_unlock(&f->filp_lock);
	}
  }
}
#endif

/*===========================================================================*
 *				check_filp_locks			     *
 *===========================================================================*/
void check_filp_locks(void)
{
  struct filp *f;
  int r, count = 0;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	r = mutex_trylock(&f->filp_lock);
	if (r == -EBUSY) {
		/* Mutex is still locked */
		count++;
	} else if (r == 0) {
		/* We just obtained a lock, don't want it */
		mutex_unlock(&f->filp_lock);
	} else
		panic("filp_lock weird state");
  }
  if (count) panic("locked filps");
#if 0
  else printf("check_filp_locks OK\n");
#endif
}

/*===========================================================================*
 *				init_filps				     *
 *===========================================================================*/
void init_filps(void)
{
/* Initialize filps */
  struct filp *f;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	if (mutex_init(&f->filp_lock, NULL) != 0)
		panic("Failed to initialize filp mutex");
  }

}

/*===========================================================================*
 *				get_fd					     *
 *===========================================================================*/
int get_fd(struct fproc *rfp, int start, mode_t bits, int *k, struct filp **fpt)
{
/* Look for a free file descriptor and a free filp slot.  Fill in the mode word
 * in the latter, but don't claim either one yet, since the open() or creat()
 * may yet fail.
 */

  register struct filp *f;
  register int i;

  /* Search the fproc fp_filp table for a free file descriptor. */
  for (i = start; i < OPEN_MAX; i++) {
	if (rfp->fp_filp[i] == NULL) {
		/* A file descriptor has been located. */
		*k = i;
		break;
	}
  }

  /* Check to see if a file descriptor has been found. */
  if (i >= OPEN_MAX) return(EMFILE);

  /* If we don't care about a filp, return now */
  if (fpt == NULL) return(OK);

  /* Now that a file descriptor has been found, look for a free filp slot. */
  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	assert(f->filp_count >= 0);
	if (f->filp_count == 0 && mutex_trylock(&f->filp_lock) == 0) {
		f->filp_mode = bits;
		f->filp_pos = 0;
		f->filp_selectors = 0;
		f->filp_select_ops = 0;
		f->filp_pipe_select_ops = 0;
		f->filp_flags = 0;
		f->filp_select_flags = 0;
		f->filp_softlock = NULL;
		*fpt = f;
		return(OK);
	}
  }

  /* If control passes here, the filp table must be full.  Report that back. */
  return(ENFILE);
}


/*===========================================================================*
 *				get_filp				     *
 *===========================================================================*/
struct filp *get_filp(fild, locktype)
int fild;			/* file descriptor */
tll_access_t locktype;
{
/* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */

  return get_filp2(fp, fild, locktype);
}


/*===========================================================================*
 *				get_filp2				     *
 *===========================================================================*/
struct filp *get_filp2(rfp, fild, locktype)
register struct fproc *rfp;
int fild;			/* file descriptor */
tll_access_t locktype;
{
/* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */
  struct filp *filp;

  filp = NULL;
  if (fild < 0 || fild >= OPEN_MAX)
	err_code = EBADF;
  else if (locktype != VNODE_OPCL && rfp->fp_filp[fild] != NULL &&
		rfp->fp_filp[fild]->filp_mode == FILP_CLOSED)
	err_code = EIO; /* disallow all use except close(2) */
  else if ((filp = rfp->fp_filp[fild]) == NULL)
	err_code = EBADF;
  else if (locktype != VNODE_NONE)	/* Only lock the filp if requested */
	lock_filp(filp, locktype);	/* All is fine */

  return(filp);	/* may also be NULL */
}


/*===========================================================================*
 *				find_filp				     *
 *===========================================================================*/
struct filp *find_filp(struct vnode *vp, mode_t bits)
{
/* Find a filp slot that refers to the vnode 'vp' in a way as described
 * by the mode bit 'bits'. Used for determining whether somebody is still
 * interested in either end of a pipe.  Also used when opening a FIFO to
 * find partners to share a filp field with (to shared the file position).
 * Like 'get_fd' it performs its job by linear search through the filp table.
 */

  struct filp *f;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	if (f->filp_count != 0 && f->filp_vno == vp && (f->filp_mode & bits)) {
		return(f);
	}
  }

  /* If control passes here, the filp wasn't there.  Report that back. */
  return(NULL);
}

/*===========================================================================*
 *				invalidate_filp				     *
 *===========================================================================*/
void invalidate_filp(struct filp *rfilp)
{
/* Invalidate filp. */

  rfilp->filp_mode = FILP_CLOSED;
}

/*===========================================================================*
 *			invalidate_filp_by_char_major			     *
 *===========================================================================*/
void invalidate_filp_by_char_major(int major)
{
  struct filp *f;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	if (f->filp_count != 0 && f->filp_vno != NULL) {
		if (major(f->filp_vno->v_sdev) == major &&
		    S_ISCHR(f->filp_vno->v_mode)) {
			invalidate_filp(f);
		}
	}
  }
}

/*===========================================================================*
 *			invalidate_filp_by_endpt			     *
 *===========================================================================*/
void invalidate_filp_by_endpt(endpoint_t proc_e)
{
  struct filp *f;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	if (f->filp_count != 0 && f->filp_vno != NULL) {
		if (f->filp_vno->v_fs_e == proc_e)
			invalidate_filp(f);
	}
  }
}

/*===========================================================================*
 *				lock_filp				     *
 *===========================================================================*/
void lock_filp(filp, locktype)
struct filp *filp;
tll_access_t locktype;
{
  struct worker_thread *org_self;
  struct vnode *vp;

  assert(filp->filp_count > 0);
  vp = filp->filp_vno;
  assert(vp != NULL);

  /* Lock vnode only if we haven't already locked it. If already locked by us,
   * we're allowed to have one additional 'soft' lock. */
  if (tll_locked_by_me(&vp->v_lock)) {
	assert(filp->filp_softlock == NULL);
	filp->filp_softlock = fp;
  } else {
	/* We have to make an exception for vnodes belonging to pipes. Even
	 * read(2) operations on pipes change the vnode and therefore require
	 * exclusive access.
	 */
	if (S_ISFIFO(vp->v_mode) && locktype == VNODE_READ)
		locktype = VNODE_WRITE;
	lock_vnode(vp, locktype);
  }

  assert(vp->v_ref_count > 0);	/* vnode still in use? */
  assert(filp->filp_vno == vp);	/* vnode still what we think it is? */

  /* First try to get filp lock right off the bat */
  if (mutex_trylock(&filp->filp_lock) != 0) {

	/* Already in use, let's wait for our turn */
	org_self = worker_suspend();

	if (mutex_lock(&filp->filp_lock) != 0)
		panic("unable to obtain lock on filp");

	worker_resume(org_self);
  }
}

/*===========================================================================*
 *				unlock_filp				     *
 *===========================================================================*/
void unlock_filp(filp)
struct filp *filp;
{
  /* If this filp holds a soft lock on the vnode, we must be the owner */
  if (filp->filp_softlock != NULL)
	assert(filp->filp_softlock == fp);

  if (filp->filp_count > 0) {
	/* Only unlock vnode if filp is still in use */

	/* and if we don't hold a soft lock */
	if (filp->filp_softlock == NULL) {
		assert(tll_islocked(&(filp->filp_vno->v_lock)));
		unlock_vnode(filp->filp_vno);
	}
  }

  filp->filp_softlock = NULL;
  if (mutex_unlock(&filp->filp_lock) != 0)
	panic("unable to release lock on filp");
}

/*===========================================================================*
 *				unlock_filps				     *
 *===========================================================================*/
void unlock_filps(filp1, filp2)
struct filp *filp1;
struct filp *filp2;
{
/* Unlock two filps that are tied to the same vnode. As a thread can lock a
 * vnode only once, unlocking the vnode twice would result in an error. */

  /* No NULL pointers and not equal */
  assert(filp1);
  assert(filp2);
  assert(filp1 != filp2);

  /* Must be tied to the same vnode and not NULL */
  assert(filp1->filp_vno == filp2->filp_vno);
  assert(filp1->filp_vno != NULL);

  if (filp1->filp_count > 0 && filp2->filp_count > 0) {
	/* Only unlock vnode if filps are still in use */
	unlock_vnode(filp1->filp_vno);
  }

  filp1->filp_softlock = NULL;
  filp2->filp_softlock = NULL;
  if (mutex_unlock(&filp2->filp_lock) != 0)
	panic("unable to release filp lock on filp2");
  if (mutex_unlock(&filp1->filp_lock) != 0)
	panic("unable to release filp lock on filp1");
}

/*===========================================================================*
 *				verify_fd				     *
 *===========================================================================*/
static filp_id_t verify_fd(ep, fd)
endpoint_t ep;
int fd;
{
/* Verify whether the file descriptor 'fd' is valid for the endpoint 'ep'. When
 * the file descriptor is valid, verify_fd returns a pointer to that filp, else
 * it returns NULL.
 */
  int slot;
  struct filp *rfilp;

  if (isokendpt(ep, &slot) != OK)
	return(NULL);

  rfilp = get_filp2(&fproc[slot], fd, VNODE_READ);

  return(rfilp);
}

/*===========================================================================*
 *                              do_verify_fd                                 *
 *===========================================================================*/
int do_verify_fd(message *m_out)
{
  struct filp *rfilp;
  endpoint_t proc_e;
  int fd;

  /* This should be replaced with an ACL check. */
  if (who_e != PFS_PROC_NR) return EPERM;

  proc_e = job_m_in.VFS_PFS_ENDPT;
  fd = job_m_in.VFS_PFS_FD;

  rfilp = (struct filp *) verify_fd(proc_e, fd);
  m_out->VFS_PFS_FILP = (void *) rfilp;
  if (rfilp != NULL) unlock_filp(rfilp);
  return (rfilp != NULL) ? OK : EINVAL;
}

/*===========================================================================*
 *                              set_filp                                     *
 *===========================================================================*/
int set_filp(sfilp)
filp_id_t sfilp;
{
  if (sfilp == NULL) return(EINVAL);

  lock_filp(sfilp, VNODE_READ);
  sfilp->filp_count++;
  unlock_filp(sfilp);

  return(OK);
}

/*===========================================================================*
 *                              do_set_filp                                  *
 *===========================================================================*/
int do_set_filp(message *UNUSED(m_out))
{
  filp_id_t f;

  /* This should be replaced with an ACL check. */
  if (who_e != PFS_PROC_NR) return EPERM;

  f = (filp_id_t) job_m_in.VFS_PFS_FILP;
  return set_filp(f);
}

/*===========================================================================*
 *                              copy_filp                                    *
 *===========================================================================*/
int copy_filp(to_ep, cfilp)
endpoint_t to_ep;
filp_id_t cfilp;
{
  int fd;
  int slot;
  struct fproc *rfp;

  if (isokendpt(to_ep, &slot) != OK) return(EINVAL);
  rfp = &fproc[slot];

  /* Find an open slot in fp_filp */
  for (fd = 0; fd < OPEN_MAX; fd++) {
	if (rfp->fp_filp[fd] == NULL) {
		/* Found a free slot, add descriptor */
		rfp->fp_filp[fd] = cfilp;
		rfp->fp_filp[fd]->filp_count++;
		return(fd);
	}
  }

  /* File descriptor table is full */
  return(EMFILE);
}

/*===========================================================================*
 *                              do_copy_filp                                 *
 *===========================================================================*/
int do_copy_filp(message *UNUSED(m_out))
{
  endpoint_t proc_e;
  filp_id_t f;

  /* This should be replaced with an ACL check. */
  if (who_e != PFS_PROC_NR) return EPERM;

  proc_e = job_m_in.VFS_PFS_ENDPT;
  f = (filp_id_t) job_m_in.VFS_PFS_FILP;

  return copy_filp(proc_e, f);
}

/*===========================================================================*
 *                              put_filp                                     *
 *===========================================================================*/
int put_filp(pfilp)
filp_id_t pfilp;
{
  if (pfilp == NULL) {
	return EINVAL;
  } else {
	lock_filp(pfilp, VNODE_OPCL);
	close_filp(pfilp);
	return(OK);
  }
}

/*===========================================================================*
 *                              do_put_filp                                  *
 *===========================================================================*/
int do_put_filp(message *UNUSED(m_out))
{
  filp_id_t f;

  /* This should be replaced with an ACL check. */
  if (who_e != PFS_PROC_NR) return EPERM;

  f = (filp_id_t) job_m_in.VFS_PFS_FILP;
  return put_filp(f);
}

/*===========================================================================*
 *                             cancel_fd				     *
 *===========================================================================*/
int cancel_fd(ep, fd)
endpoint_t ep;
int fd;
{
  int slot;
  struct fproc *rfp;
  struct filp *rfilp;

  if (isokendpt(ep, &slot) != OK) return(EINVAL);
  rfp = &fproc[slot];

  /* Check that the input 'fd' is valid */
  rfilp = (struct filp *) verify_fd(ep, fd);
  if (rfilp != NULL) {
	/* Found a valid descriptor, remove it */
	if (rfp->fp_filp[fd]->filp_count == 0) {
		unlock_filp(rfilp);
		printf("VFS: filp_count for slot %d fd %d already zero", slot,
		      fd);
		return(EINVAL);
	}
	rfp->fp_filp[fd]->filp_count--;
	rfp->fp_filp[fd] = NULL;
	unlock_filp(rfilp);
	return(fd);
  }

  /* File descriptor is not valid for the endpoint. */
  return(EINVAL);
}

/*===========================================================================*
 *                              do_cancel_fd                                 *
 *===========================================================================*/
int do_cancel_fd(message *UNUSED(m_out))
{
  endpoint_t proc_e;
  int fd;

  /* This should be replaced with an ACL check. */
  if (who_e != PFS_PROC_NR) return EPERM;

  proc_e = job_m_in.VFS_PFS_ENDPT;
  fd = job_m_in.VFS_PFS_FD;

  return cancel_fd(proc_e, fd);
}

/*===========================================================================*
 *				close_filp				     *
 *===========================================================================*/
void close_filp(f)
struct filp *f;
{
/* Close a file. Will also unlock filp when done */

  int rw;
  dev_t dev;
  struct vnode *vp;

  /* Must be locked */
  assert(mutex_trylock(&f->filp_lock) == -EDEADLK);
  assert(tll_islocked(&f->filp_vno->v_lock));

  vp = f->filp_vno;

  if (f->filp_count - 1 == 0 && f->filp_mode != FILP_CLOSED) {
	/* Check to see if the file is special. */
	if (S_ISCHR(vp->v_mode) || S_ISBLK(vp->v_mode)) {
		dev = (dev_t) vp->v_sdev;
		if (S_ISBLK(vp->v_mode))  {
			lock_bsf();
			if (vp->v_bfs_e == ROOT_FS_E) {
				/* Invalidate the cache unless the special is
				 * mounted. Assume that the root filesystem's
				 * is open only for fsck.
				 */
				req_flush(vp->v_bfs_e, dev);
			}
			unlock_bsf();

			(void) bdev_close(dev);	/* Ignore errors */
		} else {
			(void) cdev_close(dev);	/* Ignore errors */
		}

		f->filp_mode = FILP_CLOSED;
	}
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (S_ISFIFO(vp->v_mode)) {
	rw = (f->filp_mode & R_BIT ? WRITE : READ);
	release(vp, rw, susp_count);
  }

  if (--f->filp_count == 0) {
	if (S_ISFIFO(vp->v_mode)) {
		/* Last reader or writer is going. Tell PFS about latest
		 * pipe size.
		 */
		truncate_vnode(vp, vp->v_size);
	}

	unlock_vnode(f->filp_vno);
	put_vnode(f->filp_vno);
	f->filp_vno = NULL;
	f->filp_mode = FILP_CLOSED;
	f->filp_count = 0;
  } else if (f->filp_count < 0) {
	panic("VFS: invalid filp count: %d ino %d/%llu", f->filp_count,
	      vp->v_dev, vp->v_inode_nr);
  } else {
	unlock_vnode(f->filp_vno);
  }

  mutex_unlock(&f->filp_lock);
}

/*===========================================================================*
 *				do_dupfrom				     *
 *===========================================================================*/
int do_dupfrom(message *UNUSED(m_out))
{
/* Duplicate a file descriptor from another process into the calling process.
 * The other process is identified by a magic grant created for it to make the
 * initial (IOCTL) request to the calling process. This call has been added
 * specifically for the VND driver.
 */
  struct fproc *rfp;
  struct filp *rfilp;
  struct vnode *vp;
  endpoint_t endpt;
  int r, fd, slot;

  /* This should be replaced with an ACL check. */
  if (!super_user) return(EPERM);

  endpt = (endpoint_t) job_m_in.VFS_DUPFROM_ENDPT;
  fd = job_m_in.VFS_DUPFROM_FD;

  if (isokendpt(endpt, &slot) != OK) return(EINVAL);
  rfp = &fproc[slot];

  /* Obtain the filp, but do not lock it yet: we first need to make sure that
   * locking it will not result in a deadlock.
   */
  if ((rfilp = get_filp2(rfp, fd, VNODE_NONE)) == NULL)
	return(err_code);

  /* For now, we do not allow remote duplication of device nodes.  In practice,
   * only a block-special file can cause a deadlock for the caller (currently
   * only the VND driver).  This would happen if a user process passes in the
   * file descriptor to the device node on which it is performing the IOCTL.
   * This would cause two VFS threads to deadlock on the same filp.  Since the
   * VND driver does not allow device nodes to be used anyway, this somewhat
   * rudimentary check eliminates such deadlocks.  A better solution would be
   * to check if the given endpoint holds a lock to the target filp, but we
   * currently do not have this information within VFS.
   */
  vp = rfilp->filp_vno;
  if (S_ISCHR(vp->v_mode) || S_ISBLK(vp->v_mode))
	return(EINVAL);

  /* Now we can safely lock the filp, copy it, and unlock it again. */
  lock_filp(rfilp, VNODE_READ);

  r = copy_filp(who_e, (filp_id_t) rfilp);

  unlock_filp(rfilp);

  return(r);
}
