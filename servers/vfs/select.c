/* Implement entry point to select system call.
 *
 * The entry points into this file are
 *   do_select:	       perform the SELECT system call
 *   select_callback:  notify select system of possible fd operation 
 *   select_notified:  low-level entry for device notifying select
 *   select_unsuspend_by_endpt: cancel a blocking select on exiting driver
 */

#define DEBUG_SELECT 0

#include "fs.h"
#include "select.h"
#include "file.h"
#include "vnode.h"

#include <sys/time.h>
#include <sys/select.h>
#include <minix/com.h>
#include <minix/u64.h>
#include <string.h>

/* max. number of simultaneously pending select() calls */
#define MAXSELECTS 25
#define FROM_PROC 0
#define TO_PROC   1

PRIVATE struct selectentry {
  struct fproc *requestor;	/* slot is free iff this is NULL */
  int req_endpt;
  fd_set readfds, writefds, errorfds;
  fd_set ready_readfds, ready_writefds, ready_errorfds;
  fd_set *vir_readfds, *vir_writefds, *vir_errorfds;
  struct filp *filps[OPEN_MAX];
  int type[OPEN_MAX];
  int deferred;		/* awaiting initial reply from driver */
  int deferred_fd;	/* fd awaiting initial reply from driver */
  int nfds, nreadyfds;
  char block;
  clock_t expiry;
  timer_t timer;	/* if expiry > 0 */
} selecttab[MAXSELECTS];

FORWARD _PROTOTYPE(int copy_fdsets, (struct selectentry *se, int nfds,
				      int direction)			);
FORWARD _PROTOTYPE(void filp_status, (struct filp *fp, int status)	);
FORWARD _PROTOTYPE(void restart_proc, (int slot)			);
FORWARD _PROTOTYPE(void ops2tab, (int ops, int fd, struct selectentry *e));
FORWARD _PROTOTYPE(int select_reevaluate, (struct filp *fp)		);
FORWARD _PROTOTYPE(int select_request_file, (struct filp *f, int *ops,
					     int block)			);
FORWARD _PROTOTYPE(int select_match_file, (struct filp *f)		);
FORWARD _PROTOTYPE(int select_request_general, (struct filp *f, int *ops,
					        int block)		);
FORWARD _PROTOTYPE(int select_request_asynch, (struct filp *f, int *ops,
					       int block)		);
FORWARD _PROTOTYPE(int select_major_match, (int match_major,
					    struct filp *file)		);
FORWARD _PROTOTYPE(void select_cancel_all, (struct selectentry *e)	);
FORWARD _PROTOTYPE(void select_wakeup, (struct selectentry *e, int r)	);
FORWARD _PROTOTYPE(void select_return, (struct selectentry *, int)	);
FORWARD _PROTOTYPE(void sel_restart_dev, (void)				);
FORWARD _PROTOTYPE(int tab2ops, (int fd, struct selectentry *e)		);
FORWARD _PROTOTYPE(void wipe_select, (struct selectentry *s)		);

PRIVATE struct fdtype {
	int (*select_request)(struct filp *, int *ops, int block);	
	int (*select_match)(struct filp *);
	int select_major;
} fdtypes[] = {
	{ select_request_file, select_match_file, 0 },
	{ select_request_general, NULL, TTY_MAJOR },
	{ select_request_general, NULL, INET_MAJOR },
	{ select_request_pipe, select_match_pipe, 0 },
	{ select_request_asynch, NULL, LOG_MAJOR },
};
#define SEL_FDS		(sizeof(fdtypes) / sizeof(fdtypes[0]))

/*===========================================================================*
 *				do_select				      *
 *===========================================================================*/
