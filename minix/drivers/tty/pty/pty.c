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
 * etc.) using the pty_slave_read() and pty_slave_write() functions as the
 * "keyboard" and "screen" functions of the ttypX devices.
 * Be careful when reading this code, the terms "reading" and "writing" are
 * used both for the tty (slave) and the pty (master) end of the pseudo tty.
 * Writes to one end are to be read at the other end and vice-versa.
 */

#include <minix/drivers.h>
#include <termios.h>
#include <assert.h>
#include <sys/termios.h>
#include <signal.h>
#include "tty.h"

/* PTY bookkeeping structure, one per pty/tty pair. */
typedef struct pty {
  tty_t		*tty;		/* associated TTY structure */
  char		state;		/* flags: busy, closed, ... */

  /* Read call on master (/dev/ptypX). */
  endpoint_t	rdcaller;	/* process making the call, or NONE if none */
  cdev_id_t	rdid;		/* ID of suspended read request */
  cp_grant_id_t	rdgrant;	/* grant for reader's address space */
  size_t	rdleft;		/* # bytes yet to be read */
  size_t	rdcum;		/* # bytes written so far */

  /* Write call to master (/dev/ptypX). */
  endpoint_t	wrcaller;	/* process making the call, or NONE if none*/
  cdev_id_t	wrid;		/* ID of suspended write request */
  cp_grant_id_t	wrgrant;	/* grant for writer's address space */
  size_t	wrleft;		/* # bytes yet to be written */
  size_t	wrcum;		/* # bytes written so far */

  /* Output buffer. */
  int		ocount;		/* # characters in the buffer */
  char		*ohead, *otail;	/* head and tail of the circular buffer */
  char		obuf[TTY_OUT_BYTES];
				/* buffer for bytes going to the pty reader */

  /* select() data. */
  unsigned int	select_ops;	/* Which operations do we want to know about? */
  endpoint_t	select_proc;	/* Who wants to know about it? */
} pty_t;

#define TTY_ACTIVE	0x01	/* tty is open/active */
#define PTY_ACTIVE	0x02	/* pty is open/active */
#define TTY_CLOSED	0x04	/* tty side has closed down */
#define PTY_CLOSED	0x08	/* pty side has closed down */

static pty_t pty_table[NR_PTYS];	/* PTY bookkeeping */

static void pty_start(pty_t *pp);
static void pty_finish(pty_t *pp);

static int pty_master_open(devminor_t minor, int access,
	endpoint_t user_endpt);
static int pty_master_close(devminor_t minor);
static ssize_t pty_master_read(devminor_t minor, u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id);
static ssize_t pty_master_write(devminor_t minor, u64_t position,
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id);
static int pty_master_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int pty_master_select(devminor_t minor, unsigned int ops,
	endpoint_t endpt);

static struct chardriver pty_master_tab = {
  .cdr_open	= pty_master_open,
  .cdr_close	= pty_master_close,
  .cdr_read	= pty_master_read,
  .cdr_write	= pty_master_write,
  .cdr_cancel	= pty_master_cancel,
  .cdr_select	= pty_master_select
};

/*===========================================================================*
 *				pty_master_open				     *
 *===========================================================================*/
static int pty_master_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
  tty_t *tp;
  pty_t *pp;

  assert(minor >= PTYPX_MINOR && minor < PTYPX_MINOR + NR_PTYS);

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  if (pp->state & PTY_ACTIVE)
	return EIO;

  pp->state |= PTY_ACTIVE;
  pp->rdcum = 0;
  pp->wrcum = 0;

  return OK;
}

/*===========================================================================*
 *				pty_master_close			     *
 *===========================================================================*/
static int pty_master_close(devminor_t minor)
{
  tty_t *tp;
  pty_t *pp;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  if ((pp->state & (TTY_ACTIVE | TTY_CLOSED)) != TTY_ACTIVE) {
	pp->state = 0;
  } else {
	pp->state |= PTY_CLOSED;
	tp->tty_termios.c_ospeed = B0; /* cause EOF on slave side */
	sigchar(tp, SIGHUP, 1);
  }

  return OK;
}

/*===========================================================================*
 *				pty_master_read				     *
 *===========================================================================*/
