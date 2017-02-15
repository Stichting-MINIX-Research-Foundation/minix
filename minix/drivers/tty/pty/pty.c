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
 *
 * In addition to the above, PTY service now also supports Unix98 pseudo-
 * terminal pairs, thereby allowing non-root users to allocate pseudoterminals.
 * It requires the presence for PTYFS for this, and supports only old-style
 * ptys when PTYFS is not running. For Unix98 ptys, the general idea is that a
 * userland program opens a pty master by opening /dev/ptmx through the use of
 * posxix_openpt(3). A slave node is allocated on PTYFS when the program calls
 * grantpt(3) on the master. The program can then obtain the path name for the
 * slave end through ptsname(3), and open the slave end using this path.
 *
 * Implementation-wise, the Unix98 and non-Unix98 pseudoterminals share the
 * same pool of data structures, but use different ranges of minor numbers.
 * Access to the two types may not be mixed, and thus, some parts of the code
 * have checks to make sure a traditional slave is not opened for a master
 * allocated through /dev/ptmx, etcetera.
 */

#include <minix/drivers.h>
#include <paths.h>
#include <termios.h>
#include <assert.h>
#include <sys/termios.h>
#include <signal.h>
#include "tty.h"
#include "ptyfs.h"

/* Device node attributes used for Unix98 slave nodes. */
#define UNIX98_MODE		(S_IFCHR | 0620)	/* crw--w---- */

#define UNIX98_MASTER(index)	(UNIX98_MINOR + (index) * 2)
#define UNIX98_SLAVE(index)	(UNIX98_MINOR + (index) * 2 + 1)

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
  devminor_t	select_minor;	/* Which minor was being selected on? */
} pty_t;

#define TTY_ACTIVE	0x01	/* tty is open/active */
#define PTY_ACTIVE	0x02	/* pty is open/active */
#define TTY_CLOSED	0x04	/* tty side has closed down */
#define PTY_CLOSED	0x08	/* pty side has closed down */
#define PTY_UNIX98	0x10	/* pty pair is Unix98 */
#define PTY_PKTMODE	0x20	/* pty side is in packet mode (TIOCPKT) */

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
static int pty_master_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t user_endpt, cdev_id_t id);
static int pty_master_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int pty_master_select(devminor_t minor, unsigned int ops,
	endpoint_t endpt);

static struct chardriver pty_master_tab = {
  .cdr_open	= pty_master_open,
  .cdr_close	= pty_master_close,
  .cdr_read	= pty_master_read,
  .cdr_write	= pty_master_write,
  .cdr_ioctl	= pty_master_ioctl,
  .cdr_cancel	= pty_master_cancel,
  .cdr_select	= pty_master_select
};

/*===========================================================================*
 *				get_free_pty				     *
 *===========================================================================*/
static tty_t *get_free_pty(void)
{
/* Return a pointer to a free tty structure, or NULL if no tty is free. */
  tty_t *tp;
  pty_t *pp;

  for (tp = &tty_table[0]; tp < &tty_table[NR_PTYS]; tp++) {
	pp = tp->tty_priv;

	if (!(pp->state & (PTY_ACTIVE | TTY_ACTIVE)))
		return tp;
  }

  return NULL;
}

/*===========================================================================*
 *				pty_master_open				     *
 *===========================================================================*/
static int pty_master_open(devminor_t minor, int UNUSED(access),
	endpoint_t UNUSED(user_endpt))
{
  tty_t *tp;
  pty_t *pp;
  int r;

  if (minor == PTMX_MINOR) {
	/* /dev/ptmx acts as a cloning device. We return a free PTY master and
	 * mark it as a UNIX98 type.
	 */
	if ((tp = get_free_pty()) == NULL)
		return EAGAIN; /* POSIX says this is the right error code */

	/* The following call has two purposes. First, we check right here
	 * whether PTYFS is running at all; if not, the PTMX device cannot be
	 * opened at all and userland can fall back to other allocation
	 * methods right away. Second, in the exceptional case that the PTY
	 * service is restarted while PTYFS keeps running, PTYFS may expose
	 * stale slave nodes, which are a security hole if not removed as soon
	 * as a new PTY pair is allocated.
	 */
	if (ptyfs_clear(tp->tty_index) != OK)
		return EAGAIN;

	pp = tp->tty_priv;
	pp->state |= PTY_UNIX98;

	minor = UNIX98_MASTER(tp->tty_index);

	r = CDEV_CLONED | minor;
  } else {
	/* There is no way to open Unix98 masters directly, except by messing
	 * with mknod. We disallow such tricks altogether, and thus, the rest
	 * of the code deals with opening a non-Unix98 master only.
	 */
	if (minor < PTYPX_MINOR || minor >= PTYPX_MINOR + NR_PTYS)
		return EIO;

	if ((tp = line2tty(minor)) == NULL)
		return ENXIO;
	pp = tp->tty_priv;

	/* For non-Unix98 PTYs, we allow the slave to be opened before the
	 * master, but the master may be opened only once. This is how userland
	 * is able to find a free non-Unix98 PTY pair.
	 */
	if (pp->state & PTY_ACTIVE)
		return EIO;
	assert(!(pp->state & PTY_UNIX98));

	r = OK;
  }

  pp->state |= PTY_ACTIVE;

  pp->rdcum = 0;
  pp->wrcum = 0;

  return r;
}

/*===========================================================================*
 *				pty_reset				     *
 *===========================================================================*/
