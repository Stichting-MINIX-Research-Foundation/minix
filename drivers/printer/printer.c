/* This file contains the printer driver. It is a fairly simple driver,
 * supporting only one printer.  Characters that are written to the driver
 * are written to the printer without any changes at all.
 *
 * Changes:
 *	May 07, 2004	fix: wait until printer is ready  (Jorrit N. Herder)
 *	May 06, 2004	printer driver moved to user-space  (Jorrit N. Herder) 
 *
 * The valid messages and their parameters are:
 *
 *   DEV_OPEN:	initializes the printer
 *   DEV_CLOSE:	does nothing
 *   HARD_INT:	interrupt handler has finished current chunk of output
 *   DEV_WRITE:	a process wants to write on a terminal
 *   CANCEL:	terminate a previous incomplete system call immediately
 *
 *    m_type      TTY_LINE   PROC_NR    COUNT    ADDRESS
 * |-------------+---------+---------+---------+---------|
 * | DEV_OPEN    |         |         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | DEV_CLOSE   |         | proc nr |         |         |
 * -------------------------------------------------------
 * | HARD_INT    |         |         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | SYS_EVENT   |         |         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | DEV_WRITE   |minor dev| proc nr |  count  | buf ptr |
 * |-------------+---------+---------+---------+---------|
 * | CANCEL      |minor dev| proc nr |         |         |
 * -------------------------------------------------------
 * 
 * Note: since only 1 printer is supported, minor dev is not used at present.
 */

#include "../drivers.h"

/* Control bits (in port_base + 2).  "+" means positive logic and "-" means
 * negative logic.  Most of the signals are negative logic on the pins but
 * many are converted to positive logic in the ports.  Some manuals are
 * misleading because they only document the pin logic.
 *
 *	+0x01	Pin 1	-Strobe
 *	+0x02	Pin 14	-Auto Feed
 *	-0x04	Pin 16	-Initialize Printer
 *	+0x08	Pin 17	-Select Printer
 *	+0x10	IRQ7 Enable
 *
 * Auto Feed and Select Printer are always enabled. Strobe is enabled briefly
 * when characters are output.  Initialize Printer is enabled briefly when
 * the task is started.  IRQ7 is enabled when the first character is output
 * and left enabled until output is completed (or later after certain
 * abnormal completions).
 */
#define ASSERT_STROBE   0x1D	/* strobe a character to the interface */
#define NEGATE_STROBE   0x1C	/* enable interrupt on interface */
#define PR_SELECT          0x0C	/* select printer bit */
#define INIT_PRINTER    0x08	/* init printer bits */

/* Status bits (in port_base + 2).
 *
 *	-0x08	Pin 15	-Error
 *	+0x10	Pin 13	+Select Status
 *	+0x20	Pin 12	+Out of Paper
 *	-0x40	Pin 10	-Acknowledge
 *	-0x80	Pin 11	+Busy
 */
#define BUSY_STATUS     0x10	/* printer gives this status when busy */
#define NO_PAPER        0x20	/* status bit saying that paper is out */
#define NORMAL_STATUS   0x90	/* printer gives this status when idle */
#define ON_LINE         0x10	/* status bit saying that printer is online */
#define STATUS_MASK	0xB0	/* mask to filter out status bits */ 

#define MAX_ONLINE_RETRIES 120  /* about 60s: waits 0.5s after each retry */

/* Centronics interface timing that must be met by software (in microsec).
 *
 * Strobe length:	0.5u to 100u (not sure about the upper limit).
 * Data set up:		0.5u before strobe.
 * Data hold:		0.5u after strobe.
 * Init pulse length:	over 200u (not sure).
 *
 * The strobe length is about 50u with the code here and function calls for
 * sys_outb() - not much to spare.  The 0.5u minimums will not be violated 
 * with the sys_outb() messages exchanged.
 */

PRIVATE int caller;		/* process to tell when printing done (FS) */
PRIVATE int revive_pending;	/* set to true if revive is pending */
PRIVATE int revive_status;	/* revive status */
PRIVATE int done_status;	/* status of last output completion */
PRIVATE int oleft;		/* bytes of output left in obuf */
PRIVATE char obuf[128];		/* output buffer */
PRIVATE char *optr;		/* ptr to next char in obuf to print */
PRIVATE int orig_count;		/* original byte count */
PRIVATE int port_base;		/* I/O port for printer */
PRIVATE int proc_nr;		/* user requesting the printing */
PRIVATE int user_left;		/* bytes of output left in user buf */
PRIVATE vir_bytes user_vir;	/* address of remainder of user buf */
PRIVATE int writing;		/* nonzero while write is in progress */
PRIVATE int irq_hook_id;	/* id of irq hook at kernel */

extern int errno;		/* error number */

