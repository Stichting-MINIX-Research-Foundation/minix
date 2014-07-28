/* This file contains the implementation of the three-level-lock. */

#include "fs.h"
#include "glo.h"
#include "tll.h"
#include "threads.h"
#include <assert.h>

static int tll_append(tll_t *tllp, tll_access_t locktype);

static int tll_append(tll_t *tllp, tll_access_t locktype)
{
  struct worker_thread *queue;

  assert(self != NULL);
  assert(tllp != NULL);
  assert(locktype != TLL_NONE);

  /* Read-only and write-only requests go to the write queue. Read-serialized
   * requests go to the serial queue. Then we wait for an event to signal it's
   * our turn to go. */
  queue = NULL;
  if (locktype == TLL_READ || locktype == TLL_WRITE) {
	if (tllp->t_write == NULL)
		tllp->t_write = self;
	else
		queue = tllp->t_write;
  } else {
	if (tllp->t_serial == NULL)
		tllp->t_serial = self;
	else
		queue = tllp->t_serial;
  }

  if (queue != NULL) {	/* Traverse to end of queue */
	while (queue->w_next != NULL) queue = queue->w_next;
	queue->w_next = self;
  }
  self->w_next = NULL; /* End of queue */

  /* Now wait for the event it's our turn */
  worker_wait();

  tllp->t_current = locktype;
  tllp->t_status &= ~TLL_PEND;
  tllp->t_owner = self;

  if (tllp->t_current == TLL_READ) {
	tllp->t_readonly++;
	tllp->t_owner = NULL;
  } else if (tllp->t_current == TLL_WRITE)
	assert(tllp->t_readonly == 0);

  /* Due to the way upgrading and downgrading works, read-only requests are
   * scheduled to run after a downgraded lock is released (because they are
   * queued on the write-only queue which has priority). This results from the
   * fact that the downgrade operation cannot know whether the next locktype on
   * the write-only queue is really write-only or actually read-only. However,
   * that means that read-serialized requests stay queued, while they could run
   * simultaneously with read-only requests. See if there are any and grant
   * the head request access */
  if (tllp->t_current == TLL_READ && tllp->t_serial != NULL) {
	tllp->t_owner = tllp->t_serial;
	tllp->t_serial = tllp->t_serial->w_next;
	tllp->t_owner->w_next = NULL;
	assert(!(tllp->t_status & TLL_PEND));
	tllp->t_status |= TLL_PEND;
	worker_signal(tllp->t_owner);
  }

  return(OK);
}

void tll_downgrade(tll_t *tllp)
{
/* Downgrade three-level-lock tll from write-only to read-serialized, or from
 * read-serialized to read-only. Caveat: as we can't know whether the next
 * lock type on the write queue is actually read-only or write-only, we can't
 * grant access to that type. It will be granted access once we unlock. Also,
 * because we apply write-bias, we can't grant access to read-serialized
 * either, unless nothing is queued on the write-only stack. */

  assert(self != NULL);
  assert(tllp != NULL);
  assert(tllp->t_owner == self);

  switch(tllp->t_current) {
    case TLL_WRITE: tllp->t_current = TLL_READSER; break;
    case TLL_READSER:
	/* If nothing is queued on write-only, but there is a pending lock
	 * requesting read-serialized, grant it and keep the lock type. */

	if (tllp->t_write == NULL && tllp->t_serial != NULL) {
		tllp->t_owner = tllp->t_serial;
		tllp->t_serial = tllp->t_serial->w_next; /* Remove head */
		tllp->t_owner->w_next = NULL;
		assert(!(tllp->t_status & TLL_PEND));
		tllp->t_status |= TLL_PEND;
		worker_signal(tllp->t_owner);
	} else {
		tllp->t_current = TLL_READ;
		tllp->t_owner = NULL;
	}
	tllp->t_readonly++; /* Either way, there's one more read-only lock */
	break;
    default: panic("VFS: Incorrect lock state");
  }

  if (tllp->t_current != TLL_WRITE && tllp->t_current != TLL_READSER)
	assert(tllp->t_owner == NULL);
}

