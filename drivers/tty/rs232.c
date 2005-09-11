#include <minix/config.h>
/*---------------------------------------------------------------------------*
 *		rs232.c - serial driver for 8250 and 16450 UARTs	     *
 *		Added support for Atari ST M68901 and YM-2149	--kub	     *
 *---------------------------------------------------------------------------*/

#include "../drivers.h"
#include <termios.h>
#include <signal.h>
#include "tty.h"

#if NR_RS_LINES > 0

#if (MACHINE != IBM_PC) && (MACHINE != ATARI)
#error				/* rs232.c only supports PC and Atari ST */
#endif

#if (MACHINE == ATARI)
#include "staddr.h"
#include "stsound.h"
#include "stmfp.h"
#if (NR_RS_LINES > 1)
#error				/* Only one physical RS232 line available */
#endif
#endif

#if (MACHINE == IBM_PC)		/* PC/AT 8250/16450 chip combination */

/* 8250 constants. */
#define UART_FREQ         115200L	/* timer frequency */

/* Interrupt enable bits. */
#define IE_RECEIVER_READY       1
#define IE_TRANSMITTER_READY    2
#define IE_LINE_STATUS_CHANGE   4
#define IE_MODEM_STATUS_CHANGE  8

/* Interrupt status bits. */
#define IS_MODEM_STATUS_CHANGE  0
#define IS_TRANSMITTER_READY    2
#define IS_RECEIVER_READY       4
#define IS_LINE_STATUS_CHANGE   6

/* Line control bits. */
#define LC_2STOP_BITS        0x04
#define LC_PARITY            0x08
#define LC_PAREVEN           0x10
#define LC_BREAK             0x40
#define LC_ADDRESS_DIVISOR   0x80

/* Line status bits. */
#define LS_OVERRUN_ERR          2
#define LS_PARITY_ERR           4
#define LS_FRAMING_ERR          8
#define LS_BREAK_INTERRUPT   0x10
#define LS_TRANSMITTER_READY 0x20

/* Modem control bits. */
#define MC_DTR                  1
#define MC_RTS                  2
#define MC_OUT2                 8	/* required for PC & AT interrupts */

/* Modem status bits. */
#define MS_CTS               0x10
#define MS_RLSD              0x80       /* Received Line Signal Detect */
#define MS_DRLSD             0x08       /* RLSD Delta */

#else /* MACHINE == ATARI */		/* Atari ST 68901 USART */

/* Most of the USART constants are already defined in stmfp.h . The local
 * definitions made here are for keeping C code changes smaller.   --kub
 */

#define UART_FREQ          19200L	/* timer frequency */

/* Line status bits. */
#define LS_OVERRUN_ERR       R_OE
#define LS_PARITY_ERR        R_PE
#define LS_FRAMING_ERR       R_FE
#define LS_BREAK_INTERRUPT   R_BREAK

/* Modem status bits. */
#define MS_CTS               IO_SCTS	/* 0x04 */

#endif /* MACHINE == ATARI */

#define DATA_BITS_SHIFT         8	/* amount data bits shifted in mode */
#define DEF_BAUD             1200	/* default baud rate */

#define RS_IBUFSIZE          1024	/* RS232 input buffer size */
#define RS_OBUFSIZE          1024	/* RS232 output buffer size */

/* Input buffer watermarks.
 * The external device is asked to stop sending when the buffer
 * exactly reaches high water, or when TTY requests it.  Sending restarts
 * when the input buffer empties below the low watermark.
 */
#define RS_ILOWWATER	(1 * RS_IBUFSIZE / 4)
#define RS_IHIGHWATER	(3 * RS_IBUFSIZE / 4)

/* Output buffer low watermark.
 * TTY is notified when the output buffer empties below the low watermark, so
 * it may continue filling the buffer if doing a large write.
 */
#define RS_OLOWWATER	(1 * RS_OBUFSIZE / 4)

#if (MACHINE == IBM_PC)

/* Macros to handle flow control.
 * Interrupts must be off when they are used.
 * Time is critical - already the function call for outb() is annoying.
 * If outb() can be done in-line, tests to avoid it can be dropped.
 * istart() tells external device we are ready by raising RTS.
 * istop() tells external device we are not ready by dropping RTS.
 * DTR is kept high all the time (it probably should be raised by open and
 * dropped by close of the device).
 * OUT2 is also kept high all the time.
 */
#define istart(rs) \
	(sys_outb((rs)->modem_ctl_port, MC_OUT2 | MC_RTS | MC_DTR), \
		(rs)->idevready = TRUE)
