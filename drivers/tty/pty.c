/*	pty.c - pseudo terminal driver			Author: Kees J. Bot
 *								30 Dec 1995
 * PTYs can be seen as a bidirectional pipe with TTY
 * input and output processing.  For example a simple rlogin session:
 *
 *	keyboard -> rlogin -> in.rld -> /dev/ptypX -> /dev/ttypX -> shell
 *	shell -> /dev/ttypX -> /dev/ptypX -> in.rld -> rlogin -> screen
 *
 * This file takes care of copying data between the tty/pty device pairs and
 * the open/read/write/close calls on the pty devices.  The TTY task takes
 * care of the input and output processing (interrupt, backspace, raw I/O,
 * etc.) using the pty_read() and pty_write() functions as the "keyboard" and
 * "screen" functions of the ttypX devices.
 * Be careful when reading this code, the terms "reading" and "writing" are
 * used both for the tty and the pty end of the pseudo tty.  Writes to one
 * end are to be read at the other end and vice-versa.
 */

#include "../drivers.h"
#include <assert.h>
#include <termios.h>
#include <signal.h>
#include <minix/com.h>
#include <minix/callnr.h>
#include <sys/select.h>
#include "tty.h"

#if NR_PTYS > 0

/* PTY bookkeeping structure, one per pty/tty pair. */
typedef struct pty {
  tty_t		*tty;		/* associated TTY structure */
  char		state;		/* flags: busy, closed, ... */

  /* Read call on /dev/ptypX. */
  char		rdsendreply;	/* send a reply (instead of notify) */
  char		rdcaller;	/* process making the call (usually FS) */
  char		rdproc;		/* process that wants to read from the pty */
  vir_bytes	rdvir;		/* virtual address in readers address space */
  int		rdleft;		/* # bytes yet to be read */
  int		rdcum;		/* # bytes written so far */

  /* Write call to /dev/ptypX. */
  char		wrsendreply;	/* send a reply (instead of notify) */
  char		wrcaller;	/* process making the call (usually FS) */
  char		wrproc;		/* process that wants to write to the pty */
  vir_bytes	wrvir;		/* virtual address in writers address space */
  int		wrleft;		/* # bytes yet to be written */
  int		wrcum;		/* # bytes written so far */

  /* Output buffer. */
  int		ocount;		/* # characters in the buffer */
  char		*ohead, *otail;	/* head and tail of the circular buffer */
  char		obuf[128];	/* buffer for bytes going to the pty reader */

  /* select() data. */
  int		select_ops,	/* Which operations do we want to know about? */
  		select_proc,	/* Who wants to know about it? */
  		select_ready_ops;	/* For callback. */
} pty_t;

#define PTY_ACTIVE	0x01	/* pty is open/active */
#define TTY_CLOSED	0x02	/* tty side has closed down */
#define PTY_CLOSED	0x04	/* pty side has closed down */

PRIVATE pty_t pty_table[NR_PTYS];	/* PTY bookkeeping */