void tll_init(tll_t *tllp)
{
/* Initialize three-level-lock tll */
  assert(tllp != NULL);

  tllp->t_current = TLL_NONE;
  tllp->t_readonly = 0;
  tllp->t_status = TLL_DFLT;
  tllp->t_write = NULL;
  tllp->t_serial = NULL;
  tllp->t_owner = NULL;
}

int tll_islocked(tll_t *tllp)
{
  assert(tllp >= (tll_t *) PAGE_SIZE);
  return(tllp->t_current != TLL_NONE);
}

int tll_locked_by_me(tll_t *tllp)
{
  assert(tllp >= (tll_t *) PAGE_SIZE);
  assert(self != NULL);
  return(tllp->t_owner == self && !(tllp->t_status & TLL_PEND));
}

int tll_lock(tll_t *tllp, tll_access_t locktype)
{
/* Try to lock three-level-lock tll with type locktype */

  assert(self != NULL);
  assert(tllp >= (tll_t *) PAGE_SIZE);
  assert(locktype != TLL_NONE);

  self->w_next = NULL;

  if (locktype != TLL_READ && locktype != TLL_READSER && locktype != TLL_WRITE)
	panic("Invalid lock type %d\n", locktype);

  /* If this locking has pending locks, we wait */
  if (tllp->t_status & TLL_PEND)
	return tll_append(tllp, locktype);

  /* If we already own this lock don't lock it again and return immediately */
  if (tllp->t_owner == self) {
	assert(tllp->t_status == TLL_DFLT);
	return(EBUSY);
  }

  /* If this lock is not accessed by anyone, locktype is granted off the bat */
  if (tllp->t_current == TLL_NONE) {
	tllp->t_current = locktype;
	if (tllp->t_current == TLL_READ)
		tllp->t_readonly = 1;
	else { /* Record owner if locktype is read-serialized or write-only */
		tllp->t_owner = self;
	}
	if (tllp->t_current == TLL_WRITE)
		assert(tllp->t_readonly == 0);
	return(OK);
  }

  /* If the current lock is write-only, we have to wait for that lock to be
   * released (regardless of the value of locktype). */
  if (tllp->t_current == TLL_WRITE)
	return tll_append(tllp, locktype);

  /* However, if it's not and we're requesting a write-only lock, we have to
   * wait until the last read access is released (additional read requests
   * after this write-only requests are to be queued) */
  if (locktype == TLL_WRITE)
	return tll_append(tllp, locktype);

  /* We have to queue read and read-serialized requests if we have a write-only
   * request queued ("write bias") or when a read-serialized lock is trying to
   * upgrade to write-only. The current lock for this tll is either read or
   * read-serialized. */
  if (tllp->t_write != NULL || (tllp->t_status & TLL_UPGR)) {
	assert(!(tllp->t_status & TLL_PEND));
	return tll_append(tllp, locktype);
  }

  /* If this lock is in read-serialized mode, we can allow read requests and
   * queue read-serialized requests */
  if (tllp->t_current == TLL_READSER) {
	if (locktype == TLL_READ && !(tllp->t_status & TLL_UPGR)) {
		tllp->t_readonly++;
		return(OK);
	} else
		return tll_append(tllp, locktype);
  }

  /* Finally, if the current lock is read-only, we can change it to
   * read-serialized if necessary without a problem. */
  tllp->t_current = locktype; /* Either read-only or read-serialized */
  if (tllp->t_current == TLL_READ) {	/* We now have an additional reader */
	tllp->t_readonly++;
	tllp->t_owner = NULL;
  } else {
	assert(tllp->t_current != TLL_WRITE);
	tllp->t_owner = self;		/* We now have a new owner */
	self->w_next = NULL;
  }

  return(OK);
}

