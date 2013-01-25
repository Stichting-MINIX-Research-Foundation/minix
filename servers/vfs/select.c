/* Implement entry point to select system call.
 *
 * The entry points into this file are
 *   do_select:	       perform the SELECT system call
 *   select_callback:  notify select system of possible fd operation
 *   select_unsuspend_by_endpt: cancel a blocking select on exiting driver
 */

#include "fs.h"
#include <sys/fcntl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <string.h>
#include <assert.h>

#include "file.h"
#include "fproc.h"
#include "dmap.h"
#include "vnode.h"

/* max. number of simultaneously pending select() calls */
#define MAXSELECTS 25
#define FROM_PROC 0
#define TO_PROC   1

static struct selectentry {
  struct fproc *requestor;	/* slot is free iff this is NULL */
  endpoint_t req_endpt;
  fd_set readfds, writefds, errorfds;
  fd_set ready_readfds, ready_writefds, ready_errorfds;
  fd_set *vir_readfds, *vir_writefds, *vir_errorfds;
  struct filp *filps[OPEN_MAX];
  int type[OPEN_MAX];
  int nfds, nreadyfds;
  int error;
  char block;
  clock_t expiry;
  timer_t timer;	/* if expiry > 0 */
} selecttab[MAXSELECTS];

static int copy_fdsets(struct selectentry *se, int nfds, int
	direction);
static int do_select_request(struct selectentry *se, int fd, int *ops);
static void filp_status(struct filp *fp, int status);
static int is_deferred(struct selectentry *se);
static void restart_proc(struct selectentry *se);
static void ops2tab(int ops, int fd, struct selectentry *e);
static int is_regular_file(struct filp *f);
static int is_pipe(struct filp *f);
static int is_supported_major(struct filp *f);
static void select_lock_filp(struct filp *f, int ops);
static int select_request_async(struct filp *f, int *ops, int block);
static int select_request_file(struct filp *f, int *ops, int block);
static int select_request_major(struct filp *f, int *ops, int block);
static int select_request_pipe(struct filp *f, int *ops, int block);
static int select_request_sync(struct filp *f, int *ops, int block);
static void select_cancel_all(struct selectentry *e);
static void select_cancel_filp(struct filp *f);
static void select_return(struct selectentry *);
static void select_restart_filps(void);
static int tab2ops(int fd, struct selectentry *e);
static void wipe_select(struct selectentry *s);

static struct fdtype {
	int (*select_request)(struct filp *, int *ops, int block);
	int (*type_match)(struct filp *f);
} fdtypes[] = {
	{ select_request_major, is_supported_major },
	{ select_request_file, is_regular_file },
	{ select_request_pipe, is_pipe },
};
#define SEL_FDS		(sizeof(fdtypes) / sizeof(fdtypes[0]))
static int select_majors[] = { /* List of majors that support selecting on */
	TTY_MAJOR,
	INET_MAJOR,
	UDS_MAJOR,
	LOG_MAJOR,
};
#define SEL_MAJORS	(sizeof(select_majors) / sizeof(select_majors[0]))

/*===========================================================================*
 *				do_select				     *
 *===========================================================================*/
