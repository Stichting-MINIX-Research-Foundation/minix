/* TTY part of PTY. */
#include <assert.h>
#include <minix/drivers.h>
#include <minix/driver.h>
#include <minix/optset.h>
#include <termios.h>
#include <sys/ttycom.h>
#include <sys/ttydefaults.h>
#include <sys/fcntl.h>
#include <signal.h>
#include "tty.h"

#include <sys/time.h>
#include <sys/select.h>

/* Address of a tty structure. */
#define tty_addr(line)	(&tty_table[line])

/* Macros for magic tty structure pointers. */
#define FIRST_TTY	tty_addr(0)
#define END_TTY		tty_addr(sizeof(tty_table) / sizeof(tty_table[0]))

/* A device exists if at least its 'devread' function is defined. */
#define tty_active(tp)	((tp)->tty_devread != NULL)

static void tty_timed_out(int arg);
static void settimer(tty_t *tty_ptr, int enable);
static void in_transfer(tty_t *tp);
static int tty_echo(tty_t *tp, int ch);
static void rawecho(tty_t *tp, int ch);
static int back_over(tty_t *tp);
static void reprint(tty_t *tp);
static void dev_ioctl(tty_t *tp);
static void setattr(tty_t *tp);
static void tty_icancel(tty_t *tp);

static int do_open(devminor_t minor, int access, endpoint_t user_endpt);
static int do_close(devminor_t minor);
static ssize_t do_read(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t do_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static int do_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id);
static int do_select(devminor_t minor, unsigned int ops, endpoint_t endpt);

static struct chardriver tty_tab = {
	.cdr_open	= do_open,
	.cdr_close	= do_close,
	.cdr_read	= do_read,
	.cdr_write	= do_write,
	.cdr_ioctl	= tty_ioctl,
	.cdr_cancel	= do_cancel,
	.cdr_select	= do_select
};

/* Default attributes. */
static struct termios termios_defaults = {
  .c_iflag = TTYDEF_IFLAG,
  .c_oflag = TTYDEF_OFLAG,
  .c_cflag = TTYDEF_CFLAG,
  .c_lflag = TTYDEF_LFLAG,
  .c_ispeed = TTYDEF_SPEED,
  .c_ospeed = TTYDEF_SPEED,
  .c_cc = {
	[VEOF] = CEOF,
	[VEOL] = CEOL,
	[VERASE] = CERASE,
	[VINTR] = CINTR,
	[VKILL] = CKILL,
	[VMIN] = CMIN,
	[VQUIT] = CQUIT,
	[VTIME] = CTIME,
	[VSUSP] = CSUSP,
	[VSTART] = CSTART,
	[VSTOP] = CSTOP,
	[VREPRINT] = CREPRINT,
	[VLNEXT] = CLNEXT,
	[VDISCARD] = CDISCARD,
	[VSTATUS] = CSTATUS
  }
};
static struct winsize winsize_defaults;	/* = all zeroes */

static const char lined[TTLINEDNAMELEN] = "termios";	/* line discipline */

/* Global variables for the TTY task (declared extern in tty.h). */
tty_t tty_table[NR_PTYS];
u32_t system_hz;
int tty_gid;

static struct optset optset_table[] = {
  { "gid",	OPT_INT,	&tty_gid,	10	},
  { NULL,	0,		NULL,		0	}
};

static void tty_startup(void);
static int tty_init(int, sef_init_info_t *);

/*===========================================================================*
 *				is_pty					     *
 *===========================================================================*/
static int is_pty(devminor_t line)
{
/* Return TRUE if the given minor device number refers to a pty (the master
 * side of a pty/tty pair), and FALSE otherwise.
 */

  if (line == PTMX_MINOR)
	return TRUE;
  if (line >= PTYPX_MINOR && line < PTYPX_MINOR + NR_PTYS)
	return TRUE;
  if (line >= UNIX98_MINOR && line < UNIX98_MINOR + NR_PTYS * 2 && !(line & 1))
	return TRUE;

  return FALSE;
}

/*===========================================================================*
 *				tty_task				     *
 *===========================================================================*/
int main(int argc, char **argv)
{
/* Main routine of the terminal task. */

  message tty_mess;		/* buffer for all incoming messages */
  int ipc_status;
  int line;
  int r;
  register tty_t *tp;

  env_setargs(argc, argv);

  tty_startup();

  while (TRUE) {
	/* Check for and handle any events on any of the ttys. */
	for (tp = FIRST_TTY; tp < END_TTY; tp++) {
		if (tp->tty_events) handle_events(tp);
	}

	/* Get a request message. */
	r= driver_receive(ANY, &tty_mess, &ipc_status);
	if (r != 0)
		panic("driver_receive failed with: %d", r);

	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(tty_mess.m_source)) {
			case CLOCK:
				/* run watchdogs of expired timers */
				expire_timers(tty_mess.m_notify.timestamp);
				break;
			default:
				/* do nothing */
				break;
		}

		/* done, get new message */
		continue;
	}

	if (!IS_CDEV_RQ(tty_mess.m_type)) {
		chardriver_process(&tty_tab, &tty_mess, ipc_status);
		continue;
	}

	/* Only device requests should get to this point.
	 * All requests have a minor device number.
	 */
	if (OK != chardriver_get_minor(&tty_mess, &line))
		continue;

	if (is_pty(line)) {
		/* Terminals and pseudo terminals belong together. We can only
		 * make a distinction between the two based on minor number and
		 * not on position in the tty_table. Hence this special case.
		 */
		do_pty(&tty_mess, ipc_status);
		continue;
	}

	/* Execute the requested device driver function. */
	chardriver_process(&tty_tab, &tty_mess, ipc_status);
  }

  return 0;
}

