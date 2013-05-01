/* This file contains the terminal driver, both for the IBM console and regular
 * ASCII terminals.  It handles only the device-independent part of a TTY, the
 * device dependent parts are in console.c, rs232.c, etc.  This file contains
 * two main entry points, tty_task() and tty_wakeup(), and several minor entry
 * points for use by the device-dependent code.
 *
 * The device-independent part accepts "keyboard" input from the device-
 * dependent part, performs input processing (special key interpretation),
 * and sends the input to a process reading from the TTY.  Output to a TTY
 * is sent to the device-dependent code for output processing and "screen"
 * display.  Input processing is done by the device by calling 'in_process'
 * on the input characters, output processing may be done by the device itself
 * or by calling 'out_process'.  The TTY takes care of input queuing, the
 * device does the output queuing.  If a device receives an external signal,
 * like an interrupt, then it causes tty_wakeup() to be run by the CLOCK task
 * to, you guessed it, wake up the TTY to check if input or output can
 * continue.
 *
 * The valid messages and their parameters are:
 *
 *   notify from HARDWARE:       output has been completed or input has arrived
 *   notify from SYSTEM  :      e.g., MINIX wants to shutdown; run code to 
 *   				cleanly stop
 *   DEV_READ:       a process wants to read from a terminal
 *   DEV_WRITE:      a process wants to write on a terminal
 *   DEV_IOCTL:      a process wants to change a terminal's parameters
 *   DEV_OPEN:       a tty line has been opened
 *   DEV_CLOSE:      a tty line has been closed
 *   DEV_SELECT:     start select notification request
 *   DEV_STATUS:     FS wants to know status for SELECT or REVIVE
 *   CANCEL:         terminate a previous incomplete system call immediately
 *
 *    m_type      TTY_LINE  USER_ENDPT  COUNT   TTY_SPEKS IO_GRANT
 * -----------------------------------------------------------------
 * | HARD_INT    |         |         |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | SYS_SIG     | sig set |         |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_READ    |minor dev| proc nr |  count  |         |  grant  |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_WRITE   |minor dev| proc nr |  count  |         |  grant  |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_IOCTL   |minor dev| proc nr |func code|erase etc|         |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_OPEN    |minor dev| proc nr | O_NOCTTY|         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_CLOSE   |minor dev| proc nr |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | DEV_STATUS  |         |         |         |         |         |
 * |-------------+---------+---------+---------+---------+---------|
 * | CANCEL      |minor dev| proc nr |         |         |         |
 * -----------------------------------------------------------------
 *
 * Changes:
 *   Jan 20, 2004   moved TTY driver to user-space  (Jorrit N. Herder)
 *   Sep 20, 2004   local timer management/ sync alarms  (Jorrit N. Herder)
 *   Jul 13, 2004   support for function key observers  (Jorrit N. Herder)  
 */

#include <assert.h>
#include <minix/drivers.h>
#include <minix/driver.h>
#include <termios.h>
#include <sys/ioc_tty.h>
#include <signal.h>
#include <minix/callnr.h>
#include <minix/sys_config.h>
#include <minix/tty.h>
#include <minix/keymap.h>
#include <minix/endpoint.h>
#include "tty.h"

#include <sys/time.h>
#include <sys/select.h>

unsigned long kbd_irq_set = 0;
unsigned long rs_irq_set = 0;

/* Address of a tty structure. */
#define tty_addr(line)	(&tty_table[line])

/* Macros for magic tty types. */
#define isconsole(tp)	((tp) < tty_addr(NR_CONS))
#define ispty(tp)	((tp) != NULL && (tp)->tty_minor >= TTYPX_MINOR)

/* Macros for magic tty structure pointers. */
#define FIRST_TTY	tty_addr(0)
#define END_TTY		tty_addr(sizeof(tty_table) / sizeof(tty_table[0]))

/* A device exists if at least its 'devread' function is defined. */
#define tty_active(tp)	((tp)->tty_devread != NULL)

/* RS232 lines or pseudo terminals can be completely configured out. */
#if NR_RS_LINES == 0
#define rs_init(tp)	((void) 0)
#endif

#if NR_PTYS == 0
#define pty_init(tp)	((void) 0)
#define do_pty(tp, mp)	((void) 0)
#endif

struct kmessages kmess;

static void tty_timed_out(timer_t *tp);
static void settimer(tty_t *tty_ptr, int enable);
static void do_cancel(tty_t *tp, message *m_ptr);
static void do_ioctl(tty_t *tp, message *m_ptr);
static void do_open(tty_t *tp, message *m_ptr);
static void do_close(tty_t *tp, message *m_ptr);
static void do_read(tty_t *tp, message *m_ptr);
static void do_write(tty_t *tp, message *m_ptr);
static void do_select(tty_t *tp, message *m_ptr);
static void do_status(message *m_ptr);
static void in_transfer(tty_t *tp);
static int tty_echo(tty_t *tp, int ch);
static void rawecho(tty_t *tp, int ch);
static int back_over(tty_t *tp);
static void reprint(tty_t *tp);
static void dev_ioctl(tty_t *tp);
static void setattr(tty_t *tp);
static void tty_icancel(tty_t *tp);
static void tty_init(void);
static void do_new_kmess(void);
static tty_t * line2tty(int line);
static void set_console_line(char term[CONS_ARG]);
static void set_kernel_color(char color[CONS_ARG]);
static void set_color(tty_t *tp, int color);
static void reset_color(tty_t *tp);