int do_select(void)
{
/* Implement the select(nfds, readfds, writefds, errorfds, timeout) system
 * call. First we copy the arguments and verify their sanity. Then we check
 * whether there are file descriptors that satisfy the select call right of the
 * bat. If so, or if there are no ready file descriptors but the process
 * requested to return immediately, we return the result. Otherwise we set a
 * timeout and wait for either the file descriptors to become ready or the
 * timer to go off. If no timeout value was provided, we wait indefinitely. */

  int r, nfds, do_timeout = 0, fd, s;
  struct timeval timeout;
  struct selectentry *se;
  vir_bytes vtimeout;

  nfds = job_m_in.SEL_NFDS;
  vtimeout = (vir_bytes) job_m_in.SEL_TIMEOUT;

  /* Sane amount of file descriptors? */
  if (nfds < 0 || nfds > OPEN_MAX) return(EINVAL);

  /* Find a slot to store this select request */
  for (s = 0; s < MAXSELECTS; s++)
	if (selecttab[s].requestor == NULL) /* Unused slot */
		break;
  if (s >= MAXSELECTS) return(ENOSPC);

  se = &selecttab[s];
  wipe_select(se);	/* Clear results of previous usage */
  se->requestor = fp;
  se->req_endpt = who_e;
  se->vir_readfds = (fd_set *) job_m_in.SEL_READFDS;
  se->vir_writefds = (fd_set *) job_m_in.SEL_WRITEFDS;
  se->vir_errorfds = (fd_set *) job_m_in.SEL_ERRORFDS;

  /* Copy fdsets from the process */
  if ((r = copy_fdsets(se, nfds, FROM_PROC)) != OK) {
	se->requestor = NULL;
	return(r);
  }

  /* Did the process set a timeout value? If so, retrieve it. */
  if (vtimeout != 0) {
	do_timeout = 1;
	r = sys_vircopy(who_e, (vir_bytes) vtimeout, SELF, 
			(vir_bytes) &timeout, sizeof(timeout));
	if (r != OK) {
		se->requestor = NULL;
		return(r);
	}
  }

  /* No nonsense in the timeval */
  if (do_timeout && (timeout.tv_sec < 0 || timeout.tv_usec < 0)) {
	se->requestor = NULL;
	return(EINVAL);
  }

  /* If there is no timeout, we block forever. Otherwise, we block up to the
   * specified time interval.
   */
  if (!do_timeout)	/* No timeout value set */
	se->block = 1;
  else if (do_timeout && (timeout.tv_sec > 0 || timeout.tv_usec > 0))
	se->block = 1;
  else			/* timeout set as (0,0) - this effects a poll */
	se->block = 0;
  se->expiry = 0;	/* no timer set (yet) */

  /* Verify that file descriptors are okay to select on */
  for (fd = 0; fd < nfds; fd++) {
	struct filp *f;
	unsigned int type, ops;

	/* Because the select() interface implicitly includes file descriptors
	 * you might not want to select on, we have to figure out whether we're
	 * interested in them. Typically, these file descriptors include fd's
	 * inherited from the parent proc and file descriptors that have been
	 * close()d, but had a lower fd than one in the current set.
	 */
	if (!(ops = tab2ops(fd, se)))
		continue; /* No operations set; nothing to do for this fd */

	/* Get filp belonging to this fd */
	f = se->filps[fd] = get_filp(fd, VNODE_READ);
	if (f == NULL) {
		if (err_code == EBADF)
			r = err_code;
		else /* File descriptor is 'ready' to return EIO */
			r = EINTR;

		se->requestor = NULL;
		return(r);
	}

	/* Check file types. According to POSIX 2008:
	 * "The pselect() and select() functions shall support regular files,
	 * terminal and pseudo-terminal devices, FIFOs, pipes, and sockets. The
	 * behavior of pselect() and select() on file descriptors that refer to
	 * other types of file is unspecified."
	 *
	 * In our case, terminal and pseudo-terminal devices are handled by the
	 * TTY major and sockets by either INET major (socket type AF_INET) or
	 * PFS major (socket type AF_UNIX). PFS acts as an FS when it handles
	 * pipes and as a driver when it handles sockets. Additionally, we
	 * support select on the LOG major to handle kernel logging, which is
	 * beyond the POSIX spec. */

	se->type[fd] = -1;
	for (type = 0; type < SEL_FDS; type++) {
		if (fdtypes[type].type_match(f)) {
			se->type[fd] = type;
			se->nfds = fd+1;
			se->filps[fd]->filp_selectors++;
			break;
		}
	}
	unlock_filp(f);
	if (se->type[fd] == -1) { /* Type not found */
		se->requestor = NULL;
		return(EBADF);
	}
  }

  /* Check all file descriptors in the set whether one is 'ready' now */
  for (fd = 0; fd < nfds; fd++) {
	int ops, r;
	struct filp *f;

	/* Again, check for involuntarily selected fd's */
	if (!(ops = tab2ops(fd, se)))
		continue; /* No operations set; nothing to do for this fd */

	/* Test filp for select operations if not already done so. e.g.,
	 * processes sharing a filp and both doing a select on that filp. */
	f = se->filps[fd];
	if ((f->filp_select_ops & ops) != ops) {
		int wantops;

		wantops = (f->filp_select_ops |= ops);
		r = do_select_request(se, fd, &wantops);
		if (r != OK && r != SUSPEND)
			break; /* Error or bogus return code; abort */

		/* The select request above might have turned on/off some
		 * operations because they were 'ready' or not meaningful.
		 * Either way, we might have a result and we need to store them
		 * in the select table entry. */
		if (wantops & ops) ops2tab(wantops, fd, se);
	}
  }

  if ((se->nreadyfds > 0 || !se->block) && !is_deferred(se)) {
	/* fd's were found that were ready to go right away, and/or
	 * we were instructed not to block at all. Must return
	 * immediately.
	 */
	r = copy_fdsets(se, se->nfds, TO_PROC);
	select_cancel_all(se);
	se->requestor = NULL;

	if (r != OK)
		return(r);
	else if (se->error != OK)
		return(se->error);

	return(se->nreadyfds);
  }

  /* Convert timeval to ticks and set the timer. If it fails, undo
   * all, return error.
   */
  if (do_timeout) {
	int ticks;
	/* Open Group:
	 * "If the requested timeout interval requires a finer
	 * granularity than the implementation supports, the
	 * actual timeout interval shall be rounded up to the next
	 * supported value."
	 */
#define USECPERSEC 1000000
	while(timeout.tv_usec >= USECPERSEC) {
		/* this is to avoid overflow with *system_hz below */
		timeout.tv_usec -= USECPERSEC;
		timeout.tv_sec++;
	}
	ticks = timeout.tv_sec * system_hz +
		(timeout.tv_usec * system_hz + USECPERSEC-1) / USECPERSEC;
	se->expiry = ticks;
	set_timer(&se->timer, ticks, select_timeout_check, s);
  }

  /* process now blocked */
  suspend(FP_BLOCKED_ON_SELECT);
  return(SUSPEND);
}