static ssize_t pty_master_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
  tty_t *tp;
  pty_t *pp;
  ssize_t r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  /* Check, store information on the reader, do I/O. */
  if (pp->state & TTY_CLOSED)
	return 0; /* EOF */

  if (pp->rdcaller != NONE || pp->rdleft != 0 || pp->rdcum != 0)
	return EIO;

  if (size <= 0)
	return EINVAL;

  pp->rdcaller = endpt;
  pp->rdid = id;
  pp->rdgrant = grant;
  pp->rdleft = size;
  pty_start(pp);

  handle_events(tp);

  if (pp->rdleft == 0) {
	pp->rdcaller = NONE;
	return EDONTREPLY;		/* already done */
  }

  if (flags & CDEV_NONBLOCK) {
	r = pp->rdcum > 0 ? pp->rdcum : EAGAIN;
	pp->rdleft = pp->rdcum = 0;
	pp->rdcaller = NONE;
	return r;
  }

  return EDONTREPLY;			/* do suspend */
}

/*===========================================================================*
 *				pty_master_write			     *
 *===========================================================================*/
static ssize_t pty_master_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
  tty_t *tp;
  pty_t *pp;
  ssize_t r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  /* Check, store information on the writer, do I/O. */
  if (pp->state & TTY_CLOSED)
	return EIO;

  if (pp->wrcaller != NONE || pp->wrleft != 0 || pp->wrcum != 0)
	return EIO;

  if (size <= 0)
	return EINVAL;

  pp->wrcaller = endpt;
  pp->wrid = id;
  pp->wrgrant = grant;
  pp->wrleft = size;

  handle_events(tp);

  if (pp->wrleft == 0) {
	pp->wrcaller = NONE;
	return EDONTREPLY;		/* already done */
  }

  if (flags & CDEV_NONBLOCK) {
	r = pp->wrcum > 0 ? pp->wrcum : EAGAIN;
	pp->wrleft = pp->wrcum = 0;
	pp->wrcaller = NONE;
	return r;
  }

  return EDONTREPLY;			/* do suspend */
}

/*===========================================================================*
 *				pty_master_cancel			     *
 *===========================================================================*/
static int pty_master_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
  tty_t *tp;
  pty_t *pp;
  int r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  if (pp->rdcaller == endpt && pp->rdid == id) {
	/* Cancel a read from a PTY. */
	r = pp->rdcum > 0 ? pp->rdcum : EINTR;
	pp->rdleft = pp->rdcum = 0;
	pp->rdcaller = NONE;
	return r;
  }

  if (pp->wrcaller == endpt && pp->wrid == id) {
	/* Cancel a write to a PTY. */
	r = pp->wrcum > 0 ? pp->wrcum : EINTR;
	pp->wrleft = pp->wrcum = 0;
	pp->wrcaller = NONE;
	return r;
  }

  /* Request not found. */
  return EDONTREPLY;
}

/*===========================================================================*
 *				select_try_pty				     *
 *===========================================================================*/
static int select_try_pty(tty_t *tp, int ops)
{
  pty_t *pp = tp->tty_priv;
  int r = 0;

  if (ops & CDEV_OP_WR)  {
	/* Write won't block on error. */
	if (pp->state & TTY_CLOSED) r |= CDEV_OP_WR;
	else if (pp->wrleft != 0 || pp->wrcum != 0) r |= CDEV_OP_WR;
	else if (tp->tty_incount < buflen(tp->tty_inbuf)) r |= CDEV_OP_WR;
  }

  if (ops & CDEV_OP_RD) {
	/* Read won't block on error. */
	if (pp->state & TTY_CLOSED) r |= CDEV_OP_RD;
	else if (pp->rdleft != 0 || pp->rdcum != 0) r |= CDEV_OP_RD;
	else if (pp->ocount > 0) r |= CDEV_OP_RD;	/* Actual data. */
  }

  return r;
}

/*===========================================================================*
 *				select_retry_pty			     *
 *===========================================================================*/
void select_retry_pty(tty_t *tp)
{
  pty_t *pp = tp->tty_priv;
  devminor_t minor;
  int r;

  /* See if the pty side of a pty is ready to return a select. */
  if (pp->select_ops && (r = select_try_pty(tp, pp->select_ops))) {
	minor = PTYPX_MINOR + (int) (pp - pty_table);
	chardriver_reply_select(pp->select_proc, minor, r);
	pp->select_ops &= ~r;
  }
}