FORWARD _PROTOTYPE( int pty_write, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( void pty_echo, (tty_t *tp, int c)			);
FORWARD _PROTOTYPE( void pty_start, (pty_t *pp)				);
FORWARD _PROTOTYPE( void pty_finish, (pty_t *pp)			);
FORWARD _PROTOTYPE( int pty_read, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( int pty_close, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( int pty_icancel, (tty_t *tp, int try)		);
FORWARD _PROTOTYPE( int pty_ocancel, (tty_t *tp, int try)		);
FORWARD _PROTOTYPE( int pty_select, (tty_t *tp, message *m)		);

/*===========================================================================*
 *				do_pty					     *
 *===========================================================================*/
PUBLIC void do_pty(tp, m_ptr)
tty_t *tp;
message *m_ptr;
{
/* Perform an open/close/read/write call on a /dev/ptypX device. */
  pty_t *pp = tp->tty_priv;
  int r;
  phys_bytes p;

  switch (m_ptr->m_type) {
    case DEV_READ:
	/* Check, store information on the reader, do I/O. */
	if (pp->state & TTY_CLOSED) {
		r = 0;
		break;
	}
	if (pp->rdleft != 0 || pp->rdcum != 0) {
		r = EIO;
		break;
	}
	if (m_ptr->COUNT <= 0) {
		r = EINVAL;
		break;
	}
#if DEAD_CODE
	if (numap_local(m_ptr->PROC_NR, (vir_bytes) m_ptr->ADDRESS,
							m_ptr->COUNT) == 0) {
#else
	if ((r = sys_umap(m_ptr->PROC_NR, D, (vir_bytes) m_ptr->ADDRESS,
		m_ptr->COUNT, &p)) != OK) {
#endif
		break;
	}
	pp->rdsendreply = TRUE;
	pp->rdcaller = m_ptr->m_source;
	pp->rdproc = m_ptr->PROC_NR;
	pp->rdvir = (vir_bytes) m_ptr->ADDRESS;
	pp->rdleft = m_ptr->COUNT;
	pty_start(pp);
	handle_events(tp);
	if (pp->rdleft == 0) return;			/* already done */

	if (m_ptr->TTY_FLAGS & O_NONBLOCK) {
		r = EAGAIN;				/* don't suspend */
		pp->rdleft = pp->rdcum = 0;
	} else {
		r = SUSPEND;				/* do suspend */
		pp->rdsendreply = FALSE;
	}
	break;

    case DEV_WRITE:
	/* Check, store information on the writer, do I/O. */
	if (pp->state & TTY_CLOSED) {
		r = EIO;
		break;
	}
	if (pp->wrleft != 0 || pp->wrcum != 0) {
		r = EIO;
		break;
	}
	if (m_ptr->COUNT <= 0) {
		r = EINVAL;
		break;
	}
#if DEAD_CODE
	if (numap_local(m_ptr->PROC_NR, (vir_bytes) m_ptr->ADDRESS,
							m_ptr->COUNT) == 0) {
		r = EFAULT;
#else
	if ((r = sys_umap(m_ptr->PROC_NR, D, (vir_bytes) m_ptr->ADDRESS,
		m_ptr->COUNT, &p)) != OK) {
#endif
		break;
	}
	pp->wrsendreply = TRUE;
	pp->wrcaller = m_ptr->m_source;
	pp->wrproc = m_ptr->PROC_NR;
	pp->wrvir = (vir_bytes) m_ptr->ADDRESS;
	pp->wrleft = m_ptr->COUNT;
	handle_events(tp);
	if (pp->wrleft == 0) return;			/* already done */

	if (m_ptr->TTY_FLAGS & O_NONBLOCK) {		/* don't suspend */
		r = pp->wrcum > 0 ? pp->wrcum : EAGAIN;
		pp->wrleft = pp->wrcum = 0;
	} else {
		pp->wrsendreply = FALSE;			/* do suspend */
		r = SUSPEND;
	}
	break;

    case DEV_OPEN:
	r = pp->state != 0 ? EIO : OK;
	pp->state |= PTY_ACTIVE;
	pp->rdcum = 0;
	pp->wrcum = 0;
	break;

    case DEV_CLOSE:
	r = OK;
	if (pp->state & TTY_CLOSED) {
		pp->state = 0;
	} else {
		pp->state |= PTY_CLOSED;
		sigchar(tp, SIGHUP);
	}
	break;

    case DEV_SELECT:
    	r = pty_select(tp, m_ptr);
    	break;

    case CANCEL:
	if (m_ptr->PROC_NR == pp->rdproc) {
		/* Cancel a read from a PTY. */
		pp->rdleft = pp->rdcum = 0;
	}
	if (m_ptr->PROC_NR == pp->wrproc) {
		/* Cancel a write to a PTY. */
		pp->wrleft = pp->wrcum = 0;
	}
	r = EINTR;
	break;

    default:
	r = EINVAL;
  }
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, r);
}

/*===========================================================================*
 *				pty_write				     *
 *===========================================================================*/
PRIVATE int pty_write(tp, try)
tty_t *tp;
int try;
{
/* (*dev_write)() routine for PTYs.  Transfer bytes from the writer on
 * /dev/ttypX to the output buffer.
 */
  pty_t *pp = tp->tty_priv;
  int count, ocount, s;
  phys_bytes user_phys;

  /* PTY closed down? */
  if (pp->state & PTY_CLOSED) {
  	if (try) return 1;
	if (tp->tty_outleft > 0) {
		tty_reply(tp->tty_outrepcode, tp->tty_outcaller,
							tp->tty_outproc, EIO);
		tp->tty_outleft = tp->tty_outcum = 0;
	}
	return;
  }

  /* While there is something to do. */
  for (;;) {
	ocount = buflen(pp->obuf) - pp->ocount;
	if (try) return (ocount > 0);
	count = bufend(pp->obuf) - pp->ohead;
	if (count > ocount) count = ocount;
	if (count > tp->tty_outleft) count = tp->tty_outleft;
	if (count == 0 || tp->tty_inhibited)
		break;

	/* Copy from user space to the PTY output buffer. */
	if ((s = sys_vircopy(tp->tty_outproc, D, (vir_bytes) tp->tty_out_vir,
		SELF, D, (vir_bytes) pp->ohead, (phys_bytes) count)) != OK) {
		printf("pty tty%d: copy failed (error %d)\n",  s);
		break;
	}

	/* Perform output processing on the output buffer. */
	out_process(tp, pp->obuf, pp->ohead, bufend(pp->obuf), &count, &ocount);
	if (count == 0) break;

	/* Assume echoing messed up by output. */
	tp->tty_reprint = TRUE;

	/* Bookkeeping. */
	pp->ocount += ocount;
	if ((pp->ohead += ocount) >= bufend(pp->obuf))
		pp->ohead -= buflen(pp->obuf);
	pty_start(pp);
	tp->tty_out_vir += count;
	tp->tty_outcum += count;
	if ((tp->tty_outleft -= count) == 0) {
		/* Output is finished, reply to the writer. */
		tty_reply(tp->tty_outrepcode, tp->tty_outcaller,
					tp->tty_outproc, tp->tty_outcum);
		tp->tty_outcum = 0;
	}
  }
  pty_finish(pp);
  return 1;
}

/*===========================================================================*
 *				pty_echo				     *
 *===========================================================================*/
PRIVATE void pty_echo(tp, c)
tty_t *tp;
int c;
{
/* Echo one character.  (Like pty_write, but only one character, optionally.) */

  pty_t *pp = tp->tty_priv;
  int count, ocount;

  ocount = buflen(pp->obuf) - pp->ocount;
  if (ocount == 0) return;		/* output buffer full */
  count = 1;
  *pp->ohead = c;			/* add one character */

  out_process(tp, pp->obuf, pp->ohead, bufend(pp->obuf), &count, &ocount);
  if (count == 0) return;

  pp->ocount += ocount;
  if ((pp->ohead += ocount) >= bufend(pp->obuf)) pp->ohead -= buflen(pp->obuf);
  pty_start(pp);
}

/*===========================================================================*
 *				pty_start				     *
 *===========================================================================*/
PRIVATE void pty_start(pp)
pty_t *pp;
{
/* Transfer bytes written to the output buffer to the PTY reader. */
  int count;

  /* While there are things to do. */
  for (;;) {
  	int s;
	count = bufend(pp->obuf) - pp->otail;
	if (count > pp->ocount) count = pp->ocount;
	if (count > pp->rdleft) count = pp->rdleft;
	if (count == 0) break;

	/* Copy from the output buffer to the readers address space. */
	if ((s = sys_vircopy(SELF, D, (vir_bytes)pp->otail,
		(vir_bytes) pp->rdproc, D, (vir_bytes) pp->rdvir, (phys_bytes) count)) != OK) {
		printf("pty tty%d: copy failed (error %d)\n",  s);
		break;
	}

	/* Bookkeeping. */
	pp->ocount -= count;
	if ((pp->otail += count) == bufend(pp->obuf)) pp->otail = pp->obuf;
	pp->rdvir += count;
	pp->rdcum += count;
	pp->rdleft -= count;
  }
}

/*===========================================================================*
 *				pty_finish				     *
 *===========================================================================*/
PRIVATE void pty_finish(pp)
pty_t *pp;
{
/* Finish the read request of a PTY reader if there is at least one byte
 * transferred.
 */
  if (pp->rdcum > 0) {
        if (pp->rdsendreply) {
		tty_reply(TASK_REPLY, pp->rdcaller, pp->rdproc, pp->rdcum);
		pp->rdleft = pp->rdcum = 0;
	}
	else
		notify(pp->rdcaller);
  }

}

/*===========================================================================*
 *				pty_read				     *
 *===========================================================================*/
PRIVATE int pty_read(tp, try)
tty_t *tp;
int try;
{
/* Offer bytes from the PTY writer for input on the TTY.  (Do it one byte at
 * a time, 99% of the writes will be for one byte, so no sense in being smart.)
 */
  pty_t *pp = tp->tty_priv;
  char c;

  if (pp->state & PTY_CLOSED) {
	if (try) return 1;
	if (tp->tty_inleft > 0) {
		tty_reply(tp->tty_inrepcode, tp->tty_incaller, tp->tty_inproc,
								tp->tty_incum);
		tp->tty_inleft = tp->tty_incum = 0;
	}
	return 1;
  }

  if (try) {
  	if (pp->wrleft > 0)
  		return 1;
  	return 0;
  }

  while (pp->wrleft > 0) {
  	int s;

	/* Transfer one character to 'c'. */
	if ((s = sys_vircopy(pp->wrproc, D, (vir_bytes) pp->wrvir,
		SELF, D, (vir_bytes) &c, (phys_bytes) 1)) != OK) {
		printf("pty: copy failed (error %d)\n", s);
		break;
	}

	/* Input processing. */
	if (in_process(tp, &c, 1) == 0) break;

	/* PTY writer bookkeeping. */
	pp->wrvir++;
	pp->wrcum++;
	if (--pp->wrleft == 0) {
		if (pp->wrsendreply) {
			tty_reply(TASK_REPLY, pp->wrcaller, pp->wrproc,
				pp->wrcum);
			pp->wrcum = 0;
		}
		else
			notify(pp->wrcaller);
	}
  }
}

/*===========================================================================*
 *				pty_close				     *
 *===========================================================================*/
PRIVATE int pty_close(tp, try)
tty_t *tp;
int try;
{
/* The tty side has closed, so shut down the pty side. */
  pty_t *pp = tp->tty_priv;

  if (!(pp->state & PTY_ACTIVE)) return;

  if (pp->rdleft > 0) {
  	assert(!pp->rdsendreply);
  	notify(pp->rdcaller);
  }

  if (pp->wrleft > 0) {
  	assert(!pp->wrsendreply);
  	notify(pp->wrcaller);
  }

  if (pp->state & PTY_CLOSED) pp->state = 0; else pp->state |= TTY_CLOSED;
}

/*===========================================================================*
 *				pty_icancel				     *
 *===========================================================================*/
PRIVATE int pty_icancel(tp, try)
tty_t *tp;
int try;
{
/* Discard waiting input. */
  pty_t *pp = tp->tty_priv;

  if (pp->wrleft > 0) {
  	assert(!pp->wrsendreply);
  	pp->wrcum += pp->wrleft;
  	pp->wrleft= 0;
  	notify(pp->wrcaller);
  }
}

/*===========================================================================*
 *				pty_ocancel				     *
 *===========================================================================*/
PRIVATE int pty_ocancel(tp, try)
tty_t *tp;
int try;
{
/* Drain the output buffer. */
  pty_t *pp = tp->tty_priv;

  pp->ocount = 0;
  pp->otail = pp->ohead;
}

/*===========================================================================*
 *				pty_init				     *
 *===========================================================================*/
PUBLIC void pty_init(tp)
tty_t *tp;
{
  pty_t *pp;
  int line;

  /* Associate PTY and TTY structures. */
  line = tp - &tty_table[NR_CONS + NR_RS_LINES];
  pp = tp->tty_priv = &pty_table[line];
  pp->tty = tp;
  pp->select_ops = 0;

  /* Set up output queue. */
  pp->ohead = pp->otail = pp->obuf;

  /* Fill in TTY function hooks. */
  tp->tty_devread = pty_read;
  tp->tty_devwrite = pty_write;
  tp->tty_echo = pty_echo;
  tp->tty_icancel = pty_icancel;
  tp->tty_ocancel = pty_ocancel;
  tp->tty_close = pty_close;
  tp->tty_select_ops = 0;
}

/*===========================================================================*
 *				pty_status				     *
 *===========================================================================*/
PUBLIC int pty_status(message *m_ptr)
{
	int i, event_found;
	pty_t *pp;

	event_found = 0;
	for (i= 0, pp = pty_table; i<NR_PTYS; i++, pp++) {
		if ((((pp->state & TTY_CLOSED) && pp->rdleft > 0) ||
			pp->rdcum > 0) &&
			pp->rdcaller == m_ptr->m_source)
		{
			m_ptr->m_type = DEV_REVIVE;
			m_ptr->REP_PROC_NR = pp->rdproc;
			m_ptr->REP_STATUS = pp->rdcum;

			pp->rdleft = pp->rdcum = 0;
			event_found = 1;
			break;
		}

		if ((((pp->state & TTY_CLOSED) && pp->wrleft > 0) ||
			pp->wrcum > 0) &&
			pp->wrcaller == m_ptr->m_source)
		{
			m_ptr->m_type = DEV_REVIVE;
			m_ptr->REP_PROC_NR = pp->wrproc;
			if (pp->wrcum == 0)
				m_ptr->REP_STATUS = EIO;
			else
				m_ptr->REP_STATUS = pp->wrcum;

			pp->wrleft = pp->wrcum = 0;
			event_found = 1;
			break;
		}

		if (pp->select_ready_ops && pp->select_proc == m_ptr->m_source) {
			m_ptr->m_type = DEV_IO_READY;
			m_ptr->DEV_MINOR = PTYPX_MINOR + i;
			m_ptr->DEV_SEL_OPS = pp->select_ready_ops;
			pp->select_ready_ops = 0;
			event_found = 1;
			break;
		}
	}
	return event_found;
}

/*===========================================================================*
 *				select_try_pty				     *
 *===========================================================================*/
PRIVATE int select_try_pty(tty_t *tp, int ops)
{
  	pty_t *pp = tp->tty_priv;
	int r = 0;

	if (ops & SEL_WR)  {
		/* Write won't block on error. */
		if (pp->state & TTY_CLOSED) r |= SEL_WR;
		else if (pp->wrleft != 0 || pp->wrcum != 0) r |= SEL_WR;
		else r |= SEL_WR;
	}

	if (ops & SEL_RD) {
		/* Read won't block on error. */
		if (pp->state & TTY_CLOSED) r |= SEL_RD;
		else if (pp->rdleft != 0 || pp->rdcum != 0) r |= SEL_RD;
		else if (pp->ocount > 0) r |= SEL_RD;	/* Actual data. */
	}

	return r;
}

/*===========================================================================*
 *				select_retry_pty			     *
 *===========================================================================*/
PUBLIC void select_retry_pty(tty_t *tp)
{
  	pty_t *pp = tp->tty_priv;
  	int r;

	/* See if the pty side of a pty is ready to return a select. */
	if (pp->select_ops && (r=select_try_pty(tp, pp->select_ops))) {
		pp->select_ops &= ~r;
		pp->select_ready_ops |= r;
		notify(pp->select_proc);
	}
}

/*===========================================================================*
 *				pty_select				     *
 *===========================================================================*/
PRIVATE int pty_select(tty_t *tp, message *m)
{
  	pty_t *pp = tp->tty_priv;
	int ops, ready_ops = 0, watch;

	ops = m->PROC_NR & (SEL_RD|SEL_WR|SEL_ERR);
	watch = (m->PROC_NR & SEL_NOTIFY) ? 1 : 0;

	ready_ops = select_try_pty(tp, ops);

	if (!ready_ops && ops && watch) {
		pp->select_ops |= ops;
		pp->select_proc = m->m_source;
	}

	return ready_ops;
}

#endif /* NR_PTYS > 0 */