/*===========================================================================*
 *				is_deferred				     *
 *===========================================================================*/
static int is_deferred(struct selectentry *se)
{
/* Find out whether this select has pending initial replies */

  int fd;
  struct filp *f;

  for (fd = 0; fd < se->nfds; fd++) {
	if ((f = se->filps[fd]) == NULL) continue;
	if (f->filp_select_flags & (FSF_UPDATE|FSF_BUSY)) return(TRUE);
  }

  return(FALSE);
}


/*===========================================================================*
 *				is_regular_file				     *
 *===========================================================================*/
static int is_regular_file(struct filp *f)
{
  return(f && f->filp_vno && S_ISREG(f->filp_vno->v_mode));
}

/*===========================================================================*
 *				is_pipe					     *
 *===========================================================================*/
static int is_pipe(struct filp *f)
{
/* Recognize either anonymous pipe or named pipe (FIFO) */
  return(f && f->filp_vno && S_ISFIFO(f->filp_vno->v_mode));
}

/*===========================================================================*
 *				is_supported_major			     *
 *===========================================================================*/
static int is_supported_major(struct filp *f)
{
/* See if this filp is a handle on a device on which we support select() */
  unsigned int m;

  if (!(f && f->filp_vno)) return(FALSE);
  if (!S_ISCHR(f->filp_vno->v_mode)) return(FALSE);

  for (m = 0; m < SEL_MAJORS; m++)
	if (major(f->filp_vno->v_sdev) == select_majors[m])
		return(TRUE);

  return(FALSE);
}

/*===========================================================================*
 *				select_request_async			     *
 *===========================================================================*/
static int select_request_async(struct filp *f, int *ops, int block)
{
  int r, rops, major;
  struct dmap *dp;

  rops = *ops;

  /* By default, nothing to do */
  *ops = 0;

  if (!block && (f->filp_select_flags & FSF_BLOCKED)) {
	/* This filp is blocked waiting for a reply, but we don't want to
	 * block ourselves. Unless we're awaiting the initial reply, these
	 * operations won't be ready */
	if (!(f->filp_select_flags & FSF_BUSY)) {
		if ((rops & SEL_RD) && (f->filp_select_flags & FSF_RD_BLOCK))
			rops &= ~SEL_RD;
		if ((rops & SEL_WR) && (f->filp_select_flags & FSF_WR_BLOCK))
			rops &= ~SEL_WR;
		if ((rops & SEL_ERR) && (f->filp_select_flags & FSF_ERR_BLOCK))
			rops &= ~SEL_ERR;
		if (!(rops & (SEL_RD|SEL_WR|SEL_ERR)))
			return(OK);
	}
  }

  f->filp_select_flags |= FSF_UPDATE;
  if (block) {
	rops |= SEL_NOTIFY;
	if (rops & SEL_RD)	f->filp_select_flags |= FSF_RD_BLOCK;
	if (rops & SEL_WR)	f->filp_select_flags |= FSF_WR_BLOCK;
	if (rops & SEL_ERR)	f->filp_select_flags |= FSF_ERR_BLOCK;
  }

  if (f->filp_select_flags & FSF_BUSY)
	return(SUSPEND);

  major = major(f->filp_vno->v_sdev);
  if (major < 0 || major >= NR_DEVICES) return(ENXIO);
  dp = &dmap[major];
  if (dp->dmap_sel_filp)
	return(SUSPEND);

  f->filp_select_flags &= ~FSF_UPDATE;
  r = dev_io(VFS_DEV_SELECT, f->filp_vno->v_sdev, rops, NULL,
	     cvu64(0), 0, 0, FALSE);
  if (r < 0 && r != SUSPEND)
	return(r);

  if (r != SUSPEND)
	panic("select_request_asynch: expected SUSPEND got: %d", r);

  dp->dmap_sel_filp = f;
  f->filp_select_flags |= FSF_BUSY;

  return(SUSPEND);
}