FORWARD _PROTOTYPE( void do_cancel, (message *m_ptr) );
FORWARD _PROTOTYPE( void output_done, (void) );
FORWARD _PROTOTYPE( void do_write, (message *m_ptr) );
FORWARD _PROTOTYPE( void do_status, (message *m_ptr) );
FORWARD _PROTOTYPE( void prepare_output, (void) );
FORWARD _PROTOTYPE( void do_initialize, (void) );
FORWARD _PROTOTYPE( void reply, (int code,int replyee,int proc,int status));
FORWARD _PROTOTYPE( void do_printer_output, (void) );
FORWARD _PROTOTYPE( void signal_handler, (int sig) );

/*===========================================================================*
 *				 signal_handler                              *
 *===========================================================================*/
PRIVATE void signal_handler(sig)
int sig;					/* signal number */
{
/* Expect a SIGTERM signal when this server must shutdown. */
  if (sig == SIGTERM) {
  	printf("Shutting down PRINTER driver\n");
  	exit(0);
  } else {
  	printf("PRINTER got unknown signal\n");
  }
}

/*===========================================================================*
 *				printer_task				     *
 *===========================================================================*/
PUBLIC void main(void)
{
/* Main routine of the printer task. */
  message pr_mess;		/* buffer for all incoming messages */
  
  while (TRUE) {
	receive(ANY, &pr_mess);
	switch(pr_mess.m_type) {
	    case DEV_OPEN:
                 do_initialize();		/* initialize */
	        /* fall through */
	    case DEV_CLOSE:
		reply(TASK_REPLY, pr_mess.m_source, pr_mess.PROC_NR, OK);
		break;
	    case DEV_WRITE:	do_write(&pr_mess);	break;
	    case DEV_STATUS:	do_status(&pr_mess);	break;
	    case CANCEL:	do_cancel(&pr_mess);	break;
	    case HARD_INT:	do_printer_output();	break;
	    case SYS_SIG:	/* do nothing */	break;
	    default:
		reply(TASK_REPLY, pr_mess.m_source, pr_mess.PROC_NR, EINVAL);
	}
  }
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
PRIVATE void do_write(m_ptr)
register message *m_ptr;	/* pointer to the newly arrived message */
{
/* The printer is used by sending DEV_WRITE messages to it. Process one. */

    register int r = SUSPEND;
    int retries;
    int status;

    /* Reject command if last write is not yet finished, the count is not
     * positive, or the user address is bad.
     */
    if (writing)  			r = EIO;
    else if (m_ptr->COUNT <= 0)  	r = EINVAL;

    /* Reply to FS, no matter what happened, possible SUSPEND caller. */
    reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, r);

    /* If no errors occurred, continue printing with SUSPENDED caller.
     * First wait until the printer is online to prevent stupid errors.
     */
    if (SUSPEND == r) { 	
	caller = m_ptr->m_source;
	proc_nr = m_ptr->PROC_NR;
	user_left = m_ptr->COUNT;
	orig_count = m_ptr->COUNT;
	user_vir = (vir_bytes) m_ptr->ADDRESS;
	writing = TRUE;

        retries = MAX_ONLINE_RETRIES + 1;  
        while (--retries > 0) {
            sys_inb(port_base + 1, &status);
            if ((status & ON_LINE)) {		/* printer online! */
	        prepare_output();
	        do_printer_output();
	        return;
            }
            tickdelay(30);		/* wait before retry */
        }
        /* If we reach this point, the printer was not online in time. */
        done_status = status;
        output_done();
    }
}

/*===========================================================================*
 *				output_done					     *
 *===========================================================================*/
PRIVATE void output_done()
{
/* Previous chunk of printing is finished.  Continue if OK and more.
 * Otherwise, reply to caller (FS).
 */
    register int status;

    if (!writing) return;	  	/* probably leftover interrupt */
    if (done_status != OK) {      	/* printer error occurred */
        status = EIO;
	if ((done_status & ON_LINE) == 0) { 
	    printf("Printer is not on line\n");
	} else if ((done_status & NO_PAPER)) { 
	    printf("Printer is out of paper\n");
	    status = EAGAIN;	
	} else {
	    printf("Printer error, status is 0x%02X\n", done_status);
	}
	/* Some characters have been printed, tell how many. */
	if (status == EAGAIN && user_left < orig_count) {
		status = orig_count - user_left;
	}
	oleft = 0;			/* cancel further output */
    } 
    else if (user_left != 0) {		/* not yet done, continue! */
	prepare_output();
	return;
    } 
    else {				/* done! report back to FS */
	status = orig_count;
    }
#if DEAD_CODE
    reply(REVIVE, caller, proc_nr, status);
    writing = FALSE;
#else
    revive_pending = TRUE;
    revive_status = status;
    notify(caller);
#endif
}

/*===========================================================================*
 *				do_status				     *
 *===========================================================================*/