/* Default attributes. */
static struct termios termios_defaults = {
  TINPUT_DEF, TOUTPUT_DEF, TCTRL_DEF, TLOCAL_DEF, TSPEED_DEF, TSPEED_DEF,
  {
	TEOF_DEF, TEOL_DEF, TERASE_DEF, TINTR_DEF, TKILL_DEF, TMIN_DEF,
	TQUIT_DEF, TTIME_DEF, TSUSP_DEF, TSTART_DEF, TSTOP_DEF,
	TREPRINT_DEF, TLNEXT_DEF, TDISCARD_DEF,
  },
};
static struct winsize winsize_defaults;	/* = all zeroes */

/* Global variables for the TTY task (declared extern in tty.h). */
tty_t tty_table[NR_CONS+NR_RS_LINES+NR_PTYS];
int ccurrent;			/* currently active console */
struct machine machine;		/* kernel environment variables */
u32_t system_hz;
u32_t consoleline = CONS_MINOR;
u32_t kernel_msg_color = 0;

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
static void sef_cb_signal_handler(int signo);

extern struct minix_kerninfo *_minix_kerninfo;

/*===========================================================================*
 *				tty_task				     *
 *===========================================================================*/
int main(void)
{
/* Main routine of the terminal task. */

  message tty_mess;		/* buffer for all incoming messages */
  int ipc_status;
  unsigned line;
  int r;
  register tty_t *tp;

  /* SEF local startup. */
  sef_local_startup();
  while (TRUE) {
	/* Check for and handle any events on any of the ttys. */
	for (tp = FIRST_TTY; tp < END_TTY; tp++) {
		if (tp->tty_events) handle_events(tp);
	}

	/* Get a request message. */
	r= driver_receive(ANY, &tty_mess, &ipc_status);
	if (r != 0)
		panic("driver_receive failed with: %d", r);

	/* First handle all kernel notification types that the TTY supports. 
	 *  - An alarm went off, expire all timers and handle the events. 
	 *  - A hardware interrupt also is an invitation to check for events. 
	 *  - A new kernel message is available for printing.
	 *  - Reset the console on system shutdown. 
	 * Then see if this message is different from a normal device driver
	 * request and should be handled separately. These extra functions
	 * do not operate on a device, in constrast to the driver requests. 
	 */

	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(tty_mess.m_source)) {
			case CLOCK:
				/* run watchdogs of expired timers */
				expire_timers(tty_mess.NOTIFY_TIMESTAMP);
				break;
			case HARDWARE: 
				/* hardware interrupt notification */
				
				/* fetch chars from keyboard */
				if (tty_mess.NOTIFY_ARG & kbd_irq_set)
					kbd_interrupt(&tty_mess);
#if NR_RS_LINES > 0
				/* serial I/O */
				if (tty_mess.NOTIFY_ARG & rs_irq_set)
					rs_interrupt(&tty_mess);
#endif
				/* run watchdogs of expired timers */
				expire_timers(tty_mess.NOTIFY_TIMESTAMP);
				break;
			default:
				/* do nothing */
				break;
		}

		/* done, get new message */
		continue;
	}

	switch (tty_mess.m_type) { 
	case TTY_FKEY_CONTROL:		/* (un)register a fkey observer */
	case OLD_FKEY_CONTROL:		/* old number */
		do_fkey_ctl(&tty_mess);
		continue;
	case INPUT_EVENT:
		do_kb_inject(&tty_mess);
		continue;
	default:			/* should be a driver request */
		;			/* do nothing; end switch */
	}

	/* Only device requests should get to this point. All requests, 
	 * except DEV_STATUS, have a minor device number. Check this
	 * exception and get the minor device number otherwise.
	 */
	if (tty_mess.m_type == DEV_STATUS) {
		do_status(&tty_mess);
		continue;
	}
	line = tty_mess.TTY_LINE;
	if (line == CONS_MINOR || line == LOG_MINOR) {
		/* /dev/log output goes to /dev/console */
		if (consoleline != CONS_MINOR) {
			/* Console output must redirected */
			line = consoleline;
			tty_mess.TTY_LINE = line;
		}
	}
	if (line == KBD_MINOR) {
		do_kbd(&tty_mess);
		continue;
	} else if (line == KBDAUX_MINOR) {
		do_kbdaux(&tty_mess);
		continue;
	} else if (line == VIDEO_MINOR) {
		do_video(&tty_mess);
		continue;
	} else {
		tp = line2tty(line);

		/* Terminals and pseudo terminals belong together. We can only
		 * make a distinction between the two based on position in the
		 * tty_table and not on minor number (i.e., use ispty macro).
		 * Hence this special case.
		 */
		if (line - PTYPX_MINOR < NR_PTYS &&
						tty_mess.m_type != DEV_IOCTL_S){
			do_pty(tp, &tty_mess);
			continue;
		}
	}

	/* If the device doesn't exist or is not configured return ENXIO. */
	if (tp == NULL || ! tty_active(tp)) {
		if (tty_mess.m_source != LOG_PROC_NR) {
			tty_reply(TASK_REPLY, tty_mess.m_source,
				  tty_mess.USER_ENDPT, ENXIO);
		}
		continue;
	}

	/* Execute the requested device driver function. */
	switch (tty_mess.m_type) {
	    case DEV_READ_S:	 do_read(tp, &tty_mess);	  break;
	    case DEV_WRITE_S:	 do_write(tp, &tty_mess);	  break;
	    case DEV_IOCTL_S:	 do_ioctl(tp, &tty_mess);	  break;
	    case DEV_OPEN:	 do_open(tp, &tty_mess);	  break;
	    case DEV_CLOSE:	 do_close(tp, &tty_mess);	  break;
	    case DEV_SELECT:	 do_select(tp, &tty_mess);	  break;
	    case CANCEL:	 do_cancel(tp, &tty_mess);	  break;
	    default:		
		printf("Warning, TTY got unexpected request %d from %d\n",
			tty_mess.m_type, tty_mess.m_source);
	    tty_reply(TASK_REPLY, tty_mess.m_source,
						tty_mess.USER_ENDPT, EINVAL);
	}
  }

  return 0;
}