/*===========================================================================*
 *				select_request_file			     *
 *===========================================================================*/
static int select_request_file(struct filp *UNUSED(f), int *UNUSED(ops),
  int UNUSED(block))
{
  /* Files are always ready, so output *ops is input *ops */
  return(OK);
}

/*===========================================================================*
 *				select_request_major			     *
 *===========================================================================*/
static int select_request_major(struct filp *f, int *ops, int block)
{
  int major, r;

  major = major(f->filp_vno->v_sdev);
  if (major < 0 || major >= NR_DEVICES) return(ENXIO);

  if (dmap[major].dmap_style == STYLE_DEVA ||
      dmap[major].dmap_style == STYLE_CLONE_A)
	r = select_request_async(f, ops, block);
  else
	r = select_request_sync(f, ops, block);

  return(r);
}

/*===========================================================================*
 *				select_request_sync			     *
 *===========================================================================*/
static int select_request_sync(struct filp *f, int *ops, int block)
{
  int rops;

  rops = *ops;
  if (block) rops |= SEL_NOTIFY;
  *ops = dev_io(VFS_DEV_SELECT, f->filp_vno->v_sdev, rops, NULL,
		cvu64(0), 0, 0, FALSE);
  if (*ops < 0)
	return(*ops);

  return(OK);
}

/*===========================================================================*
 *				select_request_pipe			     *
 *===========================================================================*/
static int select_request_pipe(struct filp *f, int *ops, int block)
{
  int orig_ops, r = 0, err;

  orig_ops = *ops;

  if ((*ops & (SEL_RD|SEL_ERR))) {
	/* Check if we can read 1 byte */
	err = pipe_check(f->filp_vno, READING, f->filp_flags & ~O_NONBLOCK, 1,
			 1 /* Check only */);

	if (err != SUSPEND)
		r |= SEL_RD;
	if (err < 0 && err != SUSPEND)
		r |= SEL_ERR;
	if (err == SUSPEND && !(f->filp_mode & R_BIT)) {
		/* A "meaningless" read select, therefore ready
		 * for reading and no error set. */
		r |= SEL_RD;
		r &= ~SEL_ERR;
	}
  }

  if ((*ops & (SEL_WR|SEL_ERR))) {
	/* Check if we can write 1 byte */
	err = pipe_check(f->filp_vno, WRITING, f->filp_flags & ~O_NONBLOCK, 1,
			 1 /* Check only */);

	if (err != SUSPEND)
		r |= SEL_WR;
	if (err < 0 && err != SUSPEND)
		r |= SEL_ERR;
	if (err == SUSPEND && !(f->filp_mode & W_BIT)) {
		/* A "meaningless" write select, therefore ready
                   for writing and no error set. */
		r |= SEL_WR;
		r &= ~SEL_ERR;
	}
  }

  /* Some options we collected might not be requested. */
  *ops = r & orig_ops;

  if (!*ops && block)
	f->filp_pipe_select_ops |= orig_ops;

  return(OK);
}

/*===========================================================================*
 *				tab2ops					     *
 *===========================================================================*/
static int tab2ops(int fd, struct selectentry *e)
{
  int ops = 0;
  if (FD_ISSET(fd, &e->readfds))  ops |= SEL_RD;
  if (FD_ISSET(fd, &e->writefds)) ops |= SEL_WR;
  if (FD_ISSET(fd, &e->errorfds)) ops |= SEL_ERR;

  return(ops);
}


/*===========================================================================*
 *				ops2tab					     *
 *===========================================================================*/