PRIVATE void do_status(m_ptr)
register message *m_ptr;	/* pointer to the newly arrived message */
{
  if (revive_pending) {
	m_ptr->m_type = DEV_REVIVE;		/* build message */
	m_ptr->REP_PROC_NR = proc_nr;
	m_ptr->REP_STATUS = revive_status;

	writing = FALSE;			/* unmark event */
	revive_pending = FALSE;			/* unmark event */
  } else {
	m_ptr->m_type = DEV_NO_STATUS;
  }
  send(m_ptr->m_source, m_ptr);			/* send the message */
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
PRIVATE void do_cancel(m_ptr)
register message *m_ptr;	/* pointer to the newly arrived message */
{
/* Cancel a print request that has already started.  Usually this means that
 * the process doing the printing has been killed by a signal.  It is not
 * clear if there are race conditions.  Try not to cancel the wrong process,
 * but rely on FS to handle the EINTR reply and de-suspension properly.
 */

  if (writing && m_ptr->PROC_NR == proc_nr) {
	oleft = 0;		/* cancel output by interrupt handler */
	writing = FALSE;
	revive_pending = FALSE;
  }
  reply(TASK_REPLY, m_ptr->m_source, m_ptr->PROC_NR, EINTR);
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
PRIVATE void reply(code, replyee, process, status)
int code;			/* TASK_REPLY or REVIVE */
int replyee;			/* destination for message (normally FS) */
int process;			/* which user requested the printing */
int status;			/* number of  chars printed or error code */
{
/* Send a reply telling FS that printing has started or stopped. */

  message pr_mess;

  pr_mess.m_type = code;		/* TASK_REPLY or REVIVE */
  pr_mess.REP_STATUS = status;		/* count or EIO */
  pr_mess.REP_PROC_NR = process;	/* which user does this pertain to */
  send(replyee, &pr_mess);		/* send the message */
}

/*===========================================================================*
 *				do_initialize				     *
 *===========================================================================*/
PRIVATE void do_initialize()
{
/* Set global variables and initialize the printer. */

  static int initialized = FALSE;
  if (initialized) return;
  initialized = TRUE;
  
  /* Get the base port for first printer.  */
  sys_vircopy(SELF, BIOS_SEG, LPT1_IO_PORT_ADDR, 
  	SELF, D, (vir_bytes) &port_base, LPT1_IO_PORT_SIZE);
  sys_outb(port_base + 2, INIT_PRINTER);
  tickdelay(1);			/* easily satisfies Centronics minimum */
  				/* was 2 millisecs; now is ~17 millisecs */
  sys_outb(port_base + 2, PR_SELECT);
  irq_hook_id = 0;
  sys_irqsetpolicy(PRINTER_IRQ, 0, &irq_hook_id);
  sys_irqenable(&irq_hook_id);

}

/*==========================================================================*
 *		    	      prepare_output				    *
 *==========================================================================*/
PRIVATE void prepare_output()
{
/* Start next chunk of printer output. Fetch the data from user space. */

  register int chunk;

  if ( (chunk = user_left) > sizeof obuf) chunk = sizeof obuf;
  if (OK!=sys_datacopy(proc_nr, user_vir, SELF, (vir_bytes) obuf, chunk)) {
  	done_status = EFAULT;
  	output_done();
  	return;
  }
  optr = obuf;
  oleft = chunk;
}

/*===========================================================================*
 *				do_printer_output				     *
 *===========================================================================*/
PRIVATE void do_printer_output()
{
/* This function does the actual output to the printer. This is called on
 * a HARD_INT message sent from the generic interrupt handler that 'forwards'
 * interrupts to this driver. The generic handler did not reenable the 
 * printer IRQ yet! 
 */

  int status;
  pvb_pair_t char_out[3];

  if (oleft == 0) {
	/* Nothing more to print.  Turn off printer interrupts in case they
	 * are level-sensitive as on the PS/2.  This should be safe even
	 * when the printer is busy with a previous character, because the
	 * interrupt status does not affect the printer.
	 */
	sys_outb(port_base + 2, PR_SELECT);
	sys_irqenable(&irq_hook_id);
	return;
  }

  do {
	/* Loop to handle fast (buffered) printers.  It is important that
	 * processor interrupts are not disabled here, just printer interrupts.
	 */
	(void) sys_inb(port_base + 1, &status);
	if ((status & STATUS_MASK) == BUSY_STATUS) {
		/* Still busy with last output.  This normally happens
		 * immediately after doing output to an unbuffered or slow
		 * printer.  It may happen after a call from prepare_output or
		 * pr_restart, since they are not synchronized with printer
		 * interrupts.  It may happen after a spurious interrupt.
		 */
		sys_irqenable(&irq_hook_id);
		return;
	}
	if ((status & STATUS_MASK) == NORMAL_STATUS) {
		/* Everything is all right.  Output another character. */
		pv_set(char_out[0], port_base, *optr++);	
		pv_set(char_out[1], port_base+2, ASSERT_STROBE);
		pv_set(char_out[2], port_base+2, NEGATE_STROBE);
		sys_voutb(char_out, 3);	/* request series of port outb */

		user_vir++;
		user_left--;
	} else {
		/* Error.  This would be better ignored (treat as busy). */
		done_status = status;
		output_done();
		sys_irqenable(&irq_hook_id);
		return;
	}
  }
  while (--oleft != 0);

  /* Finished printing chunk OK. */
  done_status = OK;
  output_done();
  sys_irqenable(&irq_hook_id);
}

