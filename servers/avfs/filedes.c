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
 */

#include <sys/select.h>
#include <minix/callnr.h>
#include <minix/u64.h>
#include <assert.h>
#include "fs.h"
#include "file.h"
#include "fproc.h"
#include "vnode.h"


FORWARD _PROTOTYPE( filp_id_t verify_fd, (endpoint_t ep, int fd)	);

#if LOCK_DEBUG
/*===========================================================================*
 *				check_filp_locks			     *
 *===========================================================================*/
PUBLIC void check_filp_locks_by_me(void)
{
/* Check whether this thread still has filp locks held */
  struct filp *f;
  int r;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	r = mutex_trylock(&f->filp_lock);
	if (r == -EDEADLK)
		panic("Thread %d still holds filp lock on filp %p call_nr=%d\n",
		      mthread_self(), f, call_nr);
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
PUBLIC void check_filp_locks(void)
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
 *				init_filps					     *
 *===========================================================================*/
PUBLIC void init_filps(void)
{
/* Initialize filps */
  struct filp *f;

  for (f = &filp[0]; f < &filp[NR_FILPS]; f++) {
	mutex_init(&f->filp_lock, NULL);
  }

}

/*===========================================================================*
 *				get_fd					     *
 *===========================================================================*/
PUBLIC int get_fd(int start, mode_t bits, int *k, struct filp **fpt)
{
/* Look for a free file descriptor and a free filp slot.  Fill in the mode word
 * in the latter, but don't claim either one yet, since the open() or creat()
 * may yet fail.
 */

  register struct filp *f;
  register int i;

  /* Search the fproc fp_filp table for a free file descriptor. */
  for (i = start; i < OPEN_MAX; i++) {
	if (fp->fp_filp[i] == NULL && !FD_ISSET(i, &fp->fp_filp_inuse)) {
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
		if (verbose) printf("get_fd: locking filp=%p\n", f);
		f->filp_mode = bits;
		f->filp_pos = cvu64(0);
		f->filp_selectors = 0;
		f->filp_select_ops = 0;
		f->filp_pipe_select_ops = 0;
		f->filp_flags = 0;
		f->filp_state = FS_NORMAL;
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
PUBLIC struct filp *get_filp(fild, locktype)
int fild;			/* file descriptor */
tll_access_t locktype;
{
/* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */

  return get_filp2(fp, fild, locktype);
}


/*===========================================================================*
 *				get_filp2				     *
 *===========================================================================*/
PUBLIC struct filp *get_filp2(rfp, fild, locktype)
register struct fproc *rfp;
int fild;			/* file descriptor */
tll_access_t locktype;
{
/* See if 'fild' refers to a valid file descr.  If so, return its filp ptr. */
  struct filp *filp;

  err_code = EBADF;
  if (fild < 0 || fild >= OPEN_MAX ) return(NULL);
  if (rfp->fp_filp[fild] == NULL && FD_ISSET(fild, &rfp->fp_filp_inuse))
	err_code = EIO;	/* The filedes is not there, but is not closed either.
			 */
  if ((filp = rfp->fp_filp[fild]) != NULL) lock_filp(filp, locktype);

  return(filp);	/* may also be NULL */
}


/*===========================================================================*
 *				find_filp				     *
 *===========================================================================*/
PUBLIC struct filp *find_filp(struct vnode *vp, mode_t bits)
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
 *				invalidate				     *
 *===========================================================================*/
PUBLIC int invalidate(struct filp *fp)
{
/* Invalidate filp. fp_filp_inuse is not cleared, so filp can't be reused
   until it is closed first. */

  int f, fd, n = 0;
  for(f = 0; f < NR_PROCS; f++) {
	if(fproc[f].fp_pid == PID_FREE) continue;
	for(fd = 0; fd < OPEN_MAX; fd++) {
		if(fproc[f].fp_filp[fd] && fproc[f].fp_filp[fd] == fp) {
			fproc[f].fp_filp[fd] = NULL;
			n++;
		}
	}
  }

  return(n);	/* Report back how often this filp has been invalidated. */
}

/*===========================================================================*
 *				lock_filp				     *
 *===========================================================================*/
PUBLIC void lock_filp(filp, locktype)
struct filp *filp;
tll_access_t locktype;
{
  message org_m_in;
  struct fproc *org_fp;
  struct worker_thread *org_self;
  struct vnode *vp;

  assert(filp->filp_count > 0);
  vp = filp->filp_vno;
  assert(vp != NULL);

  if (verbose)
	printf("lock_filp: filp=%p locking vnode %p with locktype %d\n", filp,
		vp, locktype);

  /* Lock vnode only if we haven't already locked it. If already locked by us,
   * we're allowed to have one additional 'soft' lock. */
  if (tll_locked_by_me(&vp->v_lock)) {
	assert(filp->filp_softlock == NULL);
	filp->filp_softlock = fp;
  } else {
	lock_vnode(vp, locktype);
  }

  assert(vp->v_ref_count > 0);	/* vnode still in use? */
  assert(filp->filp_vno == vp);	/* vnode still what we think it is? */
  assert(filp->filp_count > 0); /* filp still in use? */

  /* First try to get filp lock right off the bat */
  if (mutex_trylock(&filp->filp_lock) != 0) {

	/* Already in use, let's wait for our turn */
	org_m_in = m_in;
	org_fp = fp;
	org_self = self;
	assert(mutex_lock(&filp->filp_lock) == 0);
	m_in = org_m_in;
	fp = org_fp;
	self = org_self;
  }

  assert(filp->filp_count > 0);	/* Yet again; filp still in use? */
}

/*===========================================================================*
 *				unlock_filp				     *
 *===========================================================================*/
PUBLIC void unlock_filp(filp)
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
  assert(mutex_unlock(&filp->filp_lock) == 0);
}

/*===========================================================================*
 *				unlock_filps				     *
 *===========================================================================*/
PUBLIC void unlock_filps(filp1, filp2)
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
  assert(mutex_unlock(&filp2->filp_lock) == 0);
  assert(mutex_unlock(&filp1->filp_lock) == 0);
}

/*===========================================================================*
 *				verify_fd				     *
 *===========================================================================*/
PRIVATE filp_id_t verify_fd(ep, fd)
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
PUBLIC int do_verify_fd(void)
{
  struct filp *rfilp;
  rfilp = (struct filp *) verify_fd(m_in.USER_ENDPT, m_in.COUNT);
  m_out.ADDRESS = (void *) rfilp;
  if (rfilp != NULL) unlock_filp(rfilp);
  return (rfilp != NULL) ? OK : EINVAL;
}

/*===========================================================================*
 *                              set_filp                                     *
 *===========================================================================*/
PUBLIC int set_filp(sfilp)
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
PUBLIC int do_set_filp(void)
{
  return set_filp((filp_id_t) m_in.ADDRESS);
}

/*===========================================================================*
 *                              copy_filp                                    *
 *===========================================================================*/
PUBLIC int copy_filp(to_ep, cfilp)
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
	if (rfp->fp_filp[fd] == NULL &&
	    !FD_ISSET(fd, &rfp->fp_filp_inuse)) {

		/* Found a free slot, add descriptor */
		FD_SET(fd, &rfp->fp_filp_inuse);
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
PUBLIC int do_copy_filp(void)
{
  return copy_filp(m_in.USER_ENDPT, (filp_id_t) m_in.ADDRESS);
}

/*===========================================================================*
 *                              put_filp                                     *
 *===========================================================================*/
PUBLIC int put_filp(pfilp)
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
PUBLIC int do_put_filp(void)
{
  return put_filp((filp_id_t) m_in.ADDRESS);
}

/*===========================================================================*
 *                             cancel_fd				     *
 *===========================================================================*/
PUBLIC int cancel_fd(ep, fd)
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
	FD_CLR(fd, &rfp->fp_filp_inuse);
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
PUBLIC int do_cancel_fd(void)
{
  return cancel_fd(m_in.USER_ENDPT, m_in.COUNT);
}

/*===========================================================================*
 *				close_filp				     *
 *===========================================================================*/
PUBLIC void close_filp(f)
struct filp *f;
{
/* Close a file. Will also unlock filp when done */

  int mode_word, rw;
  dev_t dev;
  struct vnode *vp;

  /* Must be locked */
  assert(mutex_trylock(&f->filp_lock) == -EDEADLK);
  assert(tll_islocked(&f->filp_vno->v_lock));

  vp = f->filp_vno;

  if (f->filp_count - 1 == 0 && f->filp_mode != FILP_CLOSED) {
	/* Check to see if the file is special. */
	mode_word = vp->v_mode & I_TYPE;
	if (mode_word == I_CHAR_SPECIAL || mode_word == I_BLOCK_SPECIAL) {
		dev = (dev_t) vp->v_sdev;
		if (mode_word == I_BLOCK_SPECIAL)  {
			lock_bsf();
			if (vp->v_bfs_e == ROOT_FS_E) {
				/* Invalidate the cache unless the special is
				 * mounted. Assume that the root filesystem's
				 * is open only for fsck.
				 */
				req_flush(vp->v_bfs_e, dev);
			}
			unlock_bsf();
		}
		/* Do any special processing on device close. */
		(void) dev_close(dev, f-filp); /* Ignore any errors, even
						 * SUSPEND. */
		f->filp_mode = FILP_CLOSED;
	}
  }

  /* If the inode being closed is a pipe, release everyone hanging on it. */
  if (vp->v_pipe == I_PIPE) {
	rw = (f->filp_mode & R_BIT ? WRITE : READ);
	release(vp, rw, NR_PROCS);
  }

  /* If a write has been done, the inode is already marked as DIRTY. */
  if (--f->filp_count == 0) {
	if (vp->v_pipe == I_PIPE) {
		/* Last reader or writer is going. Tell PFS about latest
		 * pipe size.
		 */
		truncate_vnode(vp, vp->v_size);
	}

	unlock_vnode(f->filp_vno);
	put_vnode(f->filp_vno);
  } else if (f->filp_count < 0) {
	panic("VFS: invalid filp count: %d ino %d/%d", f->filp_count,
	      vp->v_dev, vp->v_inode_nr);
  } else {
	unlock_vnode(f->filp_vno);
  }

  mutex_unlock(&f->filp_lock);
}