static void ops2tab(int ops, int fd, struct selectentry *e)
{
  if ((ops & SEL_RD) && e->vir_readfds && FD_ISSET(fd, &e->readfds) &&
      !FD_ISSET(fd, &e->ready_readfds)) {
	FD_SET(fd, &e->ready_readfds);
	e->nreadyfds++;
  }

  if ((ops & SEL_WR) && e->vir_writefds && FD_ISSET(fd, &e->writefds) &&
      !FD_ISSET(fd, &e->ready_writefds)) {
	FD_SET(fd, &e->ready_writefds);
	e->nreadyfds++;
  }

  if ((ops & SEL_ERR) && e->vir_errorfds && FD_ISSET(fd, &e->errorfds) &&
      !FD_ISSET(fd, &e->ready_errorfds)) {
	FD_SET(fd, &e->ready_errorfds);
	e->nreadyfds++;
  }
}


/*===========================================================================*
 *				copy_fdsets				     *
 *===========================================================================*/
static int copy_fdsets(struct selectentry *se, int nfds, int direction)
{
  int r;
  size_t fd_setsize;
  endpoint_t src_e, dst_e;
  fd_set *src_fds, *dst_fds;

  if (nfds < 0 || nfds > OPEN_MAX)
	panic("select copy_fdsets: nfds wrong: %d", nfds);

  /* Only copy back as many bits as the user expects. */
#ifdef __NBSD_LIBC
  fd_setsize = (size_t) (howmany(nfds, __NFDBITS) * sizeof(__fd_mask));
#else
  fd_setsize = (size_t) (_FDSETWORDS(nfds) * _FDSETBITSPERWORD/8);
#endif

  /* Set source and destination endpoints */
  src_e = (direction == FROM_PROC) ? se->req_endpt : SELF;
  dst_e = (direction == FROM_PROC) ? SELF : se->req_endpt;

  /* read set */
  src_fds = (direction == FROM_PROC) ? se->vir_readfds : &se->ready_readfds;
  dst_fds = (direction == FROM_PROC) ? &se->readfds : se->vir_readfds;
  if (se->vir_readfds) {
	r = sys_vircopy(src_e, (vir_bytes) src_fds, dst_e, 
			(vir_bytes) dst_fds, fd_setsize);
	if (r != OK) return(r);
  }

  /* write set */
  src_fds = (direction == FROM_PROC) ? se->vir_writefds : &se->ready_writefds;
  dst_fds = (direction == FROM_PROC) ? &se->writefds : se->vir_writefds;
  if (se->vir_writefds) {
	r = sys_vircopy(src_e, (vir_bytes) src_fds, dst_e, 
			(vir_bytes) dst_fds, fd_setsize);
	if (r != OK) return(r);
  }

  /* error set */
  src_fds = (direction == FROM_PROC) ? se->vir_errorfds : &se->ready_errorfds;
  dst_fds = (direction == FROM_PROC) ? &se->errorfds : se->vir_errorfds;
  if (se->vir_errorfds) {
	r = sys_vircopy(src_e, (vir_bytes) src_fds, dst_e, 
			(vir_bytes) dst_fds, fd_setsize);
	if (r != OK) return(r);
  }

  return(OK);
}


/*===========================================================================*
 *				select_cancel_all			     *
 *===========================================================================*/
static void select_cancel_all(struct selectentry *se)
{
/* Cancel select. Decrease select usage and cancel timer */

  int fd;
  struct filp *f;

  for (fd = 0; fd < se->nfds; fd++) {
	if ((f = se->filps[fd]) == NULL) continue;
	se->filps[fd] = NULL;
	select_cancel_filp(f);
  }

  if (se->expiry > 0) {
	cancel_timer(&se->timer);
	se->expiry = 0;
  }

  se->requestor = NULL;
}

/*===========================================================================*
 *				select_cancel_filp			     *
 *===========================================================================*/
static void select_cancel_filp(struct filp *f)
{
/* Reduce number of select users of this filp */

  assert(f);
  assert(f->filp_selectors >= 0);
  if (f->filp_selectors == 0) return;
  if (f->filp_count == 0) return;

  select_lock_filp(f, f->filp_select_ops);

  f->filp_selectors--;
  if (f->filp_selectors == 0) {
	/* No one selecting on this filp anymore, forget about select state */
	f->filp_select_ops = 0;
	f->filp_select_flags = 0;
	f->filp_pipe_select_ops = 0;
  }

  unlock_filp(f);
}