#define istop(rs) \
	(sys_outb((rs)->modem_ctl_port, MC_OUT2 | MC_DTR), \
		(rs)->idevready = FALSE)

/* Macro to tell if device is ready.  The rs->cts field is set to MS_CTS if
 * CLOCAL is in effect for a line without a CTS wire.
 */
#define devready(rs) ((my_inb(rs->modem_status_port) | rs->cts) & MS_CTS)

/* Macro to tell if transmitter is ready. */
#define txready(rs) (my_inb(rs->line_status_port) & LS_TRANSMITTER_READY)

/* Macro to tell if carrier has dropped.
 * The RS232 Carrier Detect (CD) line is usually connected to the 8250
 * Received Line Signal Detect pin, reflected by bit MS_RLSD in the Modem
 * Status Register.  The MS_DRLSD bit tells if MS_RLSD has just changed state.
 * So if MS_DRLSD is set and MS_RLSD cleared, we know that carrier has just
 * dropped.
 */
#define devhup(rs)	\
	((my_inb(rs->modem_status_port) & (MS_RLSD|MS_DRLSD)) == MS_DRLSD)

#else /* MACHINE == ATARI */

/* Macros to handle flow control.
 * Time is critical - already the function call for lock()/restore() is
 * annoying.
 * istart() tells external device we are ready by raising RTS.
 * istop() tells external device we are not ready by dropping RTS.
 * DTR is kept high all the time (it probably should be raised by open and
 * dropped by close of the device). NOTE: The modem lines are active low.
 */
#define set_porta(msk,val) { register int s = lock();		\
			     SOUND->sd_selr = YM_IOA;		\
			     SOUND->sd_wdat =			\
				SOUND->sd_rdat & (msk) | (val);	\
			     restore(s);	}
#define istart(rs)         { set_porta( ~(PA_SRTS|PA_SDTR),0 ); \
			     (rs)->idevready = TRUE;	}
#define istop(rs)          { set_porta( ~PA_SDTR, PA_SRTS );	\
			     (rs)->idevready = FALSE;	}

/* Macro to tell if device is ready.  The rs->cts field is set to MS_CTS if
 * CLOCAL is in effect for a line without a CTS wire.
 */
#define devready(rs)         ((~MFP->mf_gpip | rs->cts) & MS_CTS)

/* Transmitter ready test */
#define txready(rs)          (MFP->mf_tsr & (T_EMPTY | T_UE))

#endif /* MACHINE == ATARI */

/* Types. */
typedef unsigned char bool_t;	/* boolean */

/* RS232 device structure, one per device. */
typedef struct rs232 {
  tty_t *tty;			/* associated TTY structure */

  int icount;			/* number of bytes in the input buffer */
  char *ihead;			/* next free spot in input buffer */
  char *itail;			/* first byte to give to TTY */
  bool_t idevready;		/* nonzero if we are ready to receive (RTS) */
  char cts;			/* normally 0, but MS_CTS if CLOCAL is set */

  unsigned char ostate;		/* combination of flags: */
#define ODONE          1	/* output completed (< output enable bits) */
#define ORAW           2	/* raw mode for xoff disable (< enab. bits) */
#define OWAKEUP        4	/* tty_wakeup() pending (asm code only) */
#define ODEVREADY MS_CTS	/* external device hardware ready (CTS) */
#define OQUEUED     0x20	/* output buffer not empty */
#define OSWREADY    0x40	/* external device software ready (no xoff) */
#define ODEVHUP  MS_RLSD	/* external device has dropped carrier */
#define OSOFTBITS  (ODONE | ORAW | OWAKEUP | OQUEUED | OSWREADY)
				/* user-defined bits */
#if (OSOFTBITS | ODEVREADY | ODEVHUP) == OSOFTBITS
				/* a weak sanity check */
#error				/* bits are not unique */
#endif
  unsigned char oxoff;		/* char to stop output */
  bool_t inhibited;		/* output inhibited? (follows tty_inhibited) */
  bool_t drain;			/* if set drain output and reconfigure line */
  int ocount;			/* number of bytes in the output buffer */
  char *ohead;			/* next free spot in output buffer */
  char *otail;			/* next char to output */

#if (MACHINE == IBM_PC)
  port_t xmit_port;		/* i/o ports */
  port_t recv_port;
  port_t div_low_port;
  port_t div_hi_port;
  port_t int_enab_port;
  port_t int_id_port;
  port_t line_ctl_port;
  port_t modem_ctl_port;
  port_t line_status_port;
  port_t modem_status_port;
#endif

  unsigned char lstatus;	/* last line status */
  unsigned char pad;		/* ensure alignment for 16-bit ints */
  unsigned framing_errors;	/* error counts (no reporting yet) */
  unsigned overrun_errors;
  unsigned parity_errors;
  unsigned break_interrupts;

  int irq;			/* irq for this line */
  int irq_hook_id;		/* interrupt hook */

  char ibuf[RS_IBUFSIZE];	/* input buffer */
  char obuf[RS_OBUFSIZE];	/* output buffer */
} rs232_t;