int tll_haspendinglock(tll_t *tllp)
{
/* Is someone trying to obtain a lock? */
  assert(tllp != NULL);

  /* Someone is trying to obtain a lock if either the write/read-only queue or
   * the read-serialized queue is not empty. */
  return(tllp->t_write != NULL || tllp->t_serial != NULL);
}

int tll_unlock(tll_t *tllp)
{
/* Unlock a previously locked three-level-lock tll */
  int signal_owner = 0;

  assert(self != NULL);
  assert(tllp != NULL);

  if (tllp->t_owner == NULL || tllp->t_owner != self) {
	/* This unlock must have been done by a read-only lock */
	tllp->t_readonly--;
	assert(tllp->t_readonly >= 0);
	assert(tllp->t_current == TLL_READ || tllp->t_current == TLL_READSER);

	/* If a read-serialized lock is trying to upgrade and there are no more
	 * read-only locks, the lock can now be upgraded to write-only */
	if ((tllp->t_status & TLL_UPGR) && tllp->t_readonly == 0)
		signal_owner = 1;
  }

  if (tllp->t_owner == self && tllp->t_current == TLL_WRITE)
	assert(tllp->t_readonly == 0);

  if(tllp->t_owner == self || (tllp->t_owner == NULL && tllp->t_readonly == 0)){
	/* Let another read-serialized or write-only request obtain access.
	 * Write-only has priority, but only after the last read-only access
	 * has left. Read-serialized access will only be granted if there is
	 * no pending write-only access request. */
	struct worker_thread *new_owner;
	new_owner = NULL;
	tllp->t_owner = NULL;	/* Remove owner of lock */

	if (tllp->t_write != NULL) {
		if (tllp->t_readonly == 0) {
			new_owner = tllp->t_write;
			tllp->t_write = tllp->t_write->w_next;
		}
	} else if (tllp->t_serial != NULL) {
		new_owner = tllp->t_serial;
		tllp->t_serial = tllp->t_serial->w_next;
	}

	/* New owner is head of queue or NULL if no proc is available */
	if (new_owner != NULL) {
		tllp->t_owner = new_owner;
		tllp->t_owner->w_next = NULL;
		assert(tllp->t_owner != self);
		signal_owner = 1;
	}
  }

  /* If no one is using this lock, mark it as not in use */
  if (tllp->t_owner == NULL) {
	if (tllp->t_readonly == 0)
		tllp->t_current = TLL_NONE;
	else
		tllp->t_current = TLL_READ;
  }

  if (tllp->t_current == TLL_NONE || tllp->t_current == TLL_READ) {
	if (!signal_owner) {
		tllp->t_owner = NULL;
	}
  }

  /* If we have a new owner or the current owner managed to upgrade its lock,
   * tell it to start/continue running */
  if (signal_owner) {
	assert(!(tllp->t_status & TLL_PEND));
	tllp->t_status |= TLL_PEND;
	worker_signal(tllp->t_owner);
  }

  return(OK);
}

void tll_upgrade(tll_t *tllp)
{
/* Upgrade three-level-lock tll from read-serialized to write-only */

  assert(self != NULL);
  assert(tllp != NULL);
  assert(tllp->t_owner == self);
  assert(tllp->t_current != TLL_READ); /* i.e., read-serialized or write-only*/
  if (tllp->t_current == TLL_WRITE) return;	/* Nothing to do */
  if (tllp->t_readonly != 0) {		/* Wait for readers to leave */
	assert(!(tllp->t_status & TLL_UPGR));
	tllp->t_status |= TLL_UPGR;
	worker_wait();
	tllp->t_status &= ~TLL_UPGR;
	tllp->t_status &= ~TLL_PEND;
	assert(tllp->t_readonly == 0);
  }
  tllp->t_current = TLL_WRITE;
}
