/* This file handles advisory file locking as required by POSIX.
 *
 * The entry points into this file are
 *   lock_op:	perform locking operations for FCNTL system call
 *   lock_revive: revive processes when a lock is released
 */

#include "fs.h"
#include <minix/com.h>
#include <minix/u64.h>
#include <fcntl.h>
#include <unistd.h>
#include "file.h"
#include "scratchpad.h"
#include "lock.h"
#include "vnode.h"

/*===========================================================================*
 *				lock_op					     *
 *===========================================================================*/
int lock_op(f, req)
struct filp *f;
int req;			/* either F_SETLK or F_SETLKW */
{
/* Perform the advisory locking required by POSIX. */

  int r, ltype, i, conflict = 0, unlocking = 0;
  mode_t mo;
  off_t first, last;
  struct flock flock;
  struct file_lock *flp, *flp2, *empty;

  /* Fetch the flock structure from user space. */
  r = sys_datacopy_wrapper(who_e, scratch(fp).io.io_buffer, VFS_PROC_NR,
		   (vir_bytes) &flock, sizeof(flock));
  if (r != OK) return(EINVAL);

  /* Make some error checks. */
  ltype = flock.l_type;
  mo = f->filp_mode;
  if (ltype != F_UNLCK && ltype != F_RDLCK && ltype != F_WRLCK) return(EINVAL);
  if (req == F_GETLK && ltype == F_UNLCK) return(EINVAL);
  if (!S_ISREG(f->filp_vno->v_mode) && !S_ISBLK(f->filp_vno->v_mode))
	return(EINVAL);
  if (req != F_GETLK && ltype == F_RDLCK && (mo & R_BIT) == 0) return(EBADF);
  if (req != F_GETLK && ltype == F_WRLCK && (mo & W_BIT) == 0) return(EBADF);

  /* Compute the first and last bytes in the lock region. */
  switch (flock.l_whence) {
    case SEEK_SET:	first = 0; break;
    case SEEK_CUR:	first = f->filp_pos; break;
    case SEEK_END:	first = f->filp_vno->v_size; break;
    default:	return(EINVAL);
  }

  /* Check for overflow. */
  if (((long) flock.l_start > 0) && ((first + flock.l_start) < first))
	return(EINVAL);
  if (((long) flock.l_start < 0) && ((first + flock.l_start) > first))
	return(EINVAL);
  first = first + flock.l_start;
  last = first + flock.l_len - 1;
  if (flock.l_len == 0) last = MAX_FILE_POS;
  if (last < first) return(EINVAL);

  /* Check if this region conflicts with any existing lock. */
  empty = NULL;
  for (flp = &file_lock[0]; flp < &file_lock[NR_LOCKS]; flp++) {
	if (flp->lock_type == 0) {
		if (empty == NULL) empty = flp;
		continue;	/* 0 means unused slot */
	}
	if (flp->lock_vnode != f->filp_vno) continue;	/* different file */
	if (last < flp->lock_first) continue;	/* new one is in front */
	if (first > flp->lock_last) continue;	/* new one is afterwards */
	if (ltype == F_RDLCK && flp->lock_type == F_RDLCK) continue;
	if (ltype != F_UNLCK && flp->lock_pid == fp->fp_pid) continue;

	/* There might be a conflict.  Process it. */
	conflict = 1;
	if (req == F_GETLK) break;

	/* If we are trying to set a lock, it just failed. */
	if (ltype == F_RDLCK || ltype == F_WRLCK) {
		if (req == F_SETLK) {
			/* For F_SETLK, just report back failure. */
			return(EAGAIN);
		} else {
			/* For F_SETLKW, suspend the process. */
			suspend(FP_BLOCKED_ON_LOCK);
			return(SUSPEND);
		}
	}

	/* We are clearing a lock and we found something that overlaps. */
	unlocking = 1;
	if (first <= flp->lock_first && last >= flp->lock_last) {
		flp->lock_type = 0;	/* mark slot as unused */
		nr_locks--;		/* number of locks is now 1 less */
		continue;
	}

	/* Part of a locked region has been unlocked. */
	if (first <= flp->lock_first) {
		flp->lock_first = last + 1;
		continue;
	}

	if (last >= flp->lock_last) {
		flp->lock_last = first - 1;
		continue;
	}

	/* Bad luck. A lock has been split in two by unlocking the middle. */
	if (nr_locks == NR_LOCKS) return(ENOLCK);
	for (i = 0; i < NR_LOCKS; i++)
		if (file_lock[i].lock_type == 0) break;
	flp2 = &file_lock[i];
	flp2->lock_type = flp->lock_type;
	flp2->lock_pid = flp->lock_pid;
	flp2->lock_vnode = flp->lock_vnode;
	flp2->lock_first = last + 1;
	flp2->lock_last = flp->lock_last;
	flp->lock_last = first - 1;
	nr_locks++;
  }
  if (unlocking) lock_revive();

  if (req == F_GETLK) {
	if (conflict) {
		/* GETLK and conflict. Report on the conflicting lock. */
		flock.l_type = flp->lock_type;
		flock.l_whence = SEEK_SET;
		flock.l_start = flp->lock_first;
		flock.l_len = flp->lock_last - flp->lock_first + 1;
		flock.l_pid = flp->lock_pid;

	} else {
		/* It is GETLK and there is no conflict. */
		flock.l_type = F_UNLCK;
	}

	/* Copy the flock structure back to the caller. */
	r = sys_datacopy_wrapper(VFS_PROC_NR, (vir_bytes) &flock, who_e,
		scratch(fp).io.io_buffer, sizeof(flock));
	return(r);
  }

  if (ltype == F_UNLCK) return(OK);	/* unlocked a region with no locks */

  /* There is no conflict.  If space exists, store new lock in the table. */
  if (empty == NULL) return(ENOLCK);	/* table full */
  empty->lock_type = ltype;
  empty->lock_pid = fp->fp_pid;
  empty->lock_vnode = f->filp_vno;
  empty->lock_first = first;
  empty->lock_last = last;
  nr_locks++;
  return(OK);
}


/*===========================================================================*
 *				lock_revive				     *
 *===========================================================================*/
void lock_revive()
{
/* Go find all the processes that are waiting for any kind of lock and
 * revive them all.  The ones that are still blocked will block again when
 * they run.  The others will complete.  This strategy is a space-time
 * tradeoff.  Figuring out exactly which ones to unblock now would take
 * extra code, and the only thing it would win would be some performance in
 * extremely rare circumstances (namely, that somebody actually used
 * locking).
 */

  struct fproc *fptr;

  for (fptr = &fproc[0]; fptr < &fproc[NR_PROCS]; fptr++){
	if (fptr->fp_pid == PID_FREE) continue;
	if (fptr->fp_blocked_on == FP_BLOCKED_ON_LOCK) {
		revive(fptr->fp_endpoint, 0);
	}
  }
}
