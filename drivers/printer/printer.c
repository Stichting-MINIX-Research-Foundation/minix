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
 *    m_type       DEVICE   USER_ENDPT  COUNT    ADDRESS
 * |-------------+---------+---------+---------+---------|
 * | DEV_OPEN    |         |         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | DEV_CLOSE   |         | proc nr |         |         |
 * |-------------+---------+---------+---------+---------|
 * | HARD_INT    |         |         |         |         |
 * |-------------+---------+---------+---------+---------|
 * | DEV_WRITE   |minor dev| proc nr |  count  | buf ptr |
 * |-------------+---------+---------+---------+---------|
 * | CANCEL      |minor dev| proc nr |         |         |
 * -------------------------------------------------------
 * 
 * Note: since only 1 printer is supported, minor dev is not used at present.
 */

#include <minix/endpoint.h>
#include <minix/drivers.h>
#include <minix/chardriver.h>

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

static endpoint_t caller;	/* process to tell when printing done (FS) */
static int done_status;	/* status of last output completion */
static int oleft;		/* bytes of output left in obuf */
static unsigned char obuf[128];	/* output buffer */
static unsigned const char *optr;	/* ptr to next char in obuf to print */
static int orig_count;		/* original byte count */
static int port_base;		/* I/O port for printer */
static endpoint_t proc_nr;	/* user requesting the printing */
static cp_grant_id_t grant_nr;	/* grant on which print happens */
static int user_left;		/* bytes of output left in user buf */
static vir_bytes user_vir_d;	/* offset in user buf */
int writing;		/* nonzero while write is in progress */
static int irq_hook_id;	/* id of irq hook at kernel */

static void do_cancel(message *m_ptr);
static void output_done(void);
static void do_write(message *m_ptr);
static void prepare_output(void);
static int do_probe(void);
static void do_initialize(void);
static void reply(int code, endpoint_t replyee, endpoint_t proc,
	cp_grant_id_t grant, int status);
static void do_printer_output(void);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init_fresh(int type, sef_init_info_t *info);
EXTERN int sef_cb_lu_prepare(int state);
EXTERN int sef_cb_lu_state_isvalid(int state);
EXTERN void sef_cb_lu_state_dump(int state);

/*===========================================================================*
 *				printer_task				     *
 *===========================================================================*/
int main(void)
{
/* Main routine of the printer task. */
  message pr_mess;		/* buffer for all incoming messages */
  int ipc_status;

  /* SEF local startup. */
  sef_local_startup();

  while (TRUE) {
	if(driver_receive(ANY, &pr_mess, &ipc_status) != OK) {
		panic("driver_receive failed");
	}

	if (is_ipc_notify(ipc_status)) {
		switch (_ENDPOINT_P(pr_mess.m_source)) {
			case HARDWARE:
				do_printer_output();
				break;
		}
		continue;
	}

	switch(pr_mess.m_type) {
	    case DEV_OPEN:
                do_initialize();		/* initialize */
		reply(DEV_OPEN_REPL, pr_mess.m_source, pr_mess.USER_ENDPT,
			(cp_grant_id_t) pr_mess.IO_GRANT, OK);
		break;
	    case DEV_CLOSE:
		reply(DEV_CLOSE_REPL, pr_mess.m_source, pr_mess.USER_ENDPT,
			(cp_grant_id_t) pr_mess.IO_GRANT, OK);
		break;
	    case DEV_WRITE_S:	do_write(&pr_mess);	break;
	    case CANCEL:	do_cancel(&pr_mess);	break;
	    default:
		reply(DEV_REVIVE, pr_mess.m_source, pr_mess.USER_ENDPT,
			(cp_grant_id_t) pr_mess.IO_GRANT, EINVAL);
	}
  }
}

/*===========================================================================*
 *			       sef_local_startup			     *
 *===========================================================================*/
static void sef_local_startup()
{
  /* Register init callbacks. */
  sef_setcb_init_fresh(sef_cb_init_fresh);
  sef_setcb_init_lu(sef_cb_init_fresh);
  sef_setcb_init_restart(sef_cb_init_fresh);

  /* Register live update callbacks. */
  sef_setcb_lu_prepare(sef_cb_lu_prepare);
  sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid);
  sef_setcb_lu_state_dump(sef_cb_lu_state_dump);

  /* Register signal callbacks. */
  sef_setcb_signal_handler(sef_cb_signal_handler_term);

  /* Let SEF perform startup. */
  sef_startup();
}

/*===========================================================================*
 *		            sef_cb_init_fresh                                *
 *===========================================================================*/