PUBLIC int do_select(void)
{
/* Implement the select(nfds, readfds, writefds, errorfds, timeout) system
 * call. First we copy the arguments and verify their sanity. Then we check
 * whether there are file descriptors that satisfy the select call right of the
 * bat. If so, or if there are no ready file descriptors but the process 
 * requested to return immediately, we return the result. Otherwise we set a
 * timeout and wait for either the file descriptors to become ready or the 
 * timer to go off. If no timeout value was provided, we wait indefinitely. */

  int r, nfds, do_timeout = 0, nonzero_timeout = 0, fd, s, fd_setsize;
  struct timeval timeout;
  struct selectentry *se;

  nfds = m_in.SEL_NFDS;

  /* Sane amount of file descriptors? */
  if (nfds < 0 || nfds > OPEN_MAX) return(EINVAL);
  fd_setsize = _FDSETWORDS(nfds) * _FDSETBITSPERWORD/8;

  /* Find a slot to store this select request */
  for (s = 0; s < MAXSELECTS; s++)
	if (selecttab[s].requestor == NULL) /* Unused slot */
		break;
  if (s >= MAXSELECTS) return(ENOSPC);

  se = &selecttab[s];
  wipe_select(se);	/* Clear results of previous usage.*/
  se->req_endpt = who_e;
  se->vir_readfds = (fd_set *) m_in.SEL_READFDS;
  se->vir_writefds = (fd_set *) m_in.SEL_WRITEFDS;
  se->vir_errorfds = (fd_set *) m_in.SEL_ERRORFDS;

  /* Copy fdsets from the process */
  if ((r = copy_fdsets(se, nfds, FROM_PROC)) != OK) return(r); 

  /* Did the process set a timeout value? If so, retrieve it. */
  if (m_in.SEL_TIMEOUT != NULL) {
  	do_timeout = 1;
	r = sys_vircopy(who_e, D, (vir_bytes) m_in.SEL_TIMEOUT,	SELF, D,
			(vir_bytes) &timeout, sizeof(timeout));
	if (r != OK) return(r);
  }

  /* No nonsense in the timeval. */
  if(do_timeout && (timeout.tv_sec < 0 || timeout.tv_usec < 0)) return(EINVAL);

  /* If there is no timeout, we block forever. Otherwise, we block up to the
   * specified time interval.  
   */
  if(!do_timeout)	/* No timeout value set */
	se->block = 1;
  else if (do_timeout && (timeout.tv_sec > 0 || timeout.tv_usec > 0))
  	se->block = 1;
  else			/* timeout set as (0,0) - this effects a poll */
	se->block = 0;	
  se->expiry = 0;	/* no timer set (yet) */

  /* Check all file descriptors in the set whether one is 'ready' now. */
  for (fd = 0; fd < nfds; fd++) {
	int ops, t, type = -1, r;
	struct filp *filp;
	
	if (!(ops = tab2ops(fd, se)))
		continue; /* No operations set; nothing to do for this fd */

	/* Get filp belonging to this fd */
	filp = se->filps[fd] = get_filp(fd);
	if (filp == NIL_FILP) {
		if (err_code == EBADF) {
			select_cancel_all(se);
			return(EBADF);
		}

		/* File descriptor is 'ready' to return EIO */
		printf("VFS do_select: EIO after driver failure\n");
		ops2tab(SEL_RD|SEL_WR|SEL_ERR, fd, se);
		continue;
	}

	/* Figure out what type of file we're dealing with */
	for(t = 0; t < SEL_FDS; t++) {
		if (fdtypes[t].select_match) {
			if (fdtypes[t].select_match(filp)) {
				type = t;
			}
	 	} else if (select_major_match(fdtypes[t].select_major, filp)) {
			type = t;
		}
	}

	if (type == -1)	return(EBADF);
	se->type[fd] = type;

	/* Test filp for select operations if not already done so. e.g., files
	 * sharing a filp and both doing a select on that filp. */
	if ((se->filps[fd]->filp_select_ops & ops) != ops) {
		int wantops;
		
		wantops = (se->filps[fd]->filp_select_ops |= ops);
		r = fdtypes[type].select_request(filp, &wantops, se->block);
		if (r != SEL_OK) {
			if (r == SEL_DEFERRED) {
				se->deferred = TRUE;
				se->deferred_fd = 0;
				continue;
			}
			
			/* Error or bogus return code; cancel select. */
			select_cancel_all(se);
			return(EINVAL);
		}
		
		/* The select request above might have turned on/off some
		 * operations because they were 'ready' or not meaningful.
		 * Either way, we might have a result and we need to store them
		 * in the select table entry. */
		if (wantops & ops) ops2tab(wantops, fd, se);
	} 

	se->nfds = fd+1;
	se->filps[fd]->filp_selectors++;
  }

  if (se->nreadyfds > 0 || (!se->block && !se->deferred)) {
	/* fd's were found that were ready to go right away, and/or
	 * we were instructed not to block at all. Must return
	 * immediately.
	 */
	r = copy_fdsets(se, se->nfds, TO_PROC);
	select_cancel_all(se);
	se->requestor = NULL;

	if (r != OK) return(r);
	else return(se->nreadyfds);
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
	fs_set_timer(&se->timer, ticks, select_timeout_check, s);
  }

  /* if we're blocking, the table entry is now valid. */
  se->requestor = fp;

  /* process now blocked */
  suspend(FP_BLOCKED_ON_SELECT);
  return(SUSPEND);
}