static void
set_color(tty_t *tp, int color)
{
	message msg;
	char buf[8];

	buf[0] = '\033';
	snprintf(&buf[1], sizeof(buf) - 1, "[1;%dm", color);
	msg.m_source = KERNEL;
	msg.IO_GRANT = buf;
	msg.COUNT = sizeof(buf);
	do_write(tp, &msg);
}

static void
reset_color(tty_t *tp)
{
	message msg;
	char buf[8];

#define SGR_COLOR_RESET	39
	buf[0] = '\033';
	snprintf(&buf[1], sizeof(buf) - 1, "[0;%dm", SGR_COLOR_RESET);
	msg.m_source = KERNEL;
	msg.IO_GRANT = buf;
	msg.COUNT = sizeof(buf);
	do_write(tp, &msg);
}

static tty_t *
line2tty(int line)
{
/* Convert a terminal line to tty_table pointer */

	tty_t* tp;

	if (line == KBD_MINOR || line == KBDAUX_MINOR || line == VIDEO_MINOR) {
		return(NULL);
	} else if ((line - CONS_MINOR) < NR_CONS) {
		tp = tty_addr(line - CONS_MINOR);
	} else if (line == LOG_MINOR) {
		tp = tty_addr(consoleline);
	} else if ((line - RS232_MINOR) < NR_RS_LINES) {
		tp = tty_addr(line - RS232_MINOR + NR_CONS);
	} else if ((line - TTYPX_MINOR) < NR_PTYS) {
		tp = tty_addr(line - TTYPX_MINOR + NR_CONS + NR_RS_LINES);
	} else if ((line - PTYPX_MINOR) < NR_PTYS) {
		tp = tty_addr(line - PTYPX_MINOR + NR_CONS + NR_RS_LINES);
	} else {
		tp = NULL;
	}

	return(tp);
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* No live update support for now. */

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the tty driver. */
  int r;
  char val[CONS_ARG];

  /* Get kernel environment (protected_mode, pc_at and ega are needed). */ 
  if (OK != (r=sys_getmachine(&machine))) {
    panic("Couldn't obtain kernel environment: %d", r);
  }

  if (env_get_param("console", val, sizeof(val)) == OK) {
	set_console_line(val);
  }

  if ((r = env_get_param("kernelclr", val, sizeof(val))) == OK) {
	set_kernel_color(val);
  }

  /* Initialize the TTY driver. */
  tty_init();

  /* Final one-time keyboard initialization. */
  kb_init_once();

  return(OK);
}

static void
set_console_line(char term[CONS_ARG])
{
/* Parse 'term' and redirect console output there. */
	int i;

	/* Console */
	if (!strncmp(term, "console", CONS_ARG - 1)) {
		consoleline = CONS_MINOR+0;
	}

	/* The other console terminals */
	for (i = 1; i < NR_CONS; i++) {
		char cons[6];
		strlcpy(cons, "ttyc0", sizeof(cons));
		cons[4] += i;
		if (!strncmp(term, cons,
		    CONS_ARG < sizeof(cons) ? CONS_ARG-1 : sizeof(cons) - 1))
			consoleline = CONS_MINOR + i;
	}

	/* Serial lines */
	assert(NR_RS_LINES <= 9);/* bellow assumes this is the case */
	for (i = 0; i < NR_RS_LINES; i++) {
		char sercons[6];
		strlcpy(sercons, "tty00", sizeof(sercons));
		sercons[4] += i;
		if (!strncmp(term, sercons,
		    CONS_ARG < sizeof(sercons) ? CONS_ARG-1:sizeof(sercons)-1))
			consoleline = RS232_MINOR + i;
	}
}

static void
set_kernel_color(char color[CONS_ARG])
{
	int def_color;

#define SGR_COLOR_START	30
#define SGR_COLOR_END	37

	def_color = atoi(color);
	if ((SGR_COLOR_START + def_color) >= SGR_COLOR_START &&
	    (SGR_COLOR_START + def_color) <= SGR_COLOR_END) {
		kernel_msg_color = def_color + SGR_COLOR_START;
	}
}