tty_t *
line2tty(devminor_t line)
{
/* Convert a terminal line to tty_table pointer */

	tty_t* tp;

	if ((line - TTYPX_MINOR) < NR_PTYS) {
		tp = tty_addr(line - TTYPX_MINOR);
	} else if ((line - PTYPX_MINOR) < NR_PTYS) {
		tp = tty_addr(line - PTYPX_MINOR);
	} else if ((line - UNIX98_MINOR) < NR_PTYS * 2) {
		tp = tty_addr((line - UNIX98_MINOR) >> 1);
	} else {
		tp = NULL;
	}

	return(tp);
}

/*===========================================================================*
 *			       tty_startup				     *
 *===========================================================================*/
static void tty_startup(void)
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(tty_init);
  sef_setcb_init_restart(tty_init);

  /* No signal support for now. */

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
static ssize_t do_read(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
/* A process wants to read from a terminal. */
  tty_t *tp;
  int r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  /* Check if there is already a process hanging in a read, check if the
   * parameters are correct, do I/O.
   */
  if (tp->tty_incaller != NONE || tp->tty_inleft > 0)
	return EIO;
  if (size <= 0)
	return EINVAL;

  /* Copy information from the message to the tty struct. */
  tp->tty_incaller = endpt;
  tp->tty_inid = id;
  tp->tty_ingrant = grant;
  assert(tp->tty_incum == 0);
  tp->tty_inleft = size;

  if (!(tp->tty_termios.c_lflag & ICANON) && tp->tty_termios.c_cc[VTIME] > 0) {
	if (tp->tty_termios.c_cc[VMIN] == 0) {
		/* MIN & TIME specify a read timer that finishes the
		 * read in TIME/10 seconds if no bytes are available.
		 */
		settimer(tp, TRUE);
		tp->tty_min = 1;
	} else {
		/* MIN & TIME specify an inter-byte timer that may
		 * have to be cancelled if there are no bytes yet.
		 */
		if (tp->tty_eotct == 0) {
			settimer(tp, FALSE);
			tp->tty_min = tp->tty_termios.c_cc[VMIN];
		}
	}
  }

  /* Anything waiting in the input buffer? Clear it out... */
  in_transfer(tp);
  /* ...then go back for more. */
  handle_events(tp);
  if (tp->tty_inleft == 0)
	return EDONTREPLY;	/* already done */

  /* There were no bytes in the input queue available. */
  if (flags & CDEV_NONBLOCK) {
	tty_icancel(tp);
	r = tp->tty_incum > 0 ? tp->tty_incum : EAGAIN;
	tp->tty_inleft = tp->tty_incum = 0;
	tp->tty_incaller = NONE;
	return r;
  }

  if (tp->tty_select_ops)
	select_retry(tp);

  return EDONTREPLY;		/* suspend the caller */
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
static ssize_t do_write(devminor_t minor, u64_t UNUSED(position),
	endpoint_t endpt, cp_grant_id_t grant, size_t size, int flags,
	cdev_id_t id)
{
/* A process wants to write on a terminal. */
  tty_t *tp;
  int r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  /* Check if there is already a process hanging in a write, check if the
   * parameters are correct, do I/O.
   */
  if (tp->tty_outcaller != NONE || tp->tty_outleft > 0)
	return EIO;
  if (size <= 0)
	return EINVAL;

  /* Copy message parameters to the tty structure. */
  tp->tty_outcaller = endpt;
  tp->tty_outid = id;
  tp->tty_outgrant = grant;
  assert(tp->tty_outcum == 0);
  tp->tty_outleft = size;

  /* Try to write. */
  handle_events(tp);
  if (tp->tty_outleft == 0)
	return EDONTREPLY;	/* already done */

  /* None or not all the bytes could be written. */
  if (flags & CDEV_NONBLOCK) {
	r = tp->tty_outcum > 0 ? tp->tty_outcum : EAGAIN;
	tp->tty_outleft = tp->tty_outcum = 0;
	tp->tty_outcaller = NONE;
	return r;
  }

  if (tp->tty_select_ops)
	select_retry(tp);

  return EDONTREPLY;		/* suspend the caller */
}

/*===========================================================================*
 *				tty_ioctl				     *
 *===========================================================================*/
int tty_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id)
{
/* Perform an IOCTL on this terminal. POSIX termios calls are handled
 * by the IOCTL system call.
 */
  tty_t *tp;
  int i, r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  r = OK;
  switch (request) {
    case TIOCGETA:
	/* Get the termios attributes. */
	r = sys_safecopyto(endpt, grant, 0, (vir_bytes) &tp->tty_termios,
		sizeof(struct termios));
	break;

    case TIOCSETAW:
    case TIOCSETAF:
    case TIOCDRAIN:
	if (tp->tty_outleft > 0) {
		if (flags & CDEV_NONBLOCK)
			return EAGAIN;
		/* Wait for all ongoing output processing to finish. */
		tp->tty_iocaller = endpt;
		tp->tty_ioid = id;
		tp->tty_ioreq = request;
		tp->tty_iogrant = grant;
		return EDONTREPLY;	/* suspend the caller */
	}
	if (request == TIOCDRAIN) break;
	if (request == TIOCSETAF) tty_icancel(tp);
	/*FALL THROUGH*/
    case TIOCSETA:
	/* Set the termios attributes. */
	r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &tp->tty_termios,
		sizeof(struct termios));
	if (r != OK) break;
	setattr(tp);
	break;

    case TIOCFLUSH:
	r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &i, sizeof(i));
	if (r != OK) break;
	if(i & FREAD) {	tty_icancel(tp); }
	if(i & FWRITE) { (*tp->tty_ocancel)(tp, 0); }
	break;
    case TIOCSTART:
	tp->tty_inhibited = 0;
	tp->tty_events = 1;
	break;
    case TIOCSTOP:
	tp->tty_inhibited = 1;
	tp->tty_events = 1;
	break;
    case TIOCSBRK:	/* tcsendbreak - turn break on */
	if (tp->tty_break_on != NULL) (*tp->tty_break_on)(tp,0);
	break;
    case TIOCCBRK:	/* tcsendbreak - turn break off */
	if (tp->tty_break_off != NULL) (*tp->tty_break_off)(tp,0);
	break;

    case TIOCGWINSZ:
	r = sys_safecopyto(endpt, grant, 0, (vir_bytes) &tp->tty_winsize,
		sizeof(struct winsize));
	break;

    case TIOCSWINSZ:
	r = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) &tp->tty_winsize,
		sizeof(struct winsize));
	sigchar(tp, SIGWINCH, 0);
	break;
    case TIOCGETD:	/* get line discipline */
	i = TTYDISC;
	r = sys_safecopyto(endpt, grant, 0, (vir_bytes) &i, sizeof(i));
	break;
    case TIOCSETD:	/* set line discipline */
	printf("TTY: TIOCSETD: can't set any other line discipline.\n");
	r = ENOTTY;
	break;
    case TIOCGLINED:	/* get line discipline as string */
	r = sys_safecopyto(endpt, grant, 0, (vir_bytes) lined, sizeof(lined));
	break;
    case TIOCGQSIZE:	/* get input/output queue sizes */
	/* Not sure what to report here. We are using input and output queues
	 * of different sizes, and the IOCTL allows for one single value for
	 * both. So far this seems to be just for stty(1) so let's just report
	 * the larger of the two. TODO: revisit queue sizes altogether.
	 */
	i = MAX(TTY_IN_BYTES, TTY_OUT_BYTES);
	r = sys_safecopyto(endpt, grant, 0, (vir_bytes) &i, sizeof(i));
	break;
    case TIOCSCTTY:
	/* Process sets this tty as its controlling tty */
	tp->tty_pgrp = user_endpt;
	break;
	