static void pty_reset(tty_t *tp)
{
/* Both sides of a PTY pair have been closed. Clean up its state. */
  pty_t *pp;

  pp = tp->tty_priv;

  /* For Unix98 pairs, clean up the Unix98 slave node. It may never have been
   * allocated, but we don't care. Ignore failures altogether.
   */
  if (pp->state & PTY_UNIX98)
	(void)ptyfs_clear(tp->tty_index);

  pp->state = 0;
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
	pty_reset(tp);
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
 *				pty_master_ioctl			     *
 *===========================================================================*/
static int pty_master_ioctl(devminor_t minor, unsigned long request,
	endpoint_t endpt, cp_grant_id_t grant, int flags,
	endpoint_t user_endpt, cdev_id_t id)
{
  tty_t *tp;
  pty_t *pp;
  uid_t uid;
  struct ptmget pm;
  size_t len;
  int r, val;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;
  pp = tp->tty_priv;

  /* Some IOCTLs are for the master side only. */
  switch (request) {
  case TIOCGRANTPT:	/* grantpt(3) */
	if (!(pp->state & PTY_UNIX98))
		break;

	if ((int)(uid = getnuid(user_endpt)) == -1)
		return EACCES;
	if (tty_gid == -1) {
		printf("PTY: no tty group ID given at startup\n");
		return EACCES;
	}

	/* Create or update the slave node. */
	if (ptyfs_set(tp->tty_index, UNIX98_MODE, uid, tty_gid,
	    makedev(PTY_MAJOR, UNIX98_SLAVE(tp->tty_index))) != OK)
		return EACCES;

	return OK;

  case TIOCPTSNAME:	/* ptsname(3) */
	if (!(pp->state & PTY_UNIX98))
		break;

	/* Since pm.sn is 16 bytes, we can have up to a million slaves. */
	memset(&pm, 0, sizeof(pm));

	strlcpy(pm.sn, _PATH_DEV_PTS, sizeof(pm.sn));
	len = strlen(pm.sn);

	if (ptyfs_name(tp->tty_index, &pm.sn[len], sizeof(pm.sn) - len) != OK)
		return EINVAL;

	return sys_safecopyto(endpt, grant, 0, (vir_bytes)&pm, sizeof(pm));

  case TIOCPKT:
	r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes)&val, sizeof(val));
	if (r != OK)
		return r;

	if (val)
		pp->state |= PTY_PKTMODE;
	else
		pp->state &= ~PTY_PKTMODE;

	return OK;
  }

  /* TODO: historically, all IOCTLs on the master are processed as if issued on
   * the slave end. Make sure that this can not cause problems, in particular
   * with blocking IOCTLs.
   */
  return tty_ioctl(minor, request, endpt, grant, flags, user_endpt, id);
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
  int r;

  /* See if the pty side of a pty is ready to return a select. */
  if (pp->select_ops && (r = select_try_pty(tp, pp->select_ops))) {
	chardriver_reply_select(pp->select_proc, pp->select_minor, r);
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
	pp->select_minor = minor;
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
  char c;

  /* While there are things to do. */
  for (;;) {
	count = bufend(pp->obuf) - pp->otail;
	if (count > pp->ocount) count = pp->ocount;
	if (count == 0 || pp->rdleft == 0) break;

	/* If there is output at all, and packet mode is enabled, then prepend
	 * the output with a zero byte. This is absolutely minimal "support"
	 * for the TIOCPKT receipt mode to get telnetd(8) going. Implementing
	 * full support for all the TIOCPKT bits will require more work.
	 */
	if (pp->rdcum == 0 && (pp->state & PTY_PKTMODE)) {
		c = 0;
		if (sys_safecopyto(pp->rdcaller, pp->rdgrant, 0, (vir_bytes)&c,
		    sizeof(c)) != OK)
			break;

		pp->rdcum++;
		pp->rdleft--;
	}

	if (count > pp->rdleft) count = pp->rdleft;
	if (count == 0) break;

	/* Copy from the output buffer to the readers address space. */
	if (sys_safecopyto(pp->rdcaller, pp->rdgrant, pp->rdcum,
	    (vir_bytes)pp->otail, count) != OK)
		break;

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
 *				pty_slave_mayopen			     *
 *===========================================================================*/
static int pty_slave_mayopen(tty_t *tp, devminor_t line)
{
/* Check if the user is not mixing Unix98 and non-Unix98 terminal ends. */
  pty_t *pp;
  int unix98_line, unix98_pty;

  pp = tp->tty_priv;

  /* A non-Unix98 slave may be opened even if the corresponding master is not
   * opened yet, but PTY_UNIX98 is always clear for free ptys.  A Unix98 slave
   * may not be opened before its master, but this should not occur anyway.
   */
  unix98_line = (line >= UNIX98_MINOR && line < UNIX98_MINOR + NR_PTYS * 2);
  unix98_pty = !!(pp->state & PTY_UNIX98);

  return (unix98_line == unix98_pty);
}

/*===========================================================================*
 *				pty_slave_open				     *
 *===========================================================================*/
static int pty_slave_open(tty_t *tp, int UNUSED(try))
{
/* The tty side has been opened. */
  pty_t *pp = tp->tty_priv;

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

  if (pp->state & PTY_CLOSED) pty_reset(tp);
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
  tp->tty_mayopen = pty_slave_mayopen;
  tp->tty_open = pty_slave_open;
  tp->tty_close = pty_slave_close;
  tp->tty_select_ops = 0;
}