static void
do_new_kmess(void)
{
/* Kernel wants to print a new message */
	struct kmessages *kmess_ptr;	/* kmessages structure */
	char kernel_buf_copy[_KMESS_BUF_SIZE];
	static int prev_next = 0;
	int next, bytes, copy, restore = 0;
	tty_t *tp, rtp;
	message print_kmsg;

	assert(_minix_kerninfo);
	kmess_ptr = _minix_kerninfo->kmessages;

	/* The kernel buffer is circular; print only the new part. Determine
	 * how many new bytes there are with the help of current and
	 * previous 'next' index. This works fine if less than _KMESS_BUF_SIZE
	 * bytes is new data; else we miss % _KMESS_BUF_SIZE here. Obtain
	 * 'next' only once, since we are operating on shared memory here.
	 * Check for size being positive; the buffer might as well be emptied!
	 */
	next = kmess_ptr->km_next;
	bytes = ((next + _KMESS_BUF_SIZE) - prev_next) % _KMESS_BUF_SIZE;
	if (bytes > 0) {
		/* Copy from current position toward end of buffer */
		copy = MIN(_KMESS_BUF_SIZE - prev_next, bytes);
		memcpy(kernel_buf_copy, &kmess_ptr->km_buf[prev_next], copy);

		/* Copy remainder from start of buffer */
		if (copy < bytes) {
			memcpy(&kernel_buf_copy[copy], &kmess_ptr->km_buf[0],
				bytes - copy);
		}

		tp = line2tty(consoleline);
		if (tp == NULL || !tty_active(tp))
			panic("Don't know where to send kernel messages");
		if (tp->tty_outleft > 0) {
			/* Terminal is already printing */
			rtp = *tp;	/* Make backup */
			tp->tty_outleft = 0; /* So do_write is happy */
			restore = 1;
		}

		if (kernel_msg_color != 0)
			set_color(tp, kernel_msg_color);
		print_kmsg.m_source = KERNEL;
		print_kmsg.IO_GRANT = kernel_buf_copy;
		print_kmsg.COUNT = bytes;
		do_write(tp, &print_kmsg);
		if (kernel_msg_color != 0)
			reset_color(tp);
		if (restore) {
			*tp = rtp;
		}
	}

	/* Store 'next' pointer so that we can determine what part of the
	 * kernel messages buffer to print next time a notification arrives.
	 */
	prev_next = next;
}

/*===========================================================================*
 *		           sef_cb_signal_handler                             *
 *===========================================================================*/
static void sef_cb_signal_handler(int signo)
{
  /* Check for known signals, ignore anything else. */
  switch(signo) {
      /* There is a pending message from the kernel. */
      case SIGKMESS:
	  do_new_kmess();
      break;
      /* Switch to primary console on termination. */
      case SIGTERM:
          cons_stop();
      break;
  }
}

/*===========================================================================*
 *				do_status				     *
 *===========================================================================*/
static void do_status(m_ptr)
message *m_ptr;
{
  register struct tty *tp;
  int event_found;
  int status;
  int ops;
  
  /* Check for select or revive events on any of the ttys. If we found an, 
   * event return a single status message for it. The FS will make another 
   * call to see if there is more.
   */
  event_found = 0;
  for (tp = FIRST_TTY; tp < END_TTY; tp++) {
	if ((ops = select_try(tp, tp->tty_select_ops)) && 
			tp->tty_select_proc == m_ptr->m_source) {

		/* I/O for a selected minor device is ready. */
		m_ptr->m_type = DEV_IO_READY;
		m_ptr->DEV_MINOR = tp->tty_minor;
		m_ptr->DEV_SEL_OPS = ops;

		tp->tty_select_ops &= ~ops;	/* unmark select event */
  		event_found = 1;
		break;
	}
	else if (tp->tty_inrevived && tp->tty_incaller == m_ptr->m_source) {
		
		/* Suspended request finished. Send a REVIVE. */
		m_ptr->m_type = DEV_REVIVE;
  		m_ptr->REP_ENDPT = tp->tty_inproc;
  		m_ptr->REP_IO_GRANT = tp->tty_ingrant;
  		m_ptr->REP_STATUS = tp->tty_incum;

		tp->tty_inleft = tp->tty_incum = 0;
		tp->tty_inrevived = 0;		/* unmark revive event */
		tp->tty_ingrant = GRANT_INVALID;
		event_found = 1;
		break;
	}
	else if (tp->tty_outrevived && tp->tty_outcaller == m_ptr->m_source) {
		
		/* Suspended request finished. Send a REVIVE. */
		m_ptr->m_type = DEV_REVIVE;
  		m_ptr->REP_ENDPT = tp->tty_outproc;
  		m_ptr->REP_IO_GRANT = tp->tty_outgrant;
  		m_ptr->REP_STATUS = tp->tty_outcum;

		tp->tty_outcum = 0;
		tp->tty_outrevived = 0;		/* unmark revive event */
		tp->tty_outgrant = GRANT_INVALID;
		event_found = 1;
		break;
	}
	else if (tp->tty_iorevived && tp->tty_iocaller == m_ptr->m_source) {

		/* Suspended request finished. Send a REVIVE. */
		m_ptr->m_type = DEV_REVIVE;
  		m_ptr->REP_ENDPT = tp->tty_ioproc;
  		m_ptr->REP_IO_GRANT = tp->tty_iogrant;
  		m_ptr->REP_STATUS = tp->tty_iostatus;
		tp->tty_iorevived = 0;		/* unmark revive event */
		tp->tty_iogrant = GRANT_INVALID;
		event_found = 1;
		break;
	}
  }

#if NR_PTYS > 0
  if (!event_found)
  	event_found = pty_status(m_ptr);
#endif
  if (!event_found)
	event_found= kbd_status(m_ptr);

  if (! event_found) {
	/* No events of interest were found. Return an empty message. */
  	m_ptr->m_type = DEV_NO_STATUS;
  }

  /* Almost done. Send back the reply message to the caller. */
  status = send(m_ptr->m_source, m_ptr);
  if (status != OK) {
	printf("tty`do_status: send to %d failed: %d\n",
		m_ptr->m_source, status);
  }
}

/*===========================================================================*
 *				do_read					     *
 *===========================================================================*/