/* These Posix functions are allowed to fail if _POSIX_JOB_CONTROL is 
 * not defined.
 */
    case TIOCGPGRP:     
    case TIOCSPGRP:	
    default:
	r = ENOTTY;
  }

  return r;
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
static int do_open(devminor_t minor, int access, endpoint_t user_endpt)
{
/* A tty line has been opened.  Make it the callers controlling tty if
 * CDEV_NOCTTY is *not* set and it is not the log device. CDEV_CTTY is returned
 * if the tty is made the controlling tty, otherwise OK or an error code.
 */
  tty_t *tp;
  int r = OK;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  /* Make sure the user is not mixing Unix98 and old-style ends. */
  if ((*tp->tty_mayopen)(tp, minor) == FALSE)
	return EIO;

  if (!(access & CDEV_NOCTTY)) {
	tp->tty_pgrp = user_endpt;
	r = CDEV_CTTY;
  }
  tp->tty_openct++;
  if (tp->tty_openct == 1) {
	/* Tell the device that the tty is opened */
	(*tp->tty_open)(tp, 0);
  }

  return r;
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
static int do_close(devminor_t minor)
{
/* A tty line has been closed.  Clean up the line if it is the last close. */
  tty_t *tp;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  if (--tp->tty_openct == 0) {
	tp->tty_pgrp = 0;
	tty_icancel(tp);
	(*tp->tty_ocancel)(tp, 0);
	(*tp->tty_close)(tp, 0);
	tp->tty_termios = termios_defaults;
	tp->tty_winsize = winsize_defaults;
	setattr(tp);
  }

  return OK;
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
static int do_cancel(devminor_t minor, endpoint_t endpt, cdev_id_t id)
{
/* A signal has been sent to a process that is hanging trying to read or write.
 * The pending read or write must be finished off immediately.
 */
  tty_t *tp;
  int r;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  /* Check the parameters carefully, to avoid cancelling twice. */
  r = EDONTREPLY;
  if (tp->tty_inleft != 0 && endpt == tp->tty_incaller && id == tp->tty_inid) {
	/* Process was reading when killed.  Clean up input. */
	tty_icancel(tp); 
	r = tp->tty_incum > 0 ? tp->tty_incum : EAGAIN;
	tp->tty_inleft = tp->tty_incum = 0;
	tp->tty_incaller = NONE;
  } else if (tp->tty_outleft != 0 && endpt == tp->tty_outcaller &&
	id == tp->tty_outid) {
	/* Process was writing when killed.  Clean up output. */
	r = tp->tty_outcum > 0 ? tp->tty_outcum : EAGAIN;
	tp->tty_outleft = tp->tty_outcum = 0;
	tp->tty_outcaller = NONE;
  } else if (tp->tty_ioreq != 0 && endpt == tp->tty_iocaller &&
	id == tp->tty_ioid) {
	/* Process was waiting for output to drain. */
	r = EINTR;
	tp->tty_ioreq = 0;
	tp->tty_iocaller = NONE;
  }
  if (r != EDONTREPLY)
	tp->tty_events = 1;
  /* Only reply if we found a matching request. */
  return r;
}

int select_try(struct tty *tp, int ops)
{
	int ready_ops = 0;

	/* Special case. If line is hung up, no operations will block.
	 * (and it can be seen as an exceptional condition.)
	 */
	if (tp->tty_termios.c_ospeed == B0) {
		ready_ops |= ops;
	}

	if (ops & CDEV_OP_RD) {
		/* will i/o not block on read? */
		if (tp->tty_inleft > 0) {
			ready_ops |= CDEV_OP_RD; /* EIO - no blocking */
		} else if (tp->tty_incount > 0) {
			/* Is a regular read possible? tty_incount
			 * says there is data. But a read will only succeed
			 * in canonical mode if a newline has been seen.
			 */
			if (!(tp->tty_termios.c_lflag & ICANON) ||
				tp->tty_eotct > 0) {
				ready_ops |= CDEV_OP_RD;
			}
		}
	}

	if (ops & CDEV_OP_WR)  {
		if (tp->tty_outleft > 0)  ready_ops |= CDEV_OP_WR;
		else if ((*tp->tty_devwrite)(tp, 1)) ready_ops |= CDEV_OP_WR;
	}
	return ready_ops;
}

int select_retry(struct tty *tp)
{
	int ops;

	if (tp->tty_select_ops && (ops = select_try(tp, tp->tty_select_ops))) {
		chardriver_reply_select(tp->tty_select_proc,
			tp->tty_select_minor, ops);
		tp->tty_select_ops &= ~ops;
	}
	return OK;
}

/*===========================================================================*
 *				do_select				     *
 *===========================================================================*/
static int do_select(devminor_t minor, unsigned int ops, endpoint_t endpt)
{
  tty_t *tp;
  int ready_ops, watch;

  if ((tp = line2tty(minor)) == NULL)
	return ENXIO;

  watch = (ops & CDEV_NOTIFY);
  ops &= (CDEV_OP_RD | CDEV_OP_WR | CDEV_OP_ERR);

  ready_ops = select_try(tp, ops);

  ops &= ~ready_ops;
  if (ops && watch) {
	/* Translated minor numbers are a problem with late select replies. We
	 * have to save the minor number used to do the select, since otherwise
	 * VFS won't be able to make sense of those late replies. We do not
	 * support selecting on two different minors for the same object.
	 */
	if (tp->tty_select_ops != 0 && tp->tty_select_minor != minor) {
		printf("TTY: select on one object with two minors (%d, %d)\n",
			tp->tty_select_minor, minor);
		return EBADF;
	}
	tp->tty_select_ops |= ops;
	tp->tty_select_proc = endpt;
	tp->tty_select_minor = minor;
  }

  return ready_ops;
}

/*===========================================================================*
 *				handle_events				     *
 *===========================================================================*/
void handle_events(tp)
tty_t *tp;			/* TTY to check for events. */
{
/* Handle any events pending on a TTY. */

  do {
	tp->tty_events = 0;

	/* Read input and perform input processing. */
	(*tp->tty_devread)(tp, 0);

	/* Perform output processing and write output. */
	(*tp->tty_devwrite)(tp, 0);

	/* Ioctl waiting for some event? */
	if (tp->tty_ioreq != 0) dev_ioctl(tp);
  } while (tp->tty_events);

  /* Transfer characters from the input queue to a waiting process. */
  in_transfer(tp);

  /* Reply if enough bytes are available. */
  if (tp->tty_incum >= tp->tty_min && tp->tty_inleft > 0) {
	chardriver_reply_task(tp->tty_incaller, tp->tty_inid, tp->tty_incum);
	tp->tty_inleft = tp->tty_incum = 0;
	tp->tty_incaller = NONE;
  }
  if (tp->tty_select_ops)
  {
  	select_retry(tp);
  }
  select_retry_pty(tp);
}

/*===========================================================================*
 *				in_transfer				     *
 *===========================================================================*/
static void in_transfer(tp)
register tty_t *tp;		/* pointer to terminal to read from */
{
/* Transfer bytes from the input queue to a process reading from a terminal. */

  int ch;
  int count;
  char buf[64], *bp;

  /* Force read to succeed if the line is hung up, looks like EOF to reader. */
  if (tp->tty_termios.c_ospeed == B0) tp->tty_min = 0;

  /* Anything to do? */
  if (tp->tty_inleft == 0 || tp->tty_eotct < tp->tty_min) return;

  bp = buf;
  while (tp->tty_inleft > 0 && tp->tty_eotct > 0) {
	ch = *tp->tty_intail;

	if (!(ch & IN_EOF)) {
		/* One character to be delivered to the user. */
		*bp = ch & IN_CHAR;
		tp->tty_inleft--;
		if (++bp == bufend(buf)) {
			/* Temp buffer full, copy to user space. */
			sys_safecopyto(tp->tty_incaller,
				tp->tty_ingrant, tp->tty_incum,
				(vir_bytes) buf, (vir_bytes) buflen(buf));
			tp->tty_incum += buflen(buf);
			bp = buf;
		}
	}

	/* Remove the character from the input queue. */
	if (++tp->tty_intail == bufend(tp->tty_inbuf))
		tp->tty_intail = tp->tty_inbuf;
	tp->tty_incount--;
	if (ch & IN_EOT) {
		tp->tty_eotct--;
		/* Don't read past a line break in canonical mode. */
		if (tp->tty_termios.c_lflag & ICANON) tp->tty_inleft = 0;
	}
  }

  if (bp > buf) {
	/* Leftover characters in the buffer. */
	count = bp - buf;
	sys_safecopyto(tp->tty_incaller, tp->tty_ingrant, tp->tty_incum,
		(vir_bytes) buf, (vir_bytes) count);
	tp->tty_incum += count;
  }

  /* Usually reply to the reader, possibly even if incum == 0 (EOF). */
  if (tp->tty_inleft == 0) {
	chardriver_reply_task(tp->tty_incaller, tp->tty_inid, tp->tty_incum);
	tp->tty_inleft = tp->tty_incum = 0;
	tp->tty_incaller = NONE;
  }
}

/*===========================================================================*
 *				in_process				     *
 *===========================================================================*/
int in_process(tp, buf, count)
register tty_t *tp;		/* terminal on which character has arrived */
char *buf;			/* buffer with input characters */
int count;			/* number of input characters */
{
/* Characters have just been typed in.  Process, save, and echo them.  Return
 * the number of characters processed.
 */

  int ch, ct;
  int timeset = FALSE;

  for (ct = 0; ct < count; ct++) {
	/* Take one character. */
	ch = *buf++ & BYTE;

	/* Strip to seven bits? */
	if (tp->tty_termios.c_iflag & ISTRIP) ch &= 0x7F;

	/* Input extensions? */
	if (tp->tty_termios.c_lflag & IEXTEN) {

		/* Previous character was a character escape? */
		if (tp->tty_escaped) {
			tp->tty_escaped = NOT_ESCAPED;
			ch |= IN_ESC;	/* protect character */
		}

		/* LNEXT (^V) to escape the next character? */
		if (ch == tp->tty_termios.c_cc[VLNEXT]) {
			tp->tty_escaped = ESCAPED;
			rawecho(tp, '^');
			rawecho(tp, '\b');
			continue;	/* do not store the escape */
		}

		/* REPRINT (^R) to reprint echoed characters? */
		if (ch == tp->tty_termios.c_cc[VREPRINT]) {
			reprint(tp);
			continue;
		}
	}

	/* _POSIX_VDISABLE is a normal character value, so better escape it. */
	if (ch == _POSIX_VDISABLE) ch |= IN_ESC;

	/* Map CR to LF, ignore CR, or map LF to CR. */
	if (ch == '\r') {
		if (tp->tty_termios.c_iflag & IGNCR) continue;
		if (tp->tty_termios.c_iflag & ICRNL) ch = '\n';
	} else
	if (ch == '\n') {
		if (tp->tty_termios.c_iflag & INLCR) ch = '\r';
	}

	/* Canonical mode? */
	if (tp->tty_termios.c_lflag & ICANON) {

		/* Erase processing (rub out of last character). */
		if (ch == tp->tty_termios.c_cc[VERASE]) {
			(void) back_over(tp);
			if (!(tp->tty_termios.c_lflag & ECHOE)) {
				(void) tty_echo(tp, ch);
			}
			continue;
		}

		/* Kill processing (remove current line). */
		if (ch == tp->tty_termios.c_cc[VKILL]) {
			while (back_over(tp)) {}
			if (!(tp->tty_termios.c_lflag & ECHOE)) {
				(void) tty_echo(tp, ch);
				if (tp->tty_termios.c_lflag & ECHOK)
					rawecho(tp, '\n');
			}
			continue;
		}

		/* EOF (^D) means end-of-file, an invisible "line break". */
		if (ch == tp->tty_termios.c_cc[VEOF]) ch |= IN_EOT | IN_EOF;

		/* The line may be returned to the user after an LF. */
		if (ch == '\n') ch |= IN_EOT;

		/* Same thing with EOL, whatever it may be. */
		if (ch == tp->tty_termios.c_cc[VEOL]) ch |= IN_EOT;
	}

	/* Start/stop input control? */
	if (tp->tty_termios.c_iflag & IXON) {

		/* Output stops on STOP (^S). */
		if (ch == tp->tty_termios.c_cc[VSTOP]) {
			tp->tty_inhibited = STOPPED;
			tp->tty_events = 1;
			continue;
		}

		/* Output restarts on START (^Q) or any character if IXANY. */
		if (tp->tty_inhibited) {
			if (ch == tp->tty_termios.c_cc[VSTART]
					|| (tp->tty_termios.c_iflag & IXANY)) {
				tp->tty_inhibited = RUNNING;
				tp->tty_events = 1;
				if (ch == tp->tty_termios.c_cc[VSTART])
					continue;
			}
		}
	}

	if (tp->tty_termios.c_lflag & ISIG) {
		/* Check for INTR, QUIT and STATUS characters. */
		int sig = -1;
		if (ch == tp->tty_termios.c_cc[VINTR])
			sig = SIGINT;
		else if(ch == tp->tty_termios.c_cc[VQUIT])
			sig = SIGQUIT;
		else if(ch == tp->tty_termios.c_cc[VSTATUS])
			sig = SIGINFO;

		if(sig >= 0) {
			sigchar(tp, sig, 1);
			(void) tty_echo(tp, ch);
			continue;
		}
	}

	/* Is there space in the input buffer? */
	if (tp->tty_incount == buflen(tp->tty_inbuf)) {
		/* No space; discard in canonical mode, keep in raw mode. */
		if (tp->tty_termios.c_lflag & ICANON) continue;
		break;
	}

	if (!(tp->tty_termios.c_lflag & ICANON)) {
		/* In raw mode all characters are "line breaks". */
		ch |= IN_EOT;

		/* Start an inter-byte timer? */
		if (!timeset && tp->tty_termios.c_cc[VMIN] > 0
				&& tp->tty_termios.c_cc[VTIME] > 0) {
			settimer(tp, TRUE);
			timeset = TRUE;
		}
	}

	/* Perform the intricate function of echoing. */
	if (tp->tty_termios.c_lflag & (ECHO|ECHONL)) ch = tty_echo(tp, ch);

	/* Save the character in the input queue. */
	*tp->tty_inhead++ = ch;
	if (tp->tty_inhead == bufend(tp->tty_inbuf))
		tp->tty_inhead = tp->tty_inbuf;
	tp->tty_incount++;
	if (ch & IN_EOT) tp->tty_eotct++;

	/* Try to finish input if the queue threatens to overflow. */
	if (tp->tty_incount == buflen(tp->tty_inbuf)) in_transfer(tp);
  }
  return ct;
}

/*===========================================================================*
 *				echo					     *
 *===========================================================================*/
static int tty_echo(tp, ch)
register tty_t *tp;		/* terminal on which to echo */
register int ch;		/* pointer to character to echo */
{
/* Echo the character if echoing is on.  Some control characters are echoed
 * with their normal effect, other control characters are echoed as "^X",
 * normal characters are echoed normally.  EOF (^D) is echoed, but immediately
 * backspaced over.  Return the character with the echoed length added to its
 * attributes.
 */
  int len, rp;

  ch &= ~IN_LEN;
  if (!(tp->tty_termios.c_lflag & ECHO)) {
	if (ch == ('\n' | IN_EOT) && (tp->tty_termios.c_lflag
					& (ICANON|ECHONL)) == (ICANON|ECHONL))
		(*tp->tty_echo)(tp, '\n');
	return(ch);
  }

  /* "Reprint" tells if the echo output has been messed up by other output. */
  rp = tp->tty_incount == 0 ? FALSE : tp->tty_reprint;

  if ((ch & IN_CHAR) < ' ') {
	switch (ch & (IN_ESC|IN_EOF|IN_EOT|IN_CHAR)) {
	    case '\t':
		len = 0;
		do {
			(*tp->tty_echo)(tp, ' ');
			len++;
		} while (len < TAB_SIZE && (tp->tty_position & TAB_MASK) != 0);
		break;
	    case '\r' | IN_EOT:
	    case '\n' | IN_EOT:
		(*tp->tty_echo)(tp, ch & IN_CHAR);
		len = 0;
		break;
	    default:
		(*tp->tty_echo)(tp, '^');
		(*tp->tty_echo)(tp, '@' + (ch & IN_CHAR));
		len = 2;
	}
  } else
  if ((ch & IN_CHAR) == '\177') {
	/* A DEL prints as "^?". */
	(*tp->tty_echo)(tp, '^');
	(*tp->tty_echo)(tp, '?');
	len = 2;
  } else {
	(*tp->tty_echo)(tp, ch & IN_CHAR);
	len = 1;
  }
  if (ch & IN_EOF) while (len > 0) { (*tp->tty_echo)(tp, '\b'); len--; }

  tp->tty_reprint = rp;
  return(ch | (len << IN_LSHIFT));
}

/*===========================================================================*
 *				rawecho					     *
 *===========================================================================*/
static void rawecho(tp, ch)
register tty_t *tp;
int ch;
{
/* Echo without interpretation if ECHO is set. */
  int rp = tp->tty_reprint;
  if (tp->tty_termios.c_lflag & ECHO) (*tp->tty_echo)(tp, ch);
  tp->tty_reprint = rp;
}

/*===========================================================================*
 *				back_over				     *
 *===========================================================================*/
static int back_over(tp)
register tty_t *tp;
{
/* Backspace to previous character on screen and erase it. */
  u16_t *head;
  int len;

  if (tp->tty_incount == 0) return(0);	/* queue empty */
  head = tp->tty_inhead;
  if (head == tp->tty_inbuf) head = bufend(tp->tty_inbuf);
  if (*--head & IN_EOT) return(0);		/* can't erase "line breaks" */
  if (tp->tty_reprint) reprint(tp);		/* reprint if messed up */
  tp->tty_inhead = head;
  tp->tty_incount--;
  if (tp->tty_termios.c_lflag & ECHOE) {
	len = (*head & IN_LEN) >> IN_LSHIFT;
	while (len > 0) {
		rawecho(tp, '\b');
		rawecho(tp, ' ');
		rawecho(tp, '\b');
		len--;
	}
  }
  return(1);				/* one character erased */
}

/*===========================================================================*
 *				reprint					     *
 *===========================================================================*/
static void reprint(tp)
register tty_t *tp;		/* pointer to tty struct */
{
/* Restore what has been echoed to screen before if the user input has been
 * messed up by output, or if REPRINT (^R) is typed.
 */
  int count;
  u16_t *head;

  tp->tty_reprint = FALSE;

  /* Find the last line break in the input. */
  head = tp->tty_inhead;
  count = tp->tty_incount;
  while (count > 0) {
	if (head == tp->tty_inbuf) head = bufend(tp->tty_inbuf);
	if (head[-1] & IN_EOT) break;
	head--;
	count--;
  }
  if (count == tp->tty_incount) return;		/* no reason to reprint */

  /* Show REPRINT (^R) and move to a new line. */
  (void) tty_echo(tp, tp->tty_termios.c_cc[VREPRINT] | IN_ESC);
  rawecho(tp, '\r');
  rawecho(tp, '\n');

  /* Reprint from the last break onwards. */
  do {
	if (head == bufend(tp->tty_inbuf)) head = tp->tty_inbuf;
	*head = tty_echo(tp, *head);
	head++;
	count++;
  } while (count < tp->tty_incount);
}

/*===========================================================================*
 *				out_process				     *
 *===========================================================================*/
void out_process(tp, bstart, bpos, bend, icount, ocount)
tty_t *tp;
char *bstart, *bpos, *bend;	/* start/pos/end of circular buffer */
int *icount;			/* # input chars / input chars used */
int *ocount;			/* max output chars / output chars used */
{
/* Perform output processing on a circular buffer.  *icount is the number of
 * bytes to process, and the number of bytes actually processed on return.
 * *ocount is the space available on input and the space used on output.
 * (Naturally *icount < *ocount.)  The column position is updated modulo
 * the TAB size, because we really only need it for tabs.
 */

  int tablen;
  int ict = *icount;
  int oct = *ocount;
  int pos = tp->tty_position;

  while (ict > 0) {
	switch (*bpos) {
	case '\7':
		break;
	case '\b':
		pos--;
		break;
	case '\r':
		pos = 0;
		break;
	case '\n':
		if ((tp->tty_termios.c_oflag & (OPOST|ONLCR))
							== (OPOST|ONLCR)) {
			/* Map LF to CR+LF if there is space.  Note that the
			 * next character in the buffer is overwritten, so
			 * we stop at this point.
			 */
			if (oct >= 2) {
				*bpos = '\r';
				if (++bpos == bend) bpos = bstart;
				*bpos = '\n';
				pos = 0;
				ict--;
				oct -= 2;
			}
			goto out_done;	/* no space or buffer got changed */
		}
		break;
	case '\t':
		/* Best guess for the tab length. */
		tablen = TAB_SIZE - (pos & TAB_MASK);

		if ((tp->tty_termios.c_oflag & (OPOST|OXTABS))
							== (OPOST|OXTABS)) {
			/* Tabs must be expanded. */
			if (oct >= tablen) {
				pos += tablen;
				ict--;
				oct -= tablen;
				do {
					*bpos = ' ';
					if (++bpos == bend) bpos = bstart;
				} while (--tablen != 0);
			}
			goto out_done;
		}
		/* Tabs are output directly. */
		pos += tablen;
		break;
	default:
		/* Assume any other character prints as one character. */
		pos++;
	}
	if (++bpos == bend) bpos = bstart;
	ict--;
	oct--;
  }
out_done:
  tp->tty_position = pos & TAB_MASK;

  *icount -= ict;	/* [io]ct are the number of chars not used */
  *ocount -= oct;	/* *[io]count are the number of chars that are used */
}

/*===========================================================================*
 *				dev_ioctl				     *
 *===========================================================================*/
static void dev_ioctl(tp)
tty_t *tp;
{
/* The ioctl's TCSETSW, TCSETSF and TIOCDRAIN wait for output to finish to make
 * sure that an attribute change doesn't affect the processing of current
 * output.  Once output finishes the ioctl is executed as in do_ioctl().
 */
  int result = EINVAL;

  if (tp->tty_outleft > 0) return;		/* output not finished */

  if (tp->tty_ioreq != TIOCDRAIN) {
	if (tp->tty_ioreq == TIOCSETAF) tty_icancel(tp);
	result = sys_safecopyfrom(tp->tty_iocaller, tp->tty_iogrant, 0,
		(vir_bytes) &tp->tty_termios,
		(vir_bytes) sizeof(tp->tty_termios));
	if (result == OK) setattr(tp);
  }
  tp->tty_ioreq = 0;
  chardriver_reply_task(tp->tty_iocaller, tp->tty_ioid, result);
  tp->tty_iocaller = NONE;
}

/*===========================================================================*
 *				setattr					     *
 *===========================================================================*/
static void setattr(tp)
tty_t *tp;
{
/* Apply the new line attributes (raw/canonical, line speed, etc.) */
  u16_t *inp;
  int count;

  if (!(tp->tty_termios.c_lflag & ICANON)) {
	/* Raw mode; put a "line break" on all characters in the input queue.
	 * It is undefined what happens to the input queue when ICANON is
	 * switched off, a process should use TCSAFLUSH to flush the queue.
	 * Keeping the queue to preserve typeahead is the Right Thing, however
	 * when a process does use TCSANOW to switch to raw mode.
	 */
	count = tp->tty_eotct = tp->tty_incount;
	inp = tp->tty_intail;
	while (count > 0) {
		*inp |= IN_EOT;
		if (++inp == bufend(tp->tty_inbuf)) inp = tp->tty_inbuf;
		--count;
	}
  }

  /* Inspect MIN and TIME. */
  settimer(tp, FALSE);
  if (tp->tty_termios.c_lflag & ICANON) {
	/* No MIN & TIME in canonical mode. */
	tp->tty_min = 1;
  } else {
	/* In raw mode MIN is the number of chars wanted, and TIME how long
	 * to wait for them.  With interesting exceptions if either is zero.
	 */
	tp->tty_min = tp->tty_termios.c_cc[VMIN];
	if (tp->tty_min == 0 && tp->tty_termios.c_cc[VTIME] > 0)
		tp->tty_min = 1;
  }

  if (!(tp->tty_termios.c_iflag & IXON)) {
	/* No start/stop output control, so don't leave output inhibited. */
	tp->tty_inhibited = RUNNING;
	tp->tty_events = 1;
  }

  /* Setting the output speed to zero hangs up the phone. */
  if (tp->tty_termios.c_ospeed == B0) sigchar(tp, SIGHUP, 1);
}

/*===========================================================================*
 *				sigchar					     *
 *===========================================================================*/
void sigchar(tp, sig, mayflush)
register tty_t *tp;
int sig;			/* SIGINT, SIGQUIT, SIGKILL or SIGHUP */
int mayflush;
{
/* Process a SIGINT, SIGQUIT or SIGKILL char from the keyboard or SIGHUP from
 * a tty close, "stty 0", or a real RS-232 hangup.  PM will send the signal to
 * the process group (INT, QUIT), all processes (KILL), or the session leader
 * (HUP).
 */
  int status;

  if (tp->tty_pgrp != 0)  {
      if (OK != (status = sys_kill(tp->tty_pgrp, sig))) {
        panic("Error; call to sys_kill failed: %d", status);
      }
  }

  if (mayflush && !(tp->tty_termios.c_lflag & NOFLSH)) {
	tp->tty_incount = tp->tty_eotct = 0;	/* kill earlier input */
	tp->tty_intail = tp->tty_inhead;
	(*tp->tty_ocancel)(tp, 0);			/* kill all output */
	tp->tty_inhibited = RUNNING;
	tp->tty_events = 1;
  }
}

/*===========================================================================*
 *				tty_icancel				     *
 *===========================================================================*/
static void tty_icancel(tp)
register tty_t *tp;
{
/* Discard all pending input, tty buffer or device. */

  tp->tty_incount = tp->tty_eotct = 0;
  tp->tty_intail = tp->tty_inhead;
  (*tp->tty_icancel)(tp, 0);
}

/*===========================================================================*
 *				tty_devnop				     *
 *===========================================================================*/
static int tty_devnop(tty_t *UNUSED(tp), int UNUSED(try))
{
  /* Some functions need not be implemented at the device level. */
  return 0;
}

/*===========================================================================*
 *				tty_init				     *
 *===========================================================================*/
static int tty_init(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize tty structure and call device initialization routines. */

  register tty_t *tp;
  int s;

  system_hz = sys_hz();

  tty_gid = -1;
  if (env_argc > 1)
	optset_parse(optset_table, env_argv[1]);

  /* Initialize the terminal lines. */
  memset(tty_table, '\0' , sizeof(tty_table));

  for (tp = FIRST_TTY,s=0; tp < END_TTY; tp++,s++) {

  	tp->tty_index = s;
  	init_timer(&tp->tty_tmr);

  	tp->tty_intail = tp->tty_inhead = tp->tty_inbuf;
  	tp->tty_min = 1;
	tp->tty_incaller = tp->tty_outcaller = tp->tty_iocaller = NONE;
  	tp->tty_termios = termios_defaults;
	tp->tty_icancel = tp->tty_ocancel = tp->tty_close =
			  tp->tty_open = tty_devnop;
	pty_init(tp);
  }

  return OK;
}

/*===========================================================================*
 *				tty_timed_out				     *
 *===========================================================================*/
static void tty_timed_out(int arg)
{
/* This timer has expired. Set the events flag, to force processing. */
  tty_t *tty_ptr;
  tty_ptr = &tty_table[arg];
  tty_ptr->tty_min = 0;			/* force read to succeed */
  tty_ptr->tty_events = 1;		
}

/*===========================================================================*
 *				settimer				     *
 *===========================================================================*/
static void settimer(tty_ptr, enable)
tty_t *tty_ptr;			/* line to set or unset a timer on */
int enable;			/* set timer if true, otherwise unset */
{
  clock_t ticks;

  if (enable) {
  	ticks = tty_ptr->tty_termios.c_cc[VTIME] * (system_hz/10);

 	/* Set a new timer for enabling the TTY events flags. */
	set_timer(&tty_ptr->tty_tmr, ticks, tty_timed_out, tty_ptr->tty_index);
  } else {
  	/* Remove the timer from the active and expired lists. */
  	cancel_timer(&tty_ptr->tty_tmr);
  }
}