/*===========================================================================*
 *				select_return				     *
 *===========================================================================*/
static void select_return(struct selectentry *se)
{
  int r, r1;

  assert(!is_deferred(se));	/* Not done yet, first wait for async reply */

  select_cancel_all(se);

  r1 = copy_fdsets(se, se->nfds, TO_PROC);
  if (r1 != OK)
	r = r1;
  else if (se->error != OK)
	r = se->error;
  else
	r = se->nreadyfds;

  revive(se->req_endpt, r);
}


/*===========================================================================*
 *				select_callback			             *
 *===========================================================================*/
void select_callback(struct filp *f, int status)
{
  filp_status(f, status);
}

/*===========================================================================*
 *				init_select  				     *
 *===========================================================================*/
void init_select(void)
{
  int s;

  for (s = 0; s < MAXSELECTS; s++)
	init_timer(&selecttab[s].timer);
}


/*===========================================================================*
 *				select_forget			             *
 *===========================================================================*/
void select_forget(endpoint_t proc_e)
{
/* Something has happened (e.g. signal delivered that interrupts select()).
 * Totally forget about the select(). */

  int slot;
  struct selectentry *se;

  for (slot = 0; slot < MAXSELECTS; slot++) {
	se = &selecttab[slot];
	if (se->requestor != NULL && se->req_endpt == proc_e)
		break;
  }

  if (slot >= MAXSELECTS) return;	/* Entry not found */
  se->error = EINTR;
  if (is_deferred(se)) return;		/* Still awaiting initial reply */

  select_cancel_all(se);
}


/*===========================================================================*
 *				select_timeout_check	  	     	     *
 *===========================================================================*/
void select_timeout_check(timer_t *timer)
{
  int s;
  struct selectentry *se;

  s = tmr_arg(timer)->ta_int;
  if (s < 0 || s >= MAXSELECTS) return;	/* Entry does not exist */

  se = &selecttab[s];
  if (se->requestor == NULL) return;
  fp = se->requestor;
  if (se->expiry <= 0) return;	/* Strange, did we even ask for a timeout? */
  se->expiry = 0;
  if (is_deferred(se)) return;	/* Wait for initial replies to DEV_SELECT */
  select_return(se);
}


/*===========================================================================*
 *				select_unsuspend_by_endpt  	     	     *
 *===========================================================================*/
void select_unsuspend_by_endpt(endpoint_t proc_e)
{
/* Revive blocked processes when a driver has disappeared */

  int fd, s, major;
  struct selectentry *se;
  struct filp *f;

  for (s = 0; s < MAXSELECTS; s++) {
	int wakehim = 0;
	se = &selecttab[s];
	if (se->requestor == NULL) continue;
	if (se->requestor->fp_endpoint == proc_e) {
		assert(se->requestor->fp_flags & FP_EXITING);
		select_cancel_all(se);
		continue;
	}

	for (fd = 0; fd < se->nfds; fd++) {
		if ((f = se->filps[fd]) == NULL || f->filp_vno == NULL)
			continue;

		major = major(f->filp_vno->v_sdev);
		if (dmap_driver_match(proc_e, major)) {
			se->filps[fd] = NULL;
			se->error = EINTR;
			select_cancel_filp(f);
			wakehim = 1;
		}
	}

	if (wakehim && !is_deferred(se))
		select_return(se);
  }
}

/*===========================================================================*
 *				select_reply1				     *
 *===========================================================================*/