static void do_read(tp, m_ptr)
register tty_t *tp;		/* pointer to tty struct */
register message *m_ptr;	/* pointer to message sent to the task */
{
/* A process wants to read from a terminal. */
  int r;

  /* Check if there is already a process hanging in a read, check if the
   * parameters are correct, do I/O.
   */
  if (tp->tty_inleft > 0) {
	r = EIO;
  } else
  if (m_ptr->COUNT <= 0) {
	r = EINVAL;
  } else if (tp->tty_ingrant != GRANT_INVALID) {
	/* This is actually a fundamental problem with TTY; it can handle
	 * only one reader per minor device. If we don't return an error,
	 * we'll overwrite the previous reader and that process will get
	 * stuck forever. */
	r = ENOBUFS;
  } else {
	/* Copy information from the message to the tty struct. */
	tp->tty_inrepcode = TASK_REPLY;
	tp->tty_incaller = m_ptr->m_source;
	tp->tty_inproc = m_ptr->USER_ENDPT;
	tp->tty_ingrant = (cp_grant_id_t) m_ptr->IO_GRANT;
	tp->tty_inoffset = 0;
	tp->tty_inleft = m_ptr->COUNT;

	if (!(tp->tty_termios.c_lflag & ICANON)
					&& tp->tty_termios.c_cc[VTIME] > 0) {
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
	if (tp->tty_inleft == 0)  {
		return;			/* already done */
	}

	/* There were no bytes in the input queue available, so suspend
	 * the caller.
	 */
	r = SUSPEND;				/* suspend the caller */
	tp->tty_inrepcode = TTY_REVIVE;
  }
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->USER_ENDPT, r);
  if (tp->tty_select_ops)
  	select_retry(tp);
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
static void do_write(tp, m_ptr)
register tty_t *tp;
register message *m_ptr;	/* pointer to message sent to the task */
{
/* A process wants to write on a terminal. */
  int r;

  /* Check if there is already a process hanging in a write, check if the
   * parameters are correct, do I/O.
   */
  if (tp->tty_outleft > 0) {
	r = EIO;
  } else
  if (m_ptr->COUNT <= 0) {
	r = EINVAL;
  } else {
	/* Copy message parameters to the tty structure. */
	tp->tty_outrepcode = TASK_REPLY;
	tp->tty_outcaller = m_ptr->m_source;
	tp->tty_outproc = m_ptr->USER_ENDPT;
	tp->tty_outgrant = (cp_grant_id_t) m_ptr->IO_GRANT;
	tp->tty_outoffset = 0;
	tp->tty_outleft = m_ptr->COUNT;

	/* Try to write. */
	handle_events(tp);
	if (tp->tty_outleft == 0)
		return;	/* already done */

	/* None or not all the bytes could be written, so suspend the
	 * caller.
	 */
	r = SUSPEND;				/* suspend the caller */
	tp->tty_outrepcode = TTY_REVIVE;
  }
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->USER_ENDPT, r);
}

/*===========================================================================*
 *				do_ioctl				     *
 *===========================================================================*/