/*===========================================================================*
 *				select_request_file			     *
 *===========================================================================*/
PRIVATE int select_request_file(struct filp *f, int *ops, int block)
{
  /* output *ops is input *ops */
  return(SEL_OK);
}


/*===========================================================================*
 *				select_match_file			     *
 *===========================================================================*/
PRIVATE int select_match_file(struct filp *file)
{
  return(file && file->filp_vno && (file->filp_vno->v_mode & I_REGULAR));
}


/*===========================================================================*
 *				select_request_general			     *
 *===========================================================================*/
PRIVATE int select_request_general(struct filp *f, int *ops, int block)
{
  int rops = *ops;
  if (block) rops |= SEL_NOTIFY;
  *ops = dev_io(VFS_DEV_SELECT, f->filp_vno->v_sdev, rops, NULL,
		cvu64(0), 0, 0, FALSE);
  if (*ops < 0)
	return(SEL_ERR);

  return(SEL_OK);
}


/*===========================================================================*
 *				select_request_asynch			     *
 *===========================================================================*/
PRIVATE int select_request_asynch(struct filp *f, int *ops, int block)
{
  int r, rops;
  struct dmap *dp;

  rops = *ops;
  f->filp_select_flags |= FSF_UPDATE;
  if (block) {
	rops |= SEL_NOTIFY;
	f->filp_select_flags |= FSF_BLOCK;
  }

  if (f->filp_select_flags & FSF_BUSY)
	return(SEL_DEFERRED);

  dp = &dmap[((f->filp_vno->v_sdev) >> MAJOR) & BYTE];
  if (dp->dmap_sel_filp)
	return(SEL_DEFERRED);

  f->filp_select_flags &= ~FSF_UPDATE;
  r = dev_io(VFS_DEV_SELECT, f->filp_vno->v_sdev, rops, NULL,
	     cvu64(0), 0, 0, FALSE);
  if (r < 0 && r != SUSPEND)
	return(SEL_ERR);

  if (r != SUSPEND) 
	panic(__FILE__, "select_request_asynch: expected SUSPEND got", r);

  f->filp_count++;
  dp->dmap_sel_filp = f;
  f->filp_select_flags |= FSF_BUSY;

  return(SEL_DEFERRED);
}


/*===========================================================================*
 *				select_major_match			     *
 *===========================================================================*/
PRIVATE int select_major_match(int match_major, struct filp *file)
{
  int major;
  if (!(file && file->filp_vno &&
      (file->filp_vno->v_mode & I_TYPE) == I_CHAR_SPECIAL))
	return(0);
  major = (file->filp_vno->v_sdev >> MAJOR) & BYTE;
  if (major == match_major) return 1;
  return 0;
}


/*===========================================================================*
 *				tab2ops					     *
 *===========================================================================*/