static int sef_cb_init_fresh(int UNUSED(type), sef_init_info_t *UNUSED(info))
{
/* Initialize the printer driver. */

  /* If no printer is present, do not start. */
  if (!do_probe())
	return ENODEV;	/* arbitrary error code */

  /* Announce we are up! */
  chardriver_announce();

  return OK;
}

/*===========================================================================*
 *				do_write				     *
 *===========================================================================*/
static void do_write(m_ptr)
register message *m_ptr;	/* pointer to the newly arrived message */
{
/* The printer is used by sending DEV_WRITE messages to it. Process one. */

    int r = OK;
    int retries;
    u32_t status;

    /* Reject command if last write is not yet finished, the count is not
     * positive, or the user address is bad.
     */
    if (writing)  				r = EIO;
    else if (m_ptr->COUNT <= 0)  		r = EINVAL;
    else if (m_ptr->FLAGS & FLG_OP_NONBLOCK)	r = EAGAIN; /* not supported */

    /* If no errors occurred, continue printing with the caller.
     * First wait until the printer is online to prevent stupid errors.
     */
    if (r == OK) {
	caller = m_ptr->m_source;
	proc_nr = m_ptr->USER_ENDPT;
	user_left = m_ptr->COUNT;
	orig_count = m_ptr->COUNT;
	user_vir_d = 0;				 	/* Offset. */
	writing = TRUE;
	grant_nr = (cp_grant_id_t) m_ptr->IO_GRANT;

        retries = MAX_ONLINE_RETRIES + 1;  
        while (--retries > 0) {
            if(sys_inb(port_base + 1, &status) != OK) {
		printf("printer: sys_inb of %x failed\n", port_base+1);
		panic("sys_inb failed");
	    }
            if ((status & ON_LINE)) {		/* printer online! */
	        prepare_output();
	        do_printer_output();
	        return;
            }
            micro_delay(500000);		/* wait before retry */
        }
        /* If we reach this point, the printer was not online in time. */
        done_status = status;
        output_done();
    } else {
	reply(DEV_REVIVE, m_ptr->m_source, m_ptr->USER_ENDPT,
		(cp_grant_id_t) m_ptr->IO_GRANT, r);
    }
}

/*===========================================================================*
 *				output_done				     *
 *===========================================================================*/
static void output_done()
{
/* Previous chunk of printing is finished.  Continue if OK and more.
 * Otherwise, reply to caller (FS).
 */
    register int status;
    message m;

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

    memset(&m, 0, sizeof(m));
    m.m_type = DEV_REVIVE;		/* build message */
    m.REP_ENDPT = proc_nr;
    m.REP_STATUS = status;
    m.REP_IO_GRANT = grant_nr;
    send(caller, &m);

    writing = FALSE;			/* unmark event */
}

/*===========================================================================*
 *				do_cancel				     *
 *===========================================================================*/
static void do_cancel(m_ptr)
register message *m_ptr;	/* pointer to the newly arrived message */
{
/* Cancel a print request that has already started.  Usually this means that
 * the process doing the printing has been killed by a signal.  It is not
 * clear if there are race conditions.  Try not to cancel the wrong process,
 * but rely on FS to handle the EINTR reply and de-suspension properly.
 */

  if (writing && m_ptr->USER_ENDPT == proc_nr &&
		(cp_grant_id_t) m_ptr->IO_GRANT == grant_nr) {
	oleft = 0;		/* cancel output by interrupt handler */
	writing = FALSE;
	reply(DEV_REVIVE, m_ptr->m_source, proc_nr, grant_nr, EINTR);
  }
}

/*===========================================================================*
 *				reply					     *
 *===========================================================================*/
static void reply(code, replyee, process, grant, status)
int code;			/* DEV_OPEN_REPL, DEV_CLOSE_REPL, DEV_REVIVE */
endpoint_t replyee;		/* destination for message (normally FS) */
endpoint_t process;		/* which user requested the printing */
cp_grant_id_t grant;		/* which grant was involved */
int status;			/* number of  chars printed or error code */
{
/* Send a reply telling FS that printing has started or stopped. */

  message pr_mess;

  pr_mess.m_type = code;		/* reply code */
  pr_mess.REP_STATUS = status;		/* count or EIO */
  pr_mess.REP_ENDPT = process;		/* which user does this pertain to */
  pr_mess.REP_IO_GRANT = grant;		/* the grant */
  send(replyee, &pr_mess);		/* send the message */
}

/*===========================================================================*
 *				do_probe				     *
 *===========================================================================*/