void select_reply1(driver_e, minor, status)
endpoint_t driver_e;
int minor;
int status;
{
/* Handle reply to DEV_SELECT request */

  int major;
  dev_t dev;
  struct filp *f;
  struct dmap *dp;
  struct vnode *vp;

  /* Figure out which device is replying */
  if ((dp = get_dmap(driver_e)) == NULL) return;

  major = dp-dmap;
  dev = makedev(major, minor);

  /* Get filp belonging to character special file */
  if ((f = dp->dmap_sel_filp) == NULL) {
	printf("VFS (%s:%d): major %d was not expecting a DEV_SELECT reply\n",
		__FILE__, __LINE__, major);
	return;
  }

  /* Is the filp still in use and busy waiting for a reply? The owner might
   * have vanished before the driver was able to reply. */
  if (f->filp_count >= 1 && (f->filp_select_flags & FSF_BUSY)) {
	/* Find vnode and check we got a reply from the device we expected */
	vp = f->filp_vno;
	assert(vp != NULL);
	assert(S_ISCHR(vp->v_mode));
	if (vp->v_sdev != dev) {
		printf("VFS (%s:%d): expected reply from dev %d not %d\n",
			__FILE__, __LINE__, vp->v_sdev, dev);
		return;
	}
  }

  /* No longer waiting for a reply from this device */
  dp->dmap_sel_filp = NULL;

  /* Process select result only if requestor is still around. That is, the
   * corresponding filp is still in use.
   */
  if (f->filp_count >= 1) {
	select_lock_filp(f, f->filp_select_ops);
	f->filp_select_flags &= ~FSF_BUSY;

	/* The select call is done now, except when
	 * - another process started a select on the same filp with possibly a
	 *   different set of operations.
	 * - a process does a select on the same filp but using different file
	 *   descriptors.
	 * - the select has a timeout. Upon receiving this reply the operations
	 *   might not be ready yet, so we want to wait for that to ultimately
	 *   happen.
	 *   Therefore we need to keep remembering what the operations are.
	 */
	if (!(f->filp_select_flags & (FSF_UPDATE|FSF_BLOCKED)))
		f->filp_select_ops = 0;		/* done selecting */
	else if (!(f->filp_select_flags & FSF_UPDATE))
		/* there may be operations pending */
		f->filp_select_ops &= ~status;

	/* Record new filp status */
	if (!(status == 0 && (f->filp_select_flags & FSF_BLOCKED))) {
		if (status > 0) {	/* operations ready */
			if (status & SEL_RD)
				f->filp_select_flags &= ~FSF_RD_BLOCK;
			if (status & SEL_WR)
				f->filp_select_flags &= ~FSF_WR_BLOCK;
			if (status & SEL_ERR)
				f->filp_select_flags &= ~FSF_ERR_BLOCK;
		} else if (status < 0) { /* error */
			/* Always unblock upon error */
			f->filp_select_flags &= ~FSF_BLOCKED;
		}
	}

	unlock_filp(f);
	filp_status(f, status); /* Tell filp owners about the results */
  }

  select_restart_filps();
}


/*===========================================================================*
 *				select_reply2				     *
 *===========================================================================*/
void select_reply2(driver_e, minor, status)
endpoint_t driver_e;
int minor;
int status;
{
/* Handle secondary reply to DEV_SELECT request. A secondary reply occurs when
 * the select request is 'blocking' until an operation becomes ready. */
  int major, slot, fd;
  dev_t dev;
  struct filp *f;
  struct dmap *dp;
  struct vnode *vp;
  struct selectentry *se;

  if (status == 0) {
	printf("VFS (%s:%d): weird status (%d) to report\n",
		__FILE__, __LINE__, status);
	return;
  }

  /* Figure out which device is replying */
  if ((dp = get_dmap(driver_e)) == NULL) {
	printf("VFS (%s:%d): endpoint %d is not a known driver endpoint\n",
		__FILE__, __LINE__, driver_e);
	return;
  }
  major = dp-dmap;
  dev = makedev(major, minor);

  /* Find all file descriptors selecting for this device */
  for (slot = 0; slot < MAXSELECTS; slot++) {
	se = &selecttab[slot];
	if (se->requestor == NULL) continue;	/* empty slot */

	for (fd = 0; fd < se->nfds; fd++) {
		if ((f = se->filps[fd]) == NULL) continue;
		if ((vp = f->filp_vno) == NULL) continue;
		if (!S_ISCHR(vp->v_mode)) continue;
		if (vp->v_sdev != dev) continue;

		select_lock_filp(f, f->filp_select_ops);
		if (status > 0) {	/* Operations ready */
			/* Clear the replied bits from the request
			 * mask unless FSF_UPDATE is set.
			 */
			if (!(f->filp_select_flags & FSF_UPDATE))
				f->filp_select_ops &= ~status;
			if (status & SEL_RD)
				f->filp_select_flags &= ~FSF_RD_BLOCK;
			if (status & SEL_WR)
				f->filp_select_flags &= ~FSF_WR_BLOCK;
			if (status & SEL_ERR)
				f->filp_select_flags &= ~FSF_ERR_BLOCK;

			ops2tab(status, fd, se);
		} else {
			f->filp_select_flags &= ~FSF_BLOCKED;
			ops2tab(SEL_RD|SEL_WR|SEL_ERR, fd, se);
		}
		unlock_filp(f);
		if (se->nreadyfds > 0) restart_proc(se);
	}
  }

  select_restart_filps();
}