PRIVATE int tab2ops(int fd, struct selectentry *e)
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
PRIVATE void ops2tab(int ops, int fd, struct selectentry *e)
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
PRIVATE int copy_fdsets(struct selectentry *se, int nfds, int direction)
{
  int fd_setsize, r;
  endpoint_t src_e, dst_e;
  fd_set *src_fds, *dst_fds;

  if(nfds < 0 || nfds > OPEN_MAX)
	panic(__FILE__, "select copy_fdsets: nfds wrong", nfds);

  /* Only copy back as many bits as the user expects. */
  fd_setsize = _FDSETWORDS(nfds) * _FDSETBITSPERWORD/8;

  /* Set source and destination endpoints */
  src_e = (direction == FROM_PROC) ? se->req_endpt : SELF;
  dst_e = (direction == FROM_PROC) ? SELF : se->req_endpt;

  /* read set */
  src_fds = (direction == FROM_PROC) ? se->vir_readfds : &se->ready_readfds;
  dst_fds = (direction == FROM_PROC) ? &se->readfds : se->vir_readfds;
  if (se->vir_readfds) {
	r = sys_vircopy(src_e, D, (vir_bytes) src_fds, dst_e, D,
		    (vir_bytes) dst_fds, fd_setsize);
	if (r != OK) return(r);
  }

  /* write set */
  src_fds = (direction == FROM_PROC) ? se->vir_writefds : &se->ready_writefds;
  dst_fds = (direction == FROM_PROC) ? &se->writefds : se->vir_writefds;
  if (se->vir_writefds) {
	r = sys_vircopy(src_e, D, (vir_bytes) src_fds, dst_e, D,
		    (vir_bytes) dst_fds, fd_setsize);
	if (r != OK) return(r);
  }

  /* error set */
  src_fds = (direction == FROM_PROC) ? se->vir_errorfds : &se->ready_errorfds;
  dst_fds = (direction == FROM_PROC) ? &se->errorfds : se->vir_errorfds;
  if (se->vir_errorfds) {
	r = sys_vircopy(src_e, D, (vir_bytes) src_fds, dst_e, D,
		    (vir_bytes) dst_fds, fd_setsize);
	if (r != OK) return(r);
  }

  return(OK);
}


/*===========================================================================*
 *				select_cancel_all			     *
 *===========================================================================*/
PRIVATE void select_cancel_all(struct selectentry *e)
{
	int fd;

	for(fd = 0; fd < e->nfds; fd++) {
		struct filp *fp;
		fp = e->filps[fd];
		if (!fp) {
#if DEBUG_SELECT
			printf("[ fd %d/%d NULL ] ", fd, e->nfds);
#endif
			continue;
		}
		if (fp->filp_selectors < 1) {
#if DEBUG_SELECT
			printf("select: %d selectors?!\n", fp->filp_selectors);
#endif
			continue;
		}
		fp->filp_selectors--;
		e->filps[fd] = NULL;
		select_reevaluate(fp);
	}

	if (e->expiry > 0) {
#if DEBUG_SELECT
		printf("cancelling timer %d\n", e - selecttab);
#endif
		fs_cancel_timer(&e->timer); 
		e->expiry = 0;
	}

	return;
}


/*===========================================================================*
 *				select_wakeup				     *
 *===========================================================================*/
PRIVATE void select_wakeup(struct selectentry *e, int r)
{
  revive(e->req_endpt, r);
}


/*===========================================================================*
 *				select_reevaluate			     *
 *===========================================================================*/
PRIVATE int select_reevaluate(struct filp *fp)
{
	int s, remain_ops = 0, fd, type = -1;

	if (!fp) {
		printf("fs: select: reevalute NULL fp\n");
		return 0;
	}

	for(s = 0; s < MAXSELECTS; s++) {
		if (selecttab[s].requestor != NULL) continue;
		
		for(fd = 0; fd < selecttab[s].nfds; fd++)
			if (fp == selecttab[s].filps[fd]) {
				remain_ops |= tab2ops(fd, &selecttab[s]);
				type = selecttab[s].type[fd];
			}
	}

	/* If there are any select()s open that want any operations on
	 * this fd that haven't been satisfied by this callback, then we're
	 * still in the market for it.
	 */
	fp->filp_select_ops = remain_ops;
#if DEBUG_SELECT
	printf("remaining operations on fp are %d\n", fp->filp_select_ops);
#endif

	return remain_ops;
}


/*===========================================================================*
 *				select_return				     *
 *===========================================================================*/
PRIVATE void select_return(struct selectentry *se, int r)
{
  select_cancel_all(se);
  copy_fdsets(se, se->nfds, TO_PROC); /* FIXME, return error status */
  select_wakeup(se, r ? r : se->nreadyfds);
  se->requestor = NULL;
}


/*===========================================================================*
 *				select_callback			             *
 *===========================================================================*/