static int do_probe(void)
{
/* See if there is a printer at all. */

  /* Get the base port for first printer. */
  if(sys_readbios(LPT1_IO_PORT_ADDR, &port_base, LPT1_IO_PORT_SIZE) != OK) {
	panic("do_initialize: sys_readbios failed");
  }

  /* If the port is zero, the parallel port is not available at all. */
  return (port_base != 0);
}

/*===========================================================================*
 *				do_initialize				     *
 *===========================================================================*/
static void do_initialize()
{
/* Set global variables and initialize the printer. */
  static int initialized = FALSE;
  if (initialized) return;
  initialized = TRUE;
  
  if(sys_outb(port_base + 2, INIT_PRINTER) != OK) {
	printf("printer: sys_outb of %x failed\n", port_base+2);
	panic("do_initialize: sys_outb init failed");
  }
  micro_delay(1000000/20);	/* easily satisfies Centronics minimum */
  if(sys_outb(port_base + 2, PR_SELECT) != OK) {
	printf("printer: sys_outb of %x failed\n", port_base+2);
	panic("do_initialize: sys_outb select failed");
  }
  irq_hook_id = 0;
  if(sys_irqsetpolicy(PRINTER_IRQ, 0, &irq_hook_id) != OK ||
     sys_irqenable(&irq_hook_id) != OK) {
	panic("do_initialize: irq enabling failed");
  }
}

/*==========================================================================*
 *		    	      prepare_output				    *
 *==========================================================================*/
static void prepare_output()
{
/* Start next chunk of printer output. Fetch the data from user space. */
  int s;
  register int chunk;

  if ( (chunk = user_left) > sizeof obuf) chunk = sizeof obuf;

  s=sys_safecopyfrom(caller, grant_nr, user_vir_d, (vir_bytes) obuf,
	chunk);

  if(s != OK) {
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
static void do_printer_output()
{
/* This function does the actual output to the printer. This is called on an
 * interrupt message sent from the generic interrupt handler that 'forwards'
 * interrupts to this driver. The generic handler did not reenable the printer
 * IRQ yet! 
 */

  u32_t status;
  pvb_pair_t char_out[3];

  if (oleft == 0) {
	/* Nothing more to print.  Turn off printer interrupts in case they
	 * are level-sensitive as on the PS/2.  This should be safe even
	 * when the printer is busy with a previous character, because the
	 * interrupt status does not affect the printer.
	 */
	if(sys_outb(port_base + 2, PR_SELECT) != OK) {
		printf("printer: sys_outb of %x failed\n", port_base+2);
		panic("sys_outb failed");
	}
	if(sys_irqenable(&irq_hook_id) != OK) {
		panic("sys_irqenable failed");
	}
	return;
  }

  do {
	/* Loop to handle fast (buffered) printers.  It is important that
	 * processor interrupts are not disabled here, just printer interrupts.
	 */
	if(sys_inb(port_base + 1, &status) != OK) {
		printf("printer: sys_inb of %x failed\n", port_base+1);
		panic("sys_inb failed");
	}
	if ((status & STATUS_MASK) == BUSY_STATUS) {
		/* Still busy with last output.  This normally happens
		 * immediately after doing output to an unbuffered or slow
		 * printer.  It may happen after a call from prepare_output or
		 * pr_restart, since they are not synchronized with printer
		 * interrupts.  It may happen after a spurious interrupt.
		 */
		if(sys_irqenable(&irq_hook_id) != OK) {
			panic("sys_irqenable failed");
		}
		return;
	}
	if ((status & STATUS_MASK) == NORMAL_STATUS) {
		/* Everything is all right.  Output another character. */
		pv_set(char_out[0], port_base, *optr);	
		optr++;
		pv_set(char_out[1], port_base+2, ASSERT_STROBE);
		pv_set(char_out[2], port_base+2, NEGATE_STROBE);
		if(sys_voutb(char_out, 3) != OK) {
			/* request series of port outb */
			panic("sys_voutb failed");
		}

		user_vir_d++;
		user_left--;
	} else {
		/* Error.  This would be better ignored (treat as busy). */
		done_status = status;
		output_done();
		if(sys_irqenable(&irq_hook_id) != OK) {
			panic("sys_irqenable failed");
		}
		return;
	}
  }
  while (--oleft != 0);

  /* Finished printing chunk OK. */
  done_status = OK;
  output_done();
  if(sys_irqenable(&irq_hook_id) != OK) {
	panic("sys_irqenable failed");
  }
}