/*===========================================================================*
 *				pty_master_select			     *
 *===========================================================================*/
static int pty_master_select(devminor_t minor, unsigned int ops,
	endpoint_t endpt)
{
  tty_t *tp;
  pty_t *pp;
  int ready_ops, watch;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  watch = (ops & CDEV_NOTIFY);
  ops &= (CDEV_OP_RD | CDEV_OP_WR | CDEV_OP_ERR);

  ready_ops = select_try_pty(tp, ops);

  ops &= ~ready_ops;
  if (ops && watch) {
	pp->select_ops |= ops;
	pp->select_proc = endpt;
  }

  return ready_ops;
}

/*===========================================================================*
 *				do_pty					     *
 *===========================================================================*/
void do_pty(message *m_ptr, int ipc_status)
{
/* Process a request for a PTY master (/dev/ptypX) device. */

  chardriver_process(&pty_master_tab, m_ptr, ipc_status);
}

/*===========================================================================*
 *				pty_slave_write				     *
 *===========================================================================*/
static int pty_slave_write(tty_t *tp, int try)
{
/* (*dev_write)() routine for PTYs.  Transfer bytes from the writer on
 * /dev/ttypX to the output buffer.
 */
  pty_t *pp = tp->tty_priv;
  int count, ocount, s;

  /* PTY closed down? */
  if (pp->state & PTY_CLOSED) {
  	if (try) return 1;
	if (tp->tty_outleft > 0) {
		chardriver_reply_task(tp->tty_outcaller, tp->tty_outid, EIO);
		tp->tty_outleft = tp->tty_outcum = 0;
		tp->tty_outcaller = NONE;
	}
	return 0;
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
	if (tp->tty_outcaller == KERNEL) {
		/* We're trying to print on kernel's behalf */
		memcpy(pp->ohead, (void *) tp->tty_outgrant + tp->tty_outcum,
			count);
	} else {
		if ((s = sys_safecopyfrom(tp->tty_outcaller, tp->tty_outgrant,
				tp->tty_outcum, (vir_bytes) pp->ohead,
				count)) != OK) {
			break;
		}
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

	tp->tty_outcum += count;
	if ((tp->tty_outleft -= count) == 0) {
		/* Output is finished, reply to the writer. */
		chardriver_reply_task(tp->tty_outcaller, tp->tty_outid,
			tp->tty_outcum);
		tp->tty_outcum = 0;
		tp->tty_outcaller = NONE;
	}
  }
  pty_finish(pp);
  return 1;
}

/*===========================================================================*
 *				pty_slave_echo				     *
 *===========================================================================*/
static void pty_slave_echo(tty_t *tp, int c)
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
static void pty_start(pty_t *pp)
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
	if((s = sys_safecopyto(pp->rdcaller, pp->rdgrant, pp->rdcum,
		(vir_bytes) pp->otail, count)) != OK) {
		break;
 	}

	/* Bookkeeping. */
	pp->ocount -= count;
	if ((pp->otail += count) == bufend(pp->obuf)) pp->otail = pp->obuf;
	pp->rdcum += count;
	pp->rdleft -= count;
  }
}

/*===========================================================================*
 *				pty_finish				     *
 *===========================================================================*/
static void pty_finish(pty_t *pp)
{
/* Finish the read request of a PTY reader if there is at least one byte
 * transferred.
 */

  if (pp->rdcum > 0) {
	chardriver_reply_task(pp->rdcaller, pp->rdid, pp->rdcum);
	pp->rdleft = pp->rdcum = 0;
	pp->rdcaller = NONE;
  }
}

/*===========================================================================*
 *				pty_slave_read				     *
 *===========================================================================*/