PUBLIC int select_callback(struct filp *fp, int ops)
{
	int s, fd, want_ops, type;

	/* We are being notified that file pointer fp is available for
	 * operations 'ops'. We must re-register the select for
	 * operations that we are still interested in, if any.
	 */

	want_ops = 0;
	type = -1;
	for(s = 0; s < MAXSELECTS; s++) {
		int wakehim = 0;
		if (selecttab[s].requestor == NULL) continue;
		
		for(fd = 0; fd < selecttab[s].nfds; fd++) {
			if (!selecttab[s].filps[fd])
				continue;
			if (selecttab[s].filps[fd] == fp) {
				int this_want_ops;
				this_want_ops = tab2ops(fd, &selecttab[s]);
				want_ops |= this_want_ops;
				if (this_want_ops & ops) {
					/* this select() has been satisfied. */
					ops2tab(ops, fd, &selecttab[s]);
					wakehim = 1;
				}
				type = selecttab[s].type[fd];
			}
		}
		if (wakehim)
			select_return(&selecttab[s], 0);
	}

	return 0;
}


/*===========================================================================*
 *				select_notified			             *
 *===========================================================================*/
PUBLIC int select_notified(int major, int minor, int selected_ops)
{
	int s, f, t;

#if DEBUG_SELECT
	printf("select callback: %d, %d: %d\n", major, minor, selected_ops);
#endif

	for(t = 0; t < SEL_FDS; t++)
		if (!fdtypes[t].select_match && fdtypes[t].select_major == major)
		    	break;

	if (t >= SEL_FDS) {
#if DEBUG_SELECT
		printf("select callback: no fdtype found for device %d\n", major);
#endif
		return OK;
	}

	/* We have a select callback from major device no.
	 * d, which corresponds to our select type t.
	 */

	for(s = 0; s < MAXSELECTS; s++) {
		int s_minor, ops;
		if (selecttab[s].requestor == NULL) continue;
		for(f = 0; f < selecttab[s].nfds; f++) {
			if (!selecttab[s].filps[f] ||
			   !select_major_match(major, selecttab[s].filps[f]))
			   	continue;
			ops = tab2ops(f, &selecttab[s]);
			s_minor =
			(selecttab[s].filps[f]->filp_vno->v_sdev >> MINOR)
				& BYTE;
			if ((s_minor == minor) &&
				(selected_ops & ops)) {
				select_callback(selecttab[s].filps[f], (selected_ops & ops));
			}
		}
	}

	return OK;
}


/*===========================================================================*
 *				init_select  				     *
 *===========================================================================*/
PUBLIC void init_select(void)
{
	int s;

	for(s = 0; s < MAXSELECTS; s++)
		fs_init_timer(&selecttab[s].timer);
}


/*===========================================================================*
 *				select_forget			             *
 *===========================================================================*/
PUBLIC void select_forget(int proc_e)
{
	/* something has happened (e.g. signal delivered that interrupts
	 * select()). totally forget about the select().
	 */
	int s;

	for(s = 0; s < MAXSELECTS; s++) {
		if (selecttab[s].requestor != NULL &&
		    selecttab[s].req_endpt == proc_e) {
			break;
		}
	}

	if (s >= MAXSELECTS) {
#if DEBUG_SELECT
		printf("select: cancelled select() not found");
#endif
		return;
	}

	select_cancel_all(&selecttab[s]);
	selecttab[s].requestor = NULL;

	return;
}


/*===========================================================================*
 *				select_timeout_check	  	     	     *
 *===========================================================================*/
PUBLIC void select_timeout_check(timer_t *timer)
{
  int s;
  struct selectentry *se;

  s = tmr_arg(timer)->ta_int;
  if (s < 0 || s >= MAXSELECTS) {
#if DEBUG_SELECT
	printf("select: bogus slot arg to watchdog %d\n", s);
#endif
	return;
  }
  se = &selecttab[s]; /* Point to select table entry */

  if (se->requestor == NULL) {
#if DEBUG_SELECT
	printf("select: no requestor in watchdog\n");
#endif
	return;
  }

  if (se->expiry <= 0) {
#if DEBUG_SELECT
	printf("select: strange expiry value in watchdog\n", s);
#endif
	return;
  }

  se->expiry = 0;
  select_return(se, 0);

}


