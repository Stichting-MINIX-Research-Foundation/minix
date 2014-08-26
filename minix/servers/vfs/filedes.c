/* This file contains the procedures that manipulate file descriptors.
 *
 * The entry points into this file are
 *   get_fd:	    look for free file descriptor and free filp slots
 *   get_filp:	    look up the filp entry for a given file descriptor
 *   find_filp:	    find a filp slot that points to a given vnode
 *   inval_filp:    invalidate a filp and associated fd's, only let close()
 *                  happen on it
 *   do_copyfd:     copies a file descriptor from or to another endpoint
 */

#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/u64.h>
#include <assert.h>
#include <sys/stat.h>
#include "fs.h"
#include "file.h"
#include "vnode.h"


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
		f->filp_char_select_dev = NO_DEV;
		f->filp_flags = 0;
		f->filp_select_flags = 0;
		f->filp_softlock = NULL;
		f->filp_ioctl_fp = NULL;
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
void invalidate_filp_by_char_major(devmajor_t major)
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
		dev = vp->v_sdev;
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
	rw = (f->filp_mode & R_BIT ? VFS_WRITE : VFS_READ);
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
	panic("VFS: invalid filp count: %d ino %llx/%llu", f->filp_count,
	      vp->v_dev, vp->v_inode_nr);
  } else {
	unlock_vnode(f->filp_vno);
  }

  mutex_unlock(&f->filp_lock);
}

/*===========================================================================*
 *				do_copyfd				     *
 *===========================================================================*/
int do_copyfd(void)
{
/* Copy a file descriptor between processes, or close a remote file descriptor.
 * This call is used as back-call by device drivers (UDS, VND), and is expected
 * to be used in response to an IOCTL to such device drivers.
 */
  struct fproc *rfp;
  struct filp *rfilp;
  endpoint_t endpt;
  int r, fd, what, slot;

  /* This should be replaced with an ACL check. */
  if (!super_user) return(EPERM);

  endpt = job_m_in.m_lsys_vfs_copyfd.endpt;
  fd = job_m_in.m_lsys_vfs_copyfd.fd;
  what = job_m_in.m_lsys_vfs_copyfd.what;

  if (isokendpt(endpt, &slot) != OK) return(EINVAL);
  rfp = &fproc[slot];

  /* FIXME: we should now check that the user process is indeed blocked on an
   * IOCTL call, so that we can safely mess with its file descriptors.  We
   * currently do not have the necessary state to verify this, so we assume
   * that the call is always used in the right way.
   */

  /* Depending on the operation, get the file descriptor from the caller or the
   * user process.  Do not lock the filp yet: we first need to make sure that
   * locking it will not result in a deadlock.
   */
  rfilp = get_filp2((what == COPYFD_TO) ? fp : rfp, fd, VNODE_NONE);
  if (rfilp == NULL)
	return(err_code);

  /* If the filp is involved in an IOCTL by the user process, locking the filp
   * here would result in a deadlock.  This would happen if a user process
   * passes in the file descriptor to the device node on which it is performing
   * the IOCTL.  We do not allow manipulation of such device nodes.  In
   * practice, this only applies to block-special files (and thus VND), because
   * character-special files (as used by UDS) are unlocked during the IOCTL.
   */
  if (rfilp->filp_ioctl_fp == rfp)
	return(EBADF);

  /* Now we can safely lock the filp, copy or close it, and unlock it again. */
  lock_filp(rfilp, VNODE_READ);

  switch (what) {
  case COPYFD_FROM:
	rfp = fp;

	/* FALLTHROUGH */
  case COPYFD_TO:
	/* Find a free file descriptor slot in the local or remote process. */
	for (fd = 0; fd < OPEN_MAX; fd++)
		if (rfp->fp_filp[fd] == NULL)
			break;

	/* If found, fill the slot and return the slot number. */
	if (fd < OPEN_MAX) {
		rfp->fp_filp[fd] = rfilp;
		rfilp->filp_count++;
		r = fd;
	} else
		r = EMFILE;

	break;

  case COPYFD_CLOSE:
	/* This should be used ONLY to revert a successful copy-to operation,
	 * and assumes that the filp is still in use by the caller as well.
	 */
	if (rfilp->filp_count > 1) {
		rfilp->filp_count--;
		rfp->fp_filp[fd] = NULL;
		r = OK;
	} else
		r = EBADF;

	break;

  default:
	r = EINVAL;
  }

  unlock_filp(rfilp);

  return(r);
}