static void do_ioctl(tp, m_ptr)
register tty_t *tp;
message *m_ptr;			/* pointer to message sent to task */
{
/* Perform an IOCTL on this terminal. Posix termios calls are handled
 * by the IOCTL system call
 */

  int r;
  union {
	int i;
  } param;
  size_t size;

  /* Size of the ioctl parameter. */
  switch (m_ptr->TTY_REQUEST) {
    case TCGETS:        /* Posix tcgetattr function */
    case TCSETS:        /* Posix tcsetattr function, TCSANOW option */ 
    case TCSETSW:       /* Posix tcsetattr function, TCSADRAIN option */
    case TCSETSF:	/* Posix tcsetattr function, TCSAFLUSH option */
        size = sizeof(struct termios);
        break;

    case TCSBRK:        /* Posix tcsendbreak function */
    case TCFLOW:        /* Posix tcflow function */
    case TCFLSH:        /* Posix tcflush function */
    case TIOCGPGRP:     /* Posix tcgetpgrp function */
    case TIOCSPGRP:	/* Posix tcsetpgrp function */
        size = sizeof(int);
        break;

    case TIOCGWINSZ:    /* get window size (not Posix) */
    case TIOCSWINSZ:	/* set window size (not Posix) */
        size = sizeof(struct winsize);
        break;

    case KIOCSMAP:	/* load keymap (Minix extension) */
        size = sizeof(keymap_t);
        break;

    case TIOCSFON:	/* load font (Minix extension) */
        size = sizeof(u8_t [8192]);
        break;

    case TCDRAIN:	/* Posix tcdrain function -- no parameter */
    default:		size = 0;
  }

  r = OK;
  switch (m_ptr->TTY_REQUEST) {
    case TCGETS:
	/* Get the termios attributes. */
	r = sys_safecopyto(m_ptr->m_source, (cp_grant_id_t) m_ptr->IO_GRANT, 0,
		(vir_bytes) &tp->tty_termios, (vir_bytes) size);
	break;

    case TCSETSW:
    case TCSETSF:
    case TCDRAIN:
	if (tp->tty_outleft > 0) {
		/* Wait for all ongoing output processing to finish. */
		tp->tty_iocaller = m_ptr->m_source;
		tp->tty_ioproc = m_ptr->USER_ENDPT;
		tp->tty_ioreq = m_ptr->REQUEST;
		tp->tty_iogrant = (cp_grant_id_t) m_ptr->IO_GRANT;
		r = SUSPEND;
		break;
	}
	if (m_ptr->TTY_REQUEST == TCDRAIN) break;
	if (m_ptr->TTY_REQUEST == TCSETSF) tty_icancel(tp);
	/*FALL THROUGH*/
    case TCSETS:
	/* Set the termios attributes. */
	r = sys_safecopyfrom(m_ptr->m_source, (cp_grant_id_t) m_ptr->IO_GRANT,
		0, (vir_bytes) &tp->tty_termios, (vir_bytes) size);
	if (r != OK) break;
	setattr(tp);
	break;

    case TCFLSH:
	r = sys_safecopyfrom(m_ptr->m_source, (cp_grant_id_t) m_ptr->IO_GRANT,
		0, (vir_bytes) &param.i, (vir_bytes) size);
	if (r != OK) break;
	switch (param.i) {
	    case TCIFLUSH:	tty_icancel(tp);		 	    break;
	    case TCOFLUSH:	(*tp->tty_ocancel)(tp, 0);		    break;
	    case TCIOFLUSH:	tty_icancel(tp); (*tp->tty_ocancel)(tp, 0); break;
	    default:		r = EINVAL;
	}
	break;

    case TCFLOW:
	r = sys_safecopyfrom(m_ptr->m_source, (cp_grant_id_t) m_ptr->IO_GRANT,
		0, (vir_bytes) &param.i, (vir_bytes) size);
	if (r != OK) break;
	switch (param.i) {
	    case TCOOFF:
	    case TCOON:
		tp->tty_inhibited = (param.i == TCOOFF);
		tp->tty_events = 1;
		break;
	    case TCIOFF:
		(*tp->tty_echo)(tp, tp->tty_termios.c_cc[VSTOP]);
		break;
	    case TCION:
		(*tp->tty_echo)(tp, tp->tty_termios.c_cc[VSTART]);
		break;
	    default:
		r = EINVAL;
	}
	break;

    case TCSBRK:
	if (tp->tty_break != NULL) (*tp->tty_break)(tp,0);
	break;

    case TIOCGWINSZ:
	r = sys_safecopyto(m_ptr->m_source, (cp_grant_id_t) m_ptr->IO_GRANT, 0,
		(vir_bytes) &tp->tty_winsize, (vir_bytes) size);
	break;

    case TIOCSWINSZ:
	r = sys_safecopyfrom(m_ptr->m_source, (cp_grant_id_t) m_ptr->IO_GRANT,
		0, (vir_bytes) &tp->tty_winsize, (vir_bytes) size);
	sigchar(tp, SIGWINCH, 0);
	break;

    case KIOCSMAP:
	/* Load a new keymap (only /dev/console). */
	if (isconsole(tp)) r = kbd_loadmap(m_ptr);
	break;

    case TIOCSFON_OLD:
	printf("TTY: old TIOCSFON ignored.\n");
	break;
    case TIOCSFON:
	/* Load a font into an EGA or VGA card (hs@hck.hr) */
	if (isconsole(tp)) r = con_loadfont(m_ptr);
	break;

/* These Posix functions are allowed to fail if _POSIX_JOB_CONTROL is 
 * not defined.
 */
    case TIOCGPGRP:     
    case TIOCSPGRP:	
    default:
	r = ENOTTY;
  }

  /* Send the reply. */
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->USER_ENDPT, r);
}

/*===========================================================================*
 *				do_open					     *
 *===========================================================================*/
static void do_open(tp, m_ptr)
register tty_t *tp;
message *m_ptr;			/* pointer to message sent to task */
{
/* A tty line has been opened.  Make it the callers controlling tty if
 * O_NOCTTY is *not* set and it is not the log device.  1 is returned if
 * the tty is made the controlling tty, otherwise OK or an error code.
 */
  int r = OK;

  if (m_ptr->TTY_LINE == LOG_MINOR) {
	/* The log device is a write-only diagnostics device. */
	if (m_ptr->COUNT & R_BIT) r = EACCES;
  } else {
	if (!(m_ptr->COUNT & O_NOCTTY)) {
		tp->tty_pgrp = m_ptr->USER_ENDPT;
		r = 1;
	}
	tp->tty_openct++;
	if (tp->tty_openct == 1) {
		/* Tell the device that the tty is opened */
		(*tp->tty_open)(tp, 0);
	}
  }
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->USER_ENDPT, r);
}

/*===========================================================================*
 *				do_close				     *
 *===========================================================================*/