PUBLIC rs232_t rs_lines[NR_RS_LINES];

/* Table and macro to translate an RS232 line number to its rs_lines entry. */
PRIVATE rs232_t *p_rs_addr[NR_RS_LINES];

#define rs_addr(line)	(p_rs_addr[line])

#if (MACHINE == IBM_PC)
/* 8250 base addresses. */
PRIVATE port_t addr_8250[] = {
  0x3F8,	/* COM1 */
  0x2F8,	/* COM2 */
  0x3E8,	/* COM3 */
  0x2E8,	/* COM4 */
};
#endif

FORWARD _PROTOTYPE( void in_int, (rs232_t *rs)				);
FORWARD _PROTOTYPE( void line_int, (rs232_t *rs)			);
FORWARD _PROTOTYPE( void modem_int, (rs232_t *rs)			);
FORWARD _PROTOTYPE( int rs_write, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( void rs_echo, (tty_t *tp, int c)			);
FORWARD _PROTOTYPE( int rs_ioctl, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( void rs_config, (rs232_t *rs)			);
FORWARD _PROTOTYPE( int rs_read, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( int rs_icancel, (tty_t *tp, int try)		);
FORWARD _PROTOTYPE( int rs_ocancel, (tty_t *tp, int try)		);
FORWARD _PROTOTYPE( void rs_ostart, (rs232_t *rs)			);
FORWARD _PROTOTYPE( int rs_break, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( int rs_close, (tty_t *tp, int try)			);
FORWARD _PROTOTYPE( void out_int, (rs232_t *rs)				);
FORWARD _PROTOTYPE( void rs232_handler, (rs232_t *rs)			);

/* XXX */
PRIVATE void lock(void) {}
PRIVATE void unlock(void) {}

PRIVATE int my_inb(port_t port)
{
	int r, v = 0;
	r = sys_inb(port, &v);
	if (r != OK)
		printf("RS232 warning: failed inb 0x%x\n", port);

	return v;
}

/*===========================================================================*
 *				rs_write				     *
 *===========================================================================*/
PRIVATE int rs_write(tp, try)
register tty_t *tp;
int try;
{
/* (*devwrite)() routine for RS232. */

  rs232_t *rs = tp->tty_priv;
  int count, ocount;

  if (rs->inhibited != tp->tty_inhibited) {
	/* Inhibition state has changed. */
	lock();
	rs->ostate |= OSWREADY;
	if (tp->tty_inhibited) rs->ostate &= ~OSWREADY;
	unlock();
	rs->inhibited = tp->tty_inhibited;
  }

  if (rs->drain) {
	/* Wait for the line to drain then reconfigure and continue output. */
	if (rs->ocount > 0) return 0;
	rs->drain = FALSE;
	rs_config(rs);
  }

  /* While there is something to do. */
  for (;;) {
	ocount = buflen(rs->obuf) - rs->ocount;
	count = bufend(rs->obuf) - rs->ohead;
	if (count > ocount) count = ocount;
	if (count > tp->tty_outleft) count = tp->tty_outleft;
	if (count == 0 || tp->tty_inhibited) {
		if (try) return 0;
		break;
	}
	if (try) return 1;

	/* Copy from user space to the RS232 output buffer. */
	sys_vircopy(tp->tty_outproc, D, (vir_bytes) tp->tty_out_vir, 
		SELF, D, (vir_bytes) rs->ohead, (phys_bytes) count);

	/* Perform output processing on the output buffer. */
	out_process(tp, rs->obuf, rs->ohead, bufend(rs->obuf), &count, &ocount);
	if (count == 0) break;

	/* Assume echoing messed up by output. */
	tp->tty_reprint = TRUE;

	/* Bookkeeping. */
	lock();			/* protect interrupt sensitive rs->ocount */
	rs->ocount += ocount;
	rs_ostart(rs);
	unlock();
	if ((rs->ohead += ocount) >= bufend(rs->obuf))
		rs->ohead -= buflen(rs->obuf);
	tp->tty_out_vir += count;
	tp->tty_outcum += count;
	if ((tp->tty_outleft -= count) == 0) {
		/* Output is finished, reply to the writer. */
		tty_reply(tp->tty_outrepcode, tp->tty_outcaller,
					tp->tty_outproc, tp->tty_outcum);
		tp->tty_outcum = 0;
	}
  }
  if (tp->tty_outleft > 0 && tp->tty_termios.c_ospeed == B0) {
	/* Oops, the line has hung up. */
	tty_reply(tp->tty_outrepcode, tp->tty_outcaller, tp->tty_outproc, EIO);
	tp->tty_outleft = tp->tty_outcum = 0;
  }

  return 1;
}

/*===========================================================================*
 *				rs_echo					     *
 *===========================================================================*/
PRIVATE void rs_echo(tp, c)
tty_t *tp;			/* which TTY */
int c;				/* character to echo */
{
/* Echo one character.  (Like rs_write, but only one character, optionally.) */

  rs232_t *rs = tp->tty_priv;
  int count, ocount;

  ocount = buflen(rs->obuf) - rs->ocount;
  if (ocount == 0) return;		/* output buffer full */
  count = 1;
  *rs->ohead = c;			/* add one character */

  out_process(tp, rs->obuf, rs->ohead, bufend(rs->obuf), &count, &ocount);
  if (count == 0) return;

  lock();
  rs->ocount += ocount;
  rs_ostart(rs);
  unlock();
  if ((rs->ohead += ocount) >= bufend(rs->obuf)) rs->ohead -= buflen(rs->obuf);
}

/*===========================================================================*
 *				rs_ioctl				     *
 *===========================================================================*/
PRIVATE int rs_ioctl(tp, dummy)
tty_t *tp;			/* which TTY */
int dummy;
{
/* Reconfigure the line as soon as the output has drained. */
  rs232_t *rs = tp->tty_priv;

  rs->drain = TRUE;
  return 0;	/* dummy */
}

/*===========================================================================*
 *				rs_config				     *
 *===========================================================================*/
PRIVATE void rs_config(rs)
rs232_t *rs;			/* which line */
{
/* Set various line control parameters for RS232 I/O.
 * If DataBits == 5 and StopBits == 2, 8250 will generate 1.5 stop bits.
 * The 8250 can't handle split speed, so we use the input speed.
 */

  tty_t *tp = rs->tty;
  int divisor;
  int line_controls;
  static struct speed2divisor {
	speed_t	speed;
	int	divisor;
  } s2d[] = {
#if (MACHINE == IBM_PC)
	{ B50,		UART_FREQ / 50		},
#endif
	{ B75,		UART_FREQ / 75		},
	{ B110,		UART_FREQ / 110		},
	{ B134,		UART_FREQ * 10 / 1345	},
	{ B150,		UART_FREQ / 150		},
	{ B200,		UART_FREQ / 200		},
	{ B300,		UART_FREQ / 300		},
	{ B600,		UART_FREQ / 600		},
	{ B1200,	UART_FREQ / 1200	},
#if (MACHINE == IBM_PC)
	{ B1800,	UART_FREQ / 1800	},
#endif
	{ B2400,	UART_FREQ / 2400	},
	{ B4800,	UART_FREQ / 4800	},
	{ B9600,	UART_FREQ / 9600	},
	{ B19200,	UART_FREQ / 19200	},
#if (MACHINE == IBM_PC)
	{ B38400,	UART_FREQ / 38400	},
	{ B57600,	UART_FREQ / 57600	},
	{ B115200,	UART_FREQ / 115200L	},
#endif
  };
  struct speed2divisor *s2dp;

  /* RS232 needs to know the xoff character, and if CTS works. */
  rs->oxoff = tp->tty_termios.c_cc[VSTOP];
  rs->cts = (tp->tty_termios.c_cflag & CLOCAL) ? MS_CTS : 0;

  /* Look up the 8250 rate divisor from the output speed. */
  divisor = 0;
  for (s2dp = s2d; s2dp < s2d + sizeof(s2d)/sizeof(s2d[0]); s2dp++) {
	if (s2dp->speed == tp->tty_termios.c_ospeed) divisor = s2dp->divisor;
  }
  if (divisor == 0) return;	/* B0? */

#if (MACHINE == IBM_PC)
  /* Compute line control flag bits. */
  line_controls = 0;
  if (tp->tty_termios.c_cflag & PARENB) {
	line_controls |= LC_PARITY;
	if (!(tp->tty_termios.c_cflag & PARODD)) line_controls |= LC_PAREVEN;
  }
  if (divisor >= (UART_FREQ / 110)) line_controls |= LC_2STOP_BITS;
  line_controls |= (tp->tty_termios.c_cflag & CSIZE) >> 2;

  /* Lock out interrupts while setting the speed. The receiver register is
   * going to be hidden by the div_low register, but the input interrupt
   * handler relies on reading it to clear the interrupt and avoid looping
   * forever.
   */
  lock();

  /* Select the baud rate divisor registers and change the rate. */
  sys_outb(rs->line_ctl_port, LC_ADDRESS_DIVISOR);
  sys_outb(rs->div_low_port, divisor);
  sys_outb(rs->div_hi_port, divisor >> 8);

  /* Change the line controls and reselect the usual registers. */
  sys_outb(rs->line_ctl_port, line_controls);

  rs->ostate = devready(rs) | ORAW | OSWREADY;	/* reads modem_ctl_port */
  if ((tp->tty_termios.c_lflag & IXON) && rs->oxoff != _POSIX_VDISABLE)
	rs->ostate &= ~ORAW;

  unlock();

#else /* MACHINE == ATARI */

  line_controls = U_Q16;
  if (tp->tty_termios.c_cflag & PARENB) {
	line_controls |= U_PAR;
	if (!(tp->tty_termios.c_cflag & PARODD)) line_controls |= U_EVEN;
  }
  line_controls |= (divisor >= (UART_FREQ / 110)) ? U_ST2 : U_ST1;

  switch (tp->tty_termios.c_cflag & CSIZE) {	/* XXX - are U_Dn like CSn? */
	case CS5:	line_controls |= U_D5; break;
	case CS5:	line_controls |= U_D6; break;
	case CS5:	line_controls |= U_D7; break;
	case CS5:	line_controls |= U_D8; break;
  }
  lock();
  MFP->mf_ucr = line_controls;
  MFP->mf_tddr = divisor;
  unlock();
#endif /* MACHINE == ATARI */
}

/*===========================================================================*
 *				rs_init					     *
 *===========================================================================*/
PUBLIC void rs_init(tp)
tty_t *tp;			/* which TTY */
{
  int dummy;
/* Initialize RS232 for one line. */

  register rs232_t *rs;
  int line;
#if (MACHINE == IBM_PC)
  port_t this_8250;
  int irq;
  long v;
#endif

  /* Associate RS232 and TTY structures. */
  line = tp - &tty_table[NR_CONS];
  rs = tp->tty_priv = &rs_lines[line];
  rs->tty = tp;

  /* Set up input queue. */
  rs->ihead = rs->itail = rs->ibuf;

#if (MACHINE == IBM_PC)
  /* Precalculate port numbers for speed. Magic numbers in the code (once). */
  this_8250 = addr_8250[line];
  rs->xmit_port = this_8250 + 0;
  rs->recv_port = this_8250 + 0;
  rs->div_low_port = this_8250 + 0;
  rs->div_hi_port = this_8250 + 1;
  rs->int_enab_port = this_8250 + 1;
  rs->int_id_port = this_8250 + 2;
  rs->line_ctl_port = this_8250 + 3;
  rs->modem_ctl_port = this_8250 + 4;
  rs->line_status_port = this_8250 + 5;
  rs->modem_status_port = this_8250 + 6;
#endif

  /* Set up the hardware to a base state, in particular
   *	o turn off DTR (MC_DTR) to try to stop the external device.
   *	o be careful about the divisor latch.  Some BIOS's leave it enabled
   *	  here and that caused trouble (no interrupts) in version 1.5 by
   *	  hiding the interrupt enable port in the next step, and worse trouble
   *	  (continual interrupts) in an old version by hiding the receiver
   *	  port in the first interrupt.  Call rs_ioctl() early to avoid this.
   *	o disable interrupts at the chip level, to force an edge transition
   *	  on the 8259 line when interrupts are next enabled and active.
   *	  RS232 interrupts are guaranteed to be disabled now by the 8259
   *	  mask, but there used to be trouble if the mask was set without
   *	  handling a previous interrupt.
   */
  istop(rs);			/* sets modem_ctl_port */
  rs_config(rs);
#if (MACHINE == IBM_PC)
  sys_outb(rs->int_enab_port, 0);
#endif

  /* Clear any harmful leftover interrupts.  An output interrupt is harmless
   * and will occur when interrupts are enabled anyway.  Set up the output
   * queue using the status from clearing the modem status interrupt.
   */
#if (MACHINE == IBM_PC)
  sys_inb(rs->line_status_port, &dummy);
  sys_inb(rs->recv_port, &dummy);
#endif
  rs->ostate = devready(rs) | ORAW | OSWREADY;	/* reads modem_ctl_port */
  rs->ohead = rs->otail = rs->obuf;

#if (MACHINE == IBM_PC)
  /* Enable interrupts for both interrupt controller and device. */
  irq = (line & 1) == 0 ? RS232_IRQ : SECONDARY_IRQ;

  rs->irq = irq;
  rs->irq_hook_id = rs->irq;	/* call back with irq line number */
  if (sys_irqsetpolicy(irq, IRQ_REENABLE, &rs->irq_hook_id) != OK) {
  	printf("RS232: Couldn't obtain hook for irq %d\n", irq);
  } else {
  	if (sys_irqenable(&rs->irq_hook_id) != OK)  {
  		printf("RS232: Couldn't enable irq %d (hooked)\n", irq);
  	}
  }

  rs_irq_set |= (1 << irq);

  sys_outb(rs->int_enab_port, IE_LINE_STATUS_CHANGE | IE_MODEM_STATUS_CHANGE
				| IE_RECEIVER_READY | IE_TRANSMITTER_READY);
#else /* MACHINE == ATARI */
  /* Initialize the 68901 chip, then enable interrupts. */
  MFP->mf_scr = 0x00;
  MFP->mf_tcdcr |= T_Q004;
  MFP->mf_rsr = R_ENA;
  MFP->mf_tsr = T_ENA;
  MFP->mf_aer = (MFP->mf_aer | (IO_SCTS|IO_SDCD)) ^
		 (MFP->mf_gpip & (IO_SCTS|IO_SDCD));
  MFP->mf_ddr = (MFP->mf_ddr & ~ (IO_SCTS|IO_SDCD));
  MFP->mf_iera |= (IA_RRDY|IA_RERR|IA_TRDY|IA_TERR);
  MFP->mf_imra |= (IA_RRDY|IA_RERR|IA_TRDY|IA_TERR);
  MFP->mf_ierb |= (IB_SCTS|IB_SDCD);
  MFP->mf_imrb |= (IB_SCTS|IB_SDCD);
#endif /* MACHINE == ATARI */

  /* Fill in TTY function hooks. */
  tp->tty_devread = rs_read;
  tp->tty_devwrite = rs_write;
  tp->tty_echo = rs_echo;
  tp->tty_icancel = rs_icancel;
  tp->tty_ocancel = rs_ocancel;
  tp->tty_ioctl = rs_ioctl;
  tp->tty_break = rs_break;
  tp->tty_close = rs_close;

  /* Tell external device we are ready. */
  istart(rs);

}

/*===========================================================================*
 *				rs_interrupt				     *
 *===========================================================================*/
PUBLIC void rs_interrupt(m)
message *m;			/* which TTY */
{
	unsigned long irq_set;
	int i;
	rs232_t *rs;

	irq_set= m->NOTIFY_ARG;
	for (i= 0, rs = rs_lines; i<NR_RS_LINES; i++, rs++)
	{
		if (irq_set & (1 << rs->irq))
			rs232_handler(rs);
	}
}

/*===========================================================================*
 *				rs_icancel				     *
 *===========================================================================*/
PRIVATE int rs_icancel(tp, dummy)
tty_t *tp;			/* which TTY */
int dummy;
{
/* Cancel waiting input. */
  rs232_t *rs = tp->tty_priv;

  lock();
  rs->icount = 0;
  rs->itail = rs->ihead;
  istart(rs);
  unlock();

  return 0;	/* dummy */
}

/*===========================================================================*
 *				rs_ocancel				     *
 *===========================================================================*/
PRIVATE int rs_ocancel(tp, dummy)
tty_t *tp;			/* which TTY */
int dummy;
{
/* Cancel pending output. */
  rs232_t *rs = tp->tty_priv;

  lock();
  rs->ostate &= ~(ODONE | OQUEUED);
  rs->ocount = 0;
  rs->otail = rs->ohead;
  unlock();

  return 0;	/* dummy */
}

/*===========================================================================*
 *				rs_read					     *
 *===========================================================================*/
PRIVATE int rs_read(tp, try)
tty_t *tp;			/* which tty */
int try;
{
/* Process characters from the circular input buffer. */

  rs232_t *rs = tp->tty_priv;
  int icount, count, ostate;

  if (!(tp->tty_termios.c_cflag & CLOCAL)) {
  	if (try) return 1;
	/* Send a SIGHUP if hangup detected. */
	lock();
	ostate = rs->ostate;
	rs->ostate &= ~ODEVHUP;		/* save ostate, clear DEVHUP */
	unlock();
	if (ostate & ODEVHUP) {
		sigchar(tp, SIGHUP);
		tp->tty_termios.c_ospeed = B0;	/* Disable further I/O. */
		return;
	}
  }

  if (try) {
  	if (rs->icount > 0)
	  	return 1;
	return 0;
  }

  while ((count = rs->icount) > 0) {
	icount = bufend(rs->ibuf) - rs->itail;
	if (count > icount) count = icount;

	/* Perform input processing on (part of) the input buffer. */
	if ((count = in_process(tp, rs->itail, count)) == 0) break;

	lock();			/* protect interrupt sensitive variables */
	rs->icount -= count;
	if (!rs->idevready && rs->icount < RS_ILOWWATER) istart(rs);
	unlock();
	if ((rs->itail += count) == bufend(rs->ibuf)) rs->itail = rs->ibuf;
  }
}

/*===========================================================================*
 *				rs_ostart				     *
 *===========================================================================*/
PRIVATE void rs_ostart(rs)
rs232_t *rs;			/* which rs line */
{
/* Tell RS232 there is something waiting in the output buffer. */

  rs->ostate |= OQUEUED;
  if (txready(rs)) out_int(rs);
}

/*===========================================================================*
 *				rs_break				     *
 *===========================================================================*/
PRIVATE int rs_break(tp, dummy)
tty_t *tp;			/* which tty */
int dummy;
{
/* Generate a break condition by setting the BREAK bit for 0.4 sec. */
  rs232_t *rs = tp->tty_priv;
  int line_controls;

  sys_inb(rs->line_ctl_port, &line_controls);
  sys_outb(rs->line_ctl_port, line_controls | LC_BREAK);
  /* XXX */
  /* milli_delay(400); */				/* ouch */
  printf("RS232 break\n");
  sys_outb(rs->line_ctl_port, line_controls);
  return 0;	/* dummy */
}

/*===========================================================================*
 *				rs_close				     *
 *===========================================================================*/
PRIVATE int rs_close(tp, dummy)
tty_t *tp;			/* which tty */
int dummy;
{
/* The line is closed; optionally hang up. */
  rs232_t *rs = tp->tty_priv;
  int r;

  if (tp->tty_termios.c_cflag & HUPCL) {
	sys_outb(rs->modem_ctl_port, MC_OUT2 | MC_RTS);
  }
  return 0;	/* dummy */
}

/* Low level (interrupt) routines. */

#if (MACHINE == IBM_PC)
/*===========================================================================*
 *				rs232_handler				     *
 *===========================================================================*/
PRIVATE void rs232_handler(rs)
struct rs232 *rs;
{
/* Interrupt hander for RS232. */

  while (TRUE) {
  	int v;
	/* Loop to pick up ALL pending interrupts for device.
	 * This usually just wastes time unless the hardware has a buffer
	 * (and then we have to worry about being stuck in the loop too long).
	 * Unfortunately, some serial cards lock up without this.
	 */
	sys_inb(rs->int_id_port, &v);
	switch (v) {
	case IS_RECEIVER_READY:
		in_int(rs);
		continue;
	case IS_TRANSMITTER_READY:
		out_int(rs);
		continue;
	case IS_MODEM_STATUS_CHANGE:
		modem_int(rs);
		continue;
	case IS_LINE_STATUS_CHANGE:
		line_int(rs);
		continue;
	}
	return;
  }
}
#endif /* MACHINE == IBM_PC */

#if (MACHINE == ATARI)
/*===========================================================================*
 *				siaint					     *
 *===========================================================================*/
PRIVATE void siaint(type)
int    type;	       /* interrupt type */
{
/* siaint is the rs232 interrupt procedure for Atari ST's. For ST there are
 * as much as 5 interrupt lines used for rs232. The trap type byte left on the
 * stack by the assembler interrupt handler identifies the interrupt cause.
 */

  register unsigned char  code;
  register rs232_t *rs = &rs_lines[0];
  int s = lock();

  switch (type & 0x00FF)
  {
	case 0x00:	       /* receive buffer full */
		in_int(rs);
		break;
	case 0x01:	       /* receive error */
		line_int(rs);
		break;
	case 0x02:	       /* transmit buffer empty */
		out_int(rs);
		break;
	case 0x03:	       /* transmit error */
		code = MFP->mf_tsr;
		if (code & ~(T_ENA | T_UE | T_EMPTY))
		{
		    printf("sia: transmit error: status=%x\r\n", code);
		    /* MFP->mf_udr = lastchar; */ /* retry */
		}
		break;
	case 0x04:		/* modem lines change */
		modem_int(rs);
		break;
  }
  restore(s);
}
#endif /* MACHINE == ATARI */

/*===========================================================================*
 *				in_int					     *
 *===========================================================================*/
PRIVATE void in_int(rs)
register rs232_t *rs;		/* line with input interrupt */
{
/* Read the data which just arrived.
 * If it is the oxoff char, clear OSWREADY, else if OSWREADY was clear, set
 * it and restart output (any char does this, not just xon).
 * Put data in the buffer if room, otherwise discard it.
 * Set a flag for the clock interrupt handler to eventually notify TTY.
 */

  int c;

#if (MACHINE == IBM_PC)
  sys_inb(rs->recv_port, &c);
#else /* MACHINE == ATARI */
  c = MFP->mf_udr;
#endif

  if (!(rs->ostate & ORAW)) {
	if (c == rs->oxoff) {
		rs->ostate &= ~OSWREADY;
	} else
	if (!(rs->ostate & OSWREADY)) {
		rs->ostate |= OSWREADY;
		if (txready(rs)) out_int(rs);
	}
  }

  if (rs->icount == buflen(rs->ibuf)) return;	/* input buffer full, discard */

  if (++rs->icount == RS_IHIGHWATER && rs->idevready) istop(rs);
  *rs->ihead = c;
  if (++rs->ihead == bufend(rs->ibuf)) rs->ihead = rs->ibuf;
  if (rs->icount == 1) {
	rs->tty->tty_events = 1;
	force_timeout();
  }
}

/*===========================================================================*
 *				line_int				     *
 *===========================================================================*/
PRIVATE void line_int(rs)
register rs232_t *rs;		/* line with line status interrupt */
{
/* Check for and record errors. */

#if (MACHINE == IBM_PC)
  sys_inb(rs->line_status_port, &rs->lstatus);
#else /* MACHINE == ATARI */
  rs->lstatus = MFP->mf_rsr;
  MFP->mf_rsr &= R_ENA;
  rs->pad = MFP->mf_udr;	/* discard char in case of LS_OVERRUN_ERR */
#endif /* MACHINE == ATARI */
  if (rs->lstatus & LS_FRAMING_ERR) ++rs->framing_errors;
  if (rs->lstatus & LS_OVERRUN_ERR) ++rs->overrun_errors;
  if (rs->lstatus & LS_PARITY_ERR) ++rs->parity_errors;
  if (rs->lstatus & LS_BREAK_INTERRUPT) ++rs->break_interrupts;
}

/*===========================================================================*
 *				modem_int				     *
 *===========================================================================*/
PRIVATE void modem_int(rs)
register rs232_t *rs;		/* line with modem interrupt */
{
/* Get possibly new device-ready status, and clear ODEVREADY if necessary.
 * If the device just became ready, restart output.
 */

#if (MACHINE == ATARI)
  /* Set active edge interrupt so that next change causes a new interrupt */
  MFP->mf_aer = (MFP->mf_aer | (IO_SCTS|IO_SDCD)) ^
		 (MFP->mf_gpip & (IO_SCTS|IO_SDCD));
#endif

  if (devhup(rs)) {
	rs->ostate |= ODEVHUP;
	rs->tty->tty_events = 1;
	force_timeout();
  }

  if (!devready(rs))
	rs->ostate &= ~ODEVREADY;
  else if (!(rs->ostate & ODEVREADY)) {
	rs->ostate |= ODEVREADY;
	if (txready(rs)) out_int(rs);
  }
}

/*===========================================================================*
 *				out_int					    *
 *===========================================================================*/
PRIVATE void out_int(rs)
register rs232_t *rs;		/* line with output interrupt */
{
/* If there is output to do and everything is ready, do it (local device is
 * known ready).
 * Notify TTY when the buffer goes empty.
 */

  if (rs->ostate >= (ODEVREADY | OQUEUED | OSWREADY)) {
	/* Bit test allows ORAW and requires the others. */
#if (MACHINE == IBM_PC)
	sys_outb(rs->xmit_port, *rs->otail);
#else /* MACHINE == ATARI */
	MFP->mf_udr = *rs->otail;
#endif
	if (++rs->otail == bufend(rs->obuf)) rs->otail = rs->obuf;
	if (--rs->ocount == 0) {
		rs->ostate ^= (ODONE | OQUEUED);  /* ODONE on, OQUEUED off */
		rs->tty->tty_events = 1;
		force_timeout();
	} else
	if (rs->ocount == RS_OLOWWATER) {	/* running low? */
		rs->tty->tty_events = 1;
		force_timeout();
	}
  }
}
#endif /* NR_RS_LINES > 0 */