/*===========================================================================*
 *				select_unsuspend_by_endpt  	     	     *
 *===========================================================================*/
PUBLIC void select_unsuspend_by_endpt(endpoint_t proc_e)
{
  int fd, s, maj;

  for(s = 0; s < MAXSELECTS; s++) {
	if (selecttab[s].requestor == NULL) continue;
	  
	for(fd = 0; fd < selecttab[s].nfds; fd++) {
		if (selecttab[s].filps[fd] == NIL_FILP ||
		    selecttab[s].filps[fd]->filp_vno == NIL_VNODE) {
			continue;
		}
		
		maj = (selecttab[s].filps[fd]->filp_vno->v_sdev >> MAJOR)&BYTE;
		if (dmap_driver_match(proc_e, maj))
			select_return(&selecttab[s], EAGAIN);
	    
	}
  }
}


/*===========================================================================*
 *				select_reply1				     *
 *===========================================================================*/
PUBLIC void select_reply1()
{
	int i, s, minor, status;
	endpoint_t driver_e;
	dev_t dev;
	struct filp *fp;
	struct dmap *dp;
	struct vnode *vp;

	driver_e= m_in.m_source;
	minor= m_in.DEV_MINOR;
	status= m_in.DEV_SEL_OPS;

	/* Locate dmap entry */
	for (i= 0, dp= dmap; i<NR_DEVICES; i++, dp++)
	{
		if (dp->dmap_driver == driver_e)
			break;
	}
	if (i >= NR_DEVICES)
	{
		printf("select_reply1: proc %d is not a recoqnized driver\n",
			driver_e);
		return;
	}
	dev= (i << MAJOR) | (minor & BYTE);

	fp= dp->dmap_sel_filp;
	if (!fp)
	{
		printf("select_reply1: strange, no dmap_sel_filp\n");
		return;
	}

	if (!(fp->filp_select_flags & FSF_BUSY))
		panic(__FILE__, "select_reply1: strange, not FSF_BUSY", NO_NUM);

	vp= fp->filp_vno;
	if (!vp)
		panic(__FILE__, "select_reply1: FSF_BUSY but no vp", NO_NUM);

	if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL)
	{
		panic(__FILE__, "select_reply1: FSF_BUSY but not char special",
			NO_NUM);
	}

	if (vp->v_sdev != dev)
	{
		printf("select_reply1: strange, reply from wrong dev\n");
		return;
	}

	dp->dmap_sel_filp= NULL;
	fp->filp_select_flags &= ~FSF_BUSY;
	if (!(fp->filp_select_flags & (FSF_UPDATE|FSF_BLOCK)))
		fp->filp_select_ops= 0;
	if (status != 0)
	{
		if (status > 0)
		{
			/* Clear the replied bits from the request mask unless
			 * FSF_UPDATE is set.
			 */
			if (!(fp->filp_select_flags & FSF_UPDATE))
				fp->filp_select_ops &= ~status;
		}
		filp_status(fp, status);
	}
	if (fp->filp_count > 1)
		fp->filp_count--;
	else
	{
		if (fp->filp_count != 1)
		{
			panic(__FILE__, "select_reply1: bad filp_count",
				fp->filp_count);
		}
		close_filp(fp);
	}
	sel_restart_dev();
}


/*===========================================================================*
 *				select_reply2				     *
 *===========================================================================*/
PUBLIC void select_reply2()
{
	int i, s, minor, status;
	endpoint_t driver_e;
	dev_t dev;
	struct filp *fp;
	struct dmap *dp;
	struct vnode *vp;

	driver_e= m_in.m_source;
	minor= m_in.DEV_MINOR;
	status= m_in.DEV_SEL_OPS;

	/* Locate dmap entry */
	for (i= 0, dp= dmap; i<NR_DEVICES; i++, dp++)
	{
		if (dp->dmap_driver == driver_e)
			break;
	}
	if (i >= NR_DEVICES)
	{
		printf("select_reply2: proc %d is not a recognized driver\n",
			driver_e);
		return;
	}
	dev= (i << MAJOR) | (minor & BYTE);

	/* Find filedescriptors for this device */
	for (s= 0; s<MAXSELECTS; s++)
	{
		if (selecttab[s].requestor == NULL)
			continue;	/* empty slot */
					 
		for (i= 0; i<OPEN_MAX; i++)
		{
			fp= selecttab[s].filps[i];
			if (!fp)
				continue;
			vp= fp->filp_vno;
			if (!vp)
				continue;
			if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL)
				continue;

			if (vp->v_sdev != dev)
				continue;

			if (status < 0)
			{
				printf("select_reply2: should handle error\n");
			}
			else 
			{
				/* Clear the replied bits from the request
				 * mask unless FSF_UPDATE is set.
				 */
				if (!(fp->filp_select_flags & FSF_UPDATE))
					fp->filp_select_ops &= ~status;
				ops2tab(status, i, &selecttab[s]);
			}
		}
		if (selecttab[s].nreadyfds > 0)
			restart_proc(s);
	}
}