static void do_close(tp, m_ptr)
register tty_t *tp;
message *m_ptr;			/* pointer to message sent to task */
{
/* A tty line has been closed.  Clean up the line if it is the last close. */

  if (m_ptr->TTY_LINE != LOG_MINOR && --tp->tty_openct == 0) {
	tp->tty_pgrp = 0;
	tty_icancel(tp);
	(*tp->tty_ocancel)(tp, 0);
	(*tp->tty_close)(tp, 0);
	tp->tty_termios = termios_defaults;
	tp->tty_winsize = winsize_defaults;
	setattr(tp);
  }
  tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->USER_ENDPT, OK);
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
static void do_cancel(tp, m_ptr)
register tty_t *tp;
message *m_ptr;			/* pointer to message sent to task */
{
/* A signal has been sent to a process that is hanging trying to read or write.
 * The pending read or write must be finished off immediately.
 */

  int proc_nr;
  int mode;
  int r = EINTR;

  /* Check the parameters carefully, to avoid cancelling twice. */
  proc_nr = m_ptr->USER_ENDPT;
  mode = m_ptr->COUNT;
  if ((mode & R_BIT) && tp->tty_inleft != 0 && proc_nr == tp->tty_inproc &&
	tp->tty_ingrant == (cp_grant_id_t) m_ptr->IO_GRANT) {
	/* Process was reading when killed.  Clean up input. */
	tty_icancel(tp); 
	r = tp->tty_incum > 0 ? tp->tty_incum : EAGAIN;
	tp->tty_inleft = tp->tty_incum = tp->tty_inrevived = 0;
	tp->tty_ingrant = GRANT_INVALID;
  } 
  if ((mode & W_BIT) && tp->tty_outleft != 0 && proc_nr == tp->tty_outproc &&
	tp->tty_outgrant == (cp_grant_id_t) m_ptr->IO_GRANT) {
	/* Process was writing when killed.  Clean up output. */
	r = tp->tty_outcum > 0 ? tp->tty_outcum : EAGAIN;
	tp->tty_outleft = tp->tty_outcum = tp->tty_outrevived = 0;
	tp->tty_outgrant = GRANT_INVALID;
  } 
  if (tp->tty_ioreq != 0 && proc_nr == tp->tty_ioproc) {
	/* Process was waiting for output to drain. */
	tp->tty_ioreq = 0;
  }
  tp->tty_events = 1;
  tty_reply(TASK_REPLY, m_ptr->m_source, proc_nr, r);
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

	if (ops & SEL_RD) {
		/* will i/o not block on read? */
		if (tp->tty_inleft > 0) {
			ready_ops |= SEL_RD;	/* EIO - no blocking */
		} else if (tp->tty_incount > 0) {
			/* Is a regular read possible? tty_incount
			 * says there is data. But a read will only succeed
			 * in canonical mode if a newline has been seen.
			 */
			if (!(tp->tty_termios.c_lflag & ICANON) ||
				tp->tty_eotct > 0) {
				ready_ops |= SEL_RD;
			}
		}
	}

	if (ops & SEL_WR)  {
  		if (tp->tty_outleft > 0)  ready_ops |= SEL_WR;
		else if ((*tp->tty_devwrite)(tp, 1)) ready_ops |= SEL_WR;
	}
	return ready_ops;
}

int select_retry(struct tty *tp)
{
  	if (tp->tty_select_ops && select_try(tp, tp->tty_select_ops))
		notify(tp->tty_select_proc);
	return OK;
}

/*===========================================================================*
 *				handle_events				     *
 *===========================================================================*/
void handle_events(tp)
tty_t *tp;			/* TTY to check for events. */
{
/* Handle any events pending on a TTY.  These events are usually device
 * interrupts.
 *
 * Two kinds of events are prominent:
 *	- a character has been received from the console or an RS232 line.
 *	- an RS232 line has completed a write request (on behalf of a user).
 * The interrupt handler may delay the interrupt message at its discretion
 * to avoid swamping the TTY task.  Messages may be overwritten when the
 * lines are fast or when there are races between different lines, input
 * and output, because MINIX only provides single buffering for interrupt
 * messages (in proc.c).  This is handled by explicitly checking each line
 * for fresh input and completed output on each interrupt.
 */

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
	if (tp->tty_inrepcode == TTY_REVIVE) {
		notify(tp->tty_incaller);
		tp->tty_inrevived = 1;
	} else {
		tty_reply(tp->tty_inrepcode, tp->tty_incaller, 
			tp->tty_inproc, tp->tty_incum);
		tp->tty_inleft = tp->tty_incum = 0;
		tp->tty_inrevived = 0;
		tp->tty_ingrant = GRANT_INVALID;
	}
  }
  if (tp->tty_select_ops)
  {
  	select_retry(tp);
  }
#if NR_PTYS > 0
  if (ispty(tp))
  	select_retry_pty(tp);
#endif
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
				tp->tty_ingrant, tp->tty_inoffset,
				(vir_bytes) buf,
				(vir_bytes) buflen(buf));
			tp->tty_inoffset += buflen(buf);
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
	sys_safecopyto(tp->tty_incaller,
		tp->tty_ingrant, tp->tty_inoffset,
		(vir_bytes) buf, (vir_bytes) count);
	tp->tty_inoffset += count;
	tp->tty_incum += count;
  }

  /* Usually reply to the reader, possibly even if incum == 0 (EOF). */
  if (tp->tty_inleft == 0) {
	if (tp->tty_inrepcode == TTY_REVIVE) {
		notify(tp->tty_incaller);
		tp->tty_inrevived = 1;
	} else {
		tty_reply(tp->tty_inrepcode, tp->tty_incaller, 
			tp->tty_inproc, tp->tty_incum);
		tp->tty_inleft = tp->tty_incum = 0;
		tp->tty_inrevived = 0;
		tp->tty_ingrant = GRANT_INVALID;
	}
  }
}

/*===========================================================================*
 *				in_process				     *
 *===========================================================================*/
static void in_process_send_byte(
  tty_t *tp,	/* terminal on which character has arrived */
  int ch	/* input character */
)
{
	/* Save the character in the input queue. */
	*tp->tty_inhead++ = ch;
	if (tp->tty_inhead == bufend(tp->tty_inbuf))
		tp->tty_inhead = tp->tty_inbuf;
	tp->tty_incount++;
	if (ch & IN_EOT) tp->tty_eotct++;

	/* Try to finish input if the queue threatens to overflow. */
	if (tp->tty_incount == buflen(tp->tty_inbuf)) in_transfer(tp);
}
 