/*===========================================================================*
 *				select_restart_filps			     *
 *===========================================================================*/
static void select_restart_filps()
{
  int fd, slot;
  struct filp *f;
  struct vnode *vp;
  struct selectentry *se;

  /* Locate filps that can be restarted */
  for (slot = 0; slot < MAXSELECTS; slot++) {
	se = &selecttab[slot];
	if (se->requestor == NULL) continue; /* empty slot */

	/* Only 'deferred' processes are eligible to restart */
	if (!is_deferred(se)) continue;

	/* Find filps that are not waiting for a reply, but have an updated
	 * status (i.e., another select on the same filp with possibly a
	 * different set of operations is to be done), and thus requires the
	 * select request to be sent again).
	 */
	for (fd = 0; fd < se->nfds; fd++) {
		int r, wantops, ops;
		if ((f = se->filps[fd]) == NULL) continue;
		if (f->filp_select_flags & FSF_BUSY) /* Still waiting for */
			continue;		     /* initial reply */
		if (!(f->filp_select_flags & FSF_UPDATE)) /* Must be in  */
			continue;			  /* 'update' state */

		wantops = ops = f->filp_select_ops;
		vp = f->filp_vno;
		assert(S_ISCHR(vp->v_mode));
		r = do_select_request(se, fd, &wantops);
		if (r != OK && r != SUSPEND)
			break; /* Error or bogus return code; abort */
		if (wantops & ops) ops2tab(wantops, fd, se);
	}
  }
}

/*===========================================================================*
 *				do_select_request			     *
 *===========================================================================*/
static int do_select_request(se, fd, ops)
struct selectentry *se;
int fd;
int *ops;
{
/* Perform actual select request for file descriptor fd */

  int r, type;
  struct filp *f;

  type = se->type[fd];
  f = se->filps[fd];
  select_lock_filp(f, *ops);
  r = fdtypes[type].select_request(f, ops, se->block);
  unlock_filp(f);
  if (r != OK && r != SUSPEND) {
	se->error = EINTR;
	se->block = 0;	/* Stop blocking to return asap */
	if (!is_deferred(se)) select_cancel_all(se);
  }

  return(r);
}

/*===========================================================================*
 *				filp_status				     *
 *===========================================================================*/
static void filp_status(f, status)
struct filp *f;
int status;
{
/* Tell processes that need to know about the status of this filp */
  int fd, slot;
  struct selectentry *se;

  for (slot = 0; slot < MAXSELECTS; slot++) {
	se = &selecttab[slot];
	if (se->requestor == NULL) continue; /* empty slot */

	for (fd = 0; fd < se->nfds; fd++) {
		if (se->filps[fd] != f) continue;
		if (status < 0)
			ops2tab(SEL_RD|SEL_WR|SEL_ERR, fd, se);
		else
			ops2tab(status, fd, se);
		restart_proc(se);
	}
  }
}

/*===========================================================================*
 *				restart_proc				     *
 *===========================================================================*/
static void restart_proc(se)
struct selectentry *se;
{
/* Tell process about select results (if any) unless there are still results
 * pending. */

  if ((se->nreadyfds > 0 || !se->block) && !is_deferred(se))
	select_return(se);
}

/*===========================================================================*
 *				wipe_select				     *
 *===========================================================================*/
static void wipe_select(struct selectentry *se)
{
  se->nfds = 0;
  se->nreadyfds = 0;
  se->error = OK;
  se->block = 0;
  memset(se->filps, 0, sizeof(se->filps));

  FD_ZERO(&se->readfds);
  FD_ZERO(&se->writefds);
  FD_ZERO(&se->errorfds);
  FD_ZERO(&se->ready_readfds);
  FD_ZERO(&se->ready_writefds);
  FD_ZERO(&se->ready_errorfds);
}

/*===========================================================================*
 *				select_lock_filp			     *
 *===========================================================================*/
static void select_lock_filp(struct filp *f, int ops)
{
/* Lock a filp and vnode based on which operations are requested */
  tll_access_t locktype;;

  locktype = VNODE_READ; /* By default */

  if (ops & (SEL_WR|SEL_ERR))
	/* Selecting for error or writing requires exclusive access */
	locktype = VNODE_WRITE;

  lock_filp(f, locktype);
}