PRIVATE void sel_restart_dev()
{
	int i, s;
	struct filp *fp;
	struct vnode *vp;
	struct dmap *dp;

	/* Locate filps that can be restarted */
	for (s= 0; s<MAXSELECTS; s++)
	{
		if (selecttab[s].requestor == NULL)
			continue; /* empty slot */
		if (!selecttab[s].deferred)
			continue;	/* process is not waiting for an
					 * initial reply.
					 */
		for (i= 0; i<OPEN_MAX; i++)
		{
			fp= selecttab[s].filps[i];
			if (!fp)
				continue;
			if (fp->filp_select_flags & FSF_BUSY)
				continue;
			if (!(fp->filp_select_flags & FSF_UPDATE))
				continue;

			vp= fp->filp_vno;
			if (!vp)
			{
				panic(__FILE__,
					"sel_restart_dev: FSF_UPDATE but no vp",
					NO_NUM);
			}
			if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL)
			{
				panic(__FILE__,
			"sel_restart_dev: FSF_UPDATE but not char special",
					NO_NUM);
			}

			dp = &dmap[((vp->v_sdev) >> MAJOR) & BYTE];
			if (dp->dmap_sel_filp)
				continue;

			printf(
			"sel_restart_dev: should consider fd %d in slot %d\n",
				i, s);
		}
	}
}


PRIVATE void filp_status(fp, status)
struct filp *fp;
int status;
{
	int i, s;

	/* Locate processes that need to know about this result */
	for (s= 0; s<MAXSELECTS; s++)
	{
		if (selecttab[s].requestor == NULL) continue; /* empty slot */

		for (i= 0; i<OPEN_MAX; i++)
		{
			if (selecttab[s].filps[i] != fp)
				continue;

			if (status < 0)
			{
				printf("filp_status: should handle error\n");
			}
			else 
				ops2tab(status, i, &selecttab[s]);

			restart_proc(s);
		}
	}
}


PRIVATE void restart_proc(slot)
int slot;
{
	int fd;
	struct selectentry *se;
	struct filp *fp;

	se= &selecttab[slot];
	if (se->deferred)
	{
		for (fd= se->deferred_fd; fd < OPEN_MAX; fd++)
		{
			fp= se->filps[fd];
			if (!fp)
				continue;
			if (fp->filp_select_flags & (FSF_UPDATE|FSF_BUSY))
				break;
		}
		if (fd < OPEN_MAX)
		{
			se->deferred_fd= fd;
			return;
		}
		se->deferred= FALSE;
	}
	if (se->nreadyfds > 0 || !se->block) {
		copy_fdsets(se, se->nfds, TO_PROC); /* FIXME, return error */
		select_wakeup(se, se->nreadyfds);
		se->requestor = NULL;
	}
}

/*===========================================================================*
 *				wipe_select				     *
 *===========================================================================*/
PRIVATE void wipe_select(struct selectentry *se)
{
  se->deferred = FALSE;
  se->nfds = 0;
  se->nreadyfds = 0;
/*  memset(se->filps, 0, OPEN_MAX * sizeof(struct filp *)); */
  memset(se->filps, 0, sizeof(se->filps)); 

  FD_ZERO(&se->readfds);
  FD_ZERO(&se->writefds);
  FD_ZERO(&se->errorfds);
  FD_ZERO(&se->ready_readfds);
  FD_ZERO(&se->ready_writefds);
  FD_ZERO(&se->ready_errorfds);
}