int in_process(tp, buf, count, scode)
register tty_t *tp;		/* terminal on which character has arrived */
char *buf;			/* buffer with input characters */
int count;			/* number of input characters */
int scode;			/* scan code */
{
/* Characters have just been typed in.  Process, save, and echo them.  Return
 * the number of characters processed.
 */

  int ch, sig, ct;
  int timeset = FALSE;

  /* Send scancode if requested */
  if (tp->tty_termios.c_iflag & SCANCODES) {
	in_process_send_byte(tp, (scode & BYTE) | IN_EOT);
  }

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
		/* Check for INTR (^?) and QUIT (^\) characters. */
		if (ch == tp->tty_termios.c_cc[VINTR]
					|| ch == tp->tty_termios.c_cc[VQUIT]) {
			sig = SIGINT;
			if (ch == tp->tty_termios.c_cc[VQUIT]) sig = SIGQUIT;
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

	/* Send processed byte of input unless scancodes sent instead */
	if (!(tp->tty_termios.c_iflag & SCANCODES)) {
		in_process_send_byte(tp, ch);
	}
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

		if ((tp->tty_termios.c_oflag & (OPOST|XTABS))
							== (OPOST|XTABS)) {
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
/* The ioctl's TCSETSW, TCSETSF and TCDRAIN wait for output to finish to make
 * sure that an attribute change doesn't affect the processing of current
 * output.  Once output finishes the ioctl is executed as in do_ioctl().
 */
  int result = EINVAL;

  if (tp->tty_outleft > 0) return;		/* output not finished */

  if (tp->tty_ioreq != TCDRAIN) {
	if (tp->tty_ioreq == TCSETSF) tty_icancel(tp);
	result = sys_safecopyfrom(tp->tty_iocaller, tp->tty_iogrant, 0,
		(vir_bytes) &tp->tty_termios,
		(vir_bytes) sizeof(tp->tty_termios));
	if (result == OK) setattr(tp);
  }
  tp->tty_ioreq = 0;
  notify(tp->tty_iocaller);
  tp->tty_iorevived = 1;
  tp->tty_iostatus = result;
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

  /* SCANCODES is supported only for the console */
  if (!isconsole(tp)) tp->tty_termios.c_iflag &= ~SCANCODES;

  /* Set new line speed, character size, etc at the device level. */
  (*tp->tty_ioctl)(tp, 0);
}

/*===========================================================================*
 *				tty_reply				     *
 *===========================================================================*/
void 
tty_reply_f(
file, line, code, replyee, proc_nr, status)
char *file;
int line;
int code;			/* TASK_REPLY or REVIVE */
int replyee;			/* destination address for the reply */
int proc_nr;			/* to whom should the reply go? */
int status;			/* reply code */
{
/* Send a reply to a process that wanted to read or write data. */
  assert(code == TASK_REPLY);

  /* Don't reply to KERNEL (kernel messages) */
  if (replyee == KERNEL) return;

  status = send_taskreply(replyee, proc_nr, status);
  if (status != OK)
	printf("tty`tty_reply: send to %d failed: %d\n", replyee, status);
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
static void tty_init()
{
/* Initialize tty structure and call device initialization routines. */

  register tty_t *tp;
  int s;

  system_hz = sys_hz();

  /* Initialize the terminal lines. */
  memset(tty_table, '\0' , sizeof(tty_table));

  for (tp = FIRST_TTY,s=0; tp < END_TTY; tp++,s++) {

  	tp->tty_index = s;
  	init_timer(&tp->tty_tmr);

  	tp->tty_intail = tp->tty_inhead = tp->tty_inbuf;
  	tp->tty_min = 1;
	tp->tty_ingrant = tp->tty_outgrant = tp->tty_iogrant = GRANT_INVALID;
  	tp->tty_termios = termios_defaults;
  	tp->tty_icancel = tp->tty_ocancel = tp->tty_ioctl = tp->tty_close =
			  tp->tty_open = tty_devnop;
  	if (tp < tty_addr(NR_CONS)) {
		scr_init(tp);

		/* Initialize the keyboard driver. */
		kb_init(tp);

  		tp->tty_minor = CONS_MINOR + s;
  	} else
  	if (tp < tty_addr(NR_CONS+NR_RS_LINES)) {
		rs_init(tp);
  		tp->tty_minor = RS232_MINOR + s-NR_CONS;
  	} else {
		pty_init(tp);
		tp->tty_minor = s - (NR_CONS+NR_RS_LINES) + TTYPX_MINOR;
  	}
  }

}

/*===========================================================================*
 *				tty_timed_out				     *
 *===========================================================================*/
static void tty_timed_out(timer_t *tp)
{
/* This timer has expired. Set the events flag, to force processing. */
  tty_t *tty_ptr;
  tty_ptr = &tty_table[tmr_arg(tp)->ta_int];
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

/*===========================================================================*
 *				do_select				     *
 *===========================================================================*/
static void do_select(tp, m_ptr)
register tty_t *tp;		/* pointer to tty struct */
register message *m_ptr;	/* pointer to message sent to the task */
{
	int ops, ready_ops = 0, watch;

	ops = m_ptr->USER_ENDPT & (SEL_RD|SEL_WR|SEL_ERR);
	watch = (m_ptr->USER_ENDPT & SEL_NOTIFY) ? 1 : 0;

	ready_ops = select_try(tp, ops);

	if (!ready_ops && ops && watch) {
		tp->tty_select_ops |= ops;
		tp->tty_select_proc = m_ptr->m_source;
	}

        tty_reply(TASK_REPLY, m_ptr->m_source, m_ptr->USER_ENDPT, ready_ops);

        return;
}