static int pty_slave_read(tty_t *tp, int try)
{
/* Offer bytes from the PTY writer for input on the TTY.  (Do it one byte at
 * a time, 99% of the writes will be for one byte, so no sense in being smart.)
 */
  pty_t *pp = tp->tty_priv;
  char c;

  if (pp->state & PTY_CLOSED) {
	if (try) return 1;
	if (tp->tty_inleft > 0) {
		chardriver_reply_task(tp->tty_incaller, tp->tty_inid,
			tp->tty_incum);
		tp->tty_inleft = tp->tty_incum = 0;
		tp->tty_incaller = NONE;
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
	if ((s = sys_safecopyfrom(pp->wrcaller, pp->wrgrant, pp->wrcum,
		(vir_bytes) &c, 1)) != OK) {
		printf("pty: safecopy failed (error %d)\n", s);
		break;
	}

	/* Input processing. */
	if (in_process(tp, &c, 1) == 0) break;

	/* PTY writer bookkeeping. */
	pp->wrcum++;
	if (--pp->wrleft == 0) {
		chardriver_reply_task(pp->wrcaller, pp->wrid, pp->wrcum);
		pp->wrcum = 0;
		pp->wrcaller = NONE;
	}
  }

  return 0;
}

/*===========================================================================*
 *				pty_slave_open				     *
 *===========================================================================*/
static int pty_slave_open(tty_t *tp, int UNUSED(try))
{
/* The tty side has been opened. */
  pty_t *pp = tp->tty_priv;

  assert(tp->tty_minor >= TTYPX_MINOR && tp->tty_minor < TTYPX_MINOR + NR_PTYS);

  /* TTY_ACTIVE may already be set, which would indicate that the slave is
   * reopened after being fully closed while the master is still open. In that
   * case TTY_CLOSED will also be set, so clear that one.
   */
  pp->state |= TTY_ACTIVE;
  pp->state &= ~TTY_CLOSED;

  return 0;
}

/*===========================================================================*
 *				pty_slave_close				     *
 *===========================================================================*/
static int pty_slave_close(tty_t *tp, int UNUSED(try))
{
/* The tty side has closed, so shut down the pty side. */
  pty_t *pp = tp->tty_priv;

  if (!(pp->state & PTY_ACTIVE)) return 0;

  if (pp->rdleft > 0) {
	chardriver_reply_task(pp->rdcaller, pp->rdid, pp->rdcum);
	pp->rdleft = pp->rdcum = 0;
	pp->rdcaller = NONE;
  }

  if (pp->wrleft > 0) {
	chardriver_reply_task(pp->wrcaller, pp->wrid, pp->wrcum);
	pp->wrleft = pp->wrcum = 0;
	pp->wrcaller = NONE;
  }

  if (pp->state & PTY_CLOSED) pp->state = 0;
  else pp->state |= TTY_CLOSED;

  return 0;
}

/*===========================================================================*
 *				pty_slave_icancel			     *
 *===========================================================================*/
static int pty_slave_icancel(tty_t *tp, int UNUSED(try))
{
/* Discard waiting input. */
  pty_t *pp = tp->tty_priv;

  if (pp->wrleft > 0) {
	chardriver_reply_task(pp->wrcaller, pp->wrid, pp->wrcum + pp->wrleft);
	pp->wrcum = pp->wrleft = 0;
	pp->wrcaller = NONE;
  }

  return 0;
}

/*===========================================================================*
 *				pty_slave_ocancel			     *
 *===========================================================================*/
static int pty_slave_ocancel(tty_t *tp, int UNUSED(try))
{
/* Drain the output buffer. */
  pty_t *pp = tp->tty_priv;

  pp->ocount = 0;
  pp->otail = pp->ohead;

  return 0;
}

/*===========================================================================*
 *				pty_init				     *
 *===========================================================================*/
void pty_init(tty_t *tp)
{
  pty_t *pp;
  int line;

  /* Associate PTY and TTY structures. */
  line = tp - tty_table;
  pp = tp->tty_priv = &pty_table[line];
  pp->tty = tp;
  pp->select_ops = 0;
  pp->rdcaller = NONE;
  pp->wrcaller = NONE;

  /* Set up output queue. */
  pp->ohead = pp->otail = pp->obuf;

  /* Fill in TTY function hooks. */
  tp->tty_devread = pty_slave_read;
  tp->tty_devwrite = pty_slave_write;
  tp->tty_echo = pty_slave_echo;
  tp->tty_icancel = pty_slave_icancel;
  tp->tty_ocancel = pty_slave_ocancel;
  tp->tty_open = pty_slave_open;
  tp->tty_close = pty_slave_close;
  tp->tty_select_ops = 0;
}
