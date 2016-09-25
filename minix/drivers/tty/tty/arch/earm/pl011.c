#include <minix/config.h>
#include <minix/drivers.h>
#include <minix/vm.h>
#include <minix/type.h>
#include <minix/board.h>
#include <sys/mman.h>
#include <assert.h>
#include <signal.h>
#include <termios.h>
#include "pl011_serial.h"
#include "tty.h"

#if NR_RS_LINES > 0

#define UART_FREQ       48000000L	/* timer frequency */
#if 0
#define DFLT_BAUD	TSPEED_DEF		/* default baud rate */
#else
#define DFLT_BAUD	B115200		/* default baud rate */
#endif


#define RS_IBUFSIZE          40960	/* RS232 input buffer size */
#define RS_OBUFSIZE          40960	/* RS232 output buffer size */

/* Input buffer watermarks.
 * The external device is asked to stop sending when the buffer
 * exactly reaches high water, or when TTY requests it.  Sending restarts
 * when the input buffer empties below the low watermark.
 */
#define RS_ILOWWATER   (1 * RS_IBUFSIZE / 4)
#define RS_IHIGHWATER  (3 * RS_IBUFSIZE / 4)

/* Output buffer low watermark.
 * TTY is notified when the output buffer empties below the low watermark, so
 * it may continue filling the buffer if doing a large write.
 */
#define RS_OLOWWATER   (1 * RS_OBUFSIZE / 4)

/* RS232 device structure, one per device. */
typedef struct rs232 {
  tty_t *tty;			/* associated TTY structure */

  int icount;			/* number of bytes in the input buffer */
  char *ihead;			/* next free spot in input buffer */
  char *itail;			/* first byte to give to TTY */
  char idevready;		/* nonzero if we are ready to receive (RTS) */
  char cts;			/* normally 0, but MS_CTS if CLOCAL is set */

  unsigned char ostate;		/* combination of flags: */
#define ODONE          1	/* output completed (< output enable bits) */
#define ORAW           2	/* raw mode for xoff disable (< enab. bits) */
#define OWAKEUP        4	/* tty_wakeup() pending (asm code only) */
#define ODEVREADY UART_MSR_CTS	/* external device hardware ready (CTS) */
#define OQUEUED     0x20	/* output buffer not empty */
#define OSWREADY    0x40	/* external device software ready (no xoff) */
#define ODEVHUP  UART_MSR_DCD	/* external device has dropped carrier */
#define OSOFTBITS  (ODONE | ORAW | OWAKEUP | OQUEUED | OSWREADY)
				/* user-defined bits */
#if (OSOFTBITS | ODEVREADY | ODEVHUP) == OSOFTBITS
				/* a weak sanity check */
#error				/* bits are not unique */
#endif
  unsigned char oxoff;		/* char to stop output */
  char inhibited;		/* output inhibited? (follows tty_inhibited) */
  char drain;			/* if set drain output and reconfigure line */
  int ocount;			/* number of bytes in the output buffer */
  char *ohead;			/* next free spot in output buffer */
  char *otail;			/* next char to output */

  phys_bytes phys_base;		/* UART physical base address (I/O map) */
  unsigned int reg_offset;	/* UART register offset */
  unsigned int uartclk;		/* UART clock rate */
  int rx_overrun_events;

  int irq;			/* irq for this line */
  int irq_hook_id;		/* interrupt hook */
  int irq_hook_kernel_id;	/* id as returned from sys_irqsetpolicy */

  char ibuf[RS_IBUFSIZE];	/* input buffer */
  char obuf[RS_OBUFSIZE];	/* output buffer */
} rs232_t;

static rs232_t rs_lines[NR_RS_LINES];

typedef struct uart_port {
	phys_bytes base_addr;
	int irq;
} uart_port_t;

static uart_port_t bcm2835_ports[] = {
	{ PL011_UART0_BASE, 121 },	/* UART0 */
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};

static int rs_write(tty_t *tp, int try);
static void rs_echo(tty_t *tp, int c);
static int rs_ioctl(tty_t *tp, int try);
static void rs_config(rs232_t *rs);
static int rs_read(tty_t *tp, int try);
static int rs_icancel(tty_t *tp, int try);
static int rs_ocancel(tty_t *tp, int try);
static void rs_ostart(rs232_t *rs);
static int rs_break_on(tty_t *tp, int try);
static int rs_break_off(tty_t *tp, int try);
static int rs_close(tty_t *tp, int try);
static int rs_open(tty_t *tp, int try);
static void rs232_handler(rs232_t *rs);
static void rs_reset(rs232_t *rs);

static inline unsigned int readw(vir_bytes addr);
static inline unsigned int serial_in(rs232_t *rs, int offset);
static inline void serial_out(rs232_t *rs, int offset, int val);
static inline void writew(vir_bytes addr, int val);
static void write_chars(rs232_t *rs);
static void read_chars(rs232_t *rs);

static inline unsigned int
readw(vir_bytes addr)
{
	return *((volatile unsigned int *) addr);
}

static inline void
writew(vir_bytes addr, int val)
{
	*((volatile unsigned int *) addr) = val;
}

static inline unsigned int
serial_in(rs232_t *rs, int offset)
{
	offset <<= rs->reg_offset;
	return readw(rs->phys_base + offset);
}

static inline void
serial_out(rs232_t *rs, int offset, int val)
{
	offset <<= rs->reg_offset;
	writew(rs->phys_base + offset, val);
}

static void
rs_reset(rs232_t *rs)
{
}

static int
rs_write(register tty_t *tp, int try)
{
	/* (*devwrite)() routine for RS232. */
	rs232_t *rs = tp->tty_priv;
	int r, count, ocount;

	if (rs->inhibited != tp->tty_inhibited) {
		/* Inhibition state has changed. */
		rs->ostate |= OSWREADY;
		if (tp->tty_inhibited) rs->ostate &= ~OSWREADY;
		rs->inhibited = tp->tty_inhibited;
	}

	if (rs->drain) {
		/* Wait for the line to drain then reconfigure and continue
		 * output. */
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
		if (tp->tty_outcaller == KERNEL) {
			/* We're trying to print on kernel's behalf */
			memcpy(rs->ohead,
				(char *) tp->tty_outgrant + tp->tty_outcum,
				count);
		} else {
			if ((r = sys_safecopyfrom(tp->tty_outcaller,
				tp->tty_outgrant, tp->tty_outcum,
				(vir_bytes) rs->ohead, count)) != OK) {
				return 0;
			}
		}

		/* Perform output processing on the output buffer. */
		out_process(tp, rs->obuf, rs->ohead, bufend(rs->obuf), &count,
		    &ocount);
		if (count == 0) {
			break;
		}

		/* Assume echoing messed up by output. */
		tp->tty_reprint = TRUE;

		/* Bookkeeping. */
		rs->ocount += ocount;
		rs_ostart(rs);
		if ((rs->ohead += ocount) >= bufend(rs->obuf))
			rs->ohead -= buflen(rs->obuf);
		tp->tty_outcum += count;
		if ((tp->tty_outleft -= count) == 0) {
			/* Output is finished, reply to the writer. */
			if (tp->tty_outcaller != KERNEL)
				chardriver_reply_task(tp->tty_outcaller,
					tp->tty_outid, tp->tty_outcum);
			tp->tty_outcum = 0;
			tp->tty_outcaller = NONE;
		}

	}
	if (tp->tty_outleft > 0 && tp->tty_termios.c_ospeed == B0) {
		/* Oops, the line has hung up. */
		if (tp->tty_outcaller != KERNEL)
			chardriver_reply_task(tp->tty_outcaller, tp->tty_outid,
				EIO);
		tp->tty_outleft = tp->tty_outcum = 0;
		tp->tty_outcaller = NONE;
	}

	return 1;
}

static void
rs_echo(tty_t *tp, int character)
{
	/* Echo one character.  (Like rs_write, but only one character, optionally.) */
	rs232_t *rs = tp->tty_priv;
	int count, ocount;

	ocount = buflen(rs->obuf) - rs->ocount;
	if (ocount == 0) return;		/* output buffer full */
	count = 1;
	*rs->ohead = character;			/* add one character */

	out_process(tp, rs->obuf, rs->ohead, bufend(rs->obuf), &count, &ocount);
	if (count == 0) return;

	rs->ocount += ocount;
	rs_ostart(rs);
	if ((rs->ohead += ocount) >= bufend(rs->obuf))
		rs->ohead -= buflen(rs->obuf);
}

static int
rs_ioctl(tty_t *tp, int UNUSED(dummy))
{
	/* Reconfigure the line as soon as the output has drained. */
	rs232_t *rs = tp->tty_priv;

	rs->drain = TRUE;
	return 0;	/* dummy */
}

static void rs_config(rs232_t *rs)
{
	rs->ostate = ODEVREADY | ORAW | OSWREADY;	/* reads MSR */

	/*
	 * XXX: Disable FIFO otherwise only half of every received character
	 * will trigger an interrupt.
	 */
	serial_out(rs, PL011_LCR_H, serial_in(rs, PL011_LCR_H) & ~PL011_FEN);
	/* Set interrupt levels */
	serial_out(rs, PL011_IFLS, 0x0);

	if (sys_irqenable(&rs->irq_hook_kernel_id) != OK)
		panic("unable to enable interrupts");
}

void
rs_init(tty_t *tp)
{
	/* Initialize RS232 for one line. */
	rs232_t *rs;
	int line;
	uart_port_t this_pl011;
	char l[10];
	struct minix_mem_range mr;

	/* Associate RS232 and TTY structures. */
	line = tp - &tty_table[NR_CONS];

	/* See if kernel debugging is enabled; if so, don't initialize this
	 * serial line, making tty not look at the irq and returning ENXIO
	 * for all requests on it from userland. (The kernel will use it.)
	 */
	if(env_get_param(SERVARNAME, l, sizeof(l)-1) == OK && atoi(l) == line){
		printf("TTY: rs232 line %d not initialized (used by kernel)\n",
			line);
		return;
	}

	rs = tp->tty_priv = &rs_lines[line];
	rs->tty = tp;

	/* Set up input queue. */
	rs->ihead = rs->itail = rs->ibuf;

	this_pl011 = bcm2835_ports[line];
	if (this_pl011.base_addr == 0) return;

	/* Configure memory access */
	mr.mr_base = rs->phys_base;
	mr.mr_limit = rs->phys_base + 0x1000;
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		panic("Unable to request access to UART memory");
	}
	rs->phys_base = (vir_bytes) vm_map_phys(SELF,
					(void *) this_pl011.base_addr, 0x1000);
	
	if (rs->phys_base ==  (vir_bytes) MAP_FAILED) {
		panic("Unable to request access to UART memory");
	}
	rs->reg_offset = 0;

	rs->uartclk = UART_FREQ;
	rs->ohead = rs->otail = rs->obuf;

	/* Override system default baud rate. We do this because u-boot
	 * configures the UART for a baud rate of 115200 b/s and the kernel
	 * directly sends data over serial out upon boot up. If we then
	 * suddenly change the settings, the output will be garbled during
	 * booting.
	 */
	tp->tty_termios.c_ospeed = DFLT_BAUD;

	/* Configure IRQ */
	rs->irq = this_pl011.irq;

	/* callback with irq line number */
	rs->irq_hook_kernel_id = rs->irq_hook_id = line;

	/*
	 * sys_irqsetpolicy modifies irq_hook_kernel_id. this modified id
	 * needs to be used in sys_irqenable and similar calls.
	 */
	if (sys_irqsetpolicy(rs->irq, 0, &rs->irq_hook_kernel_id) != OK) {
		printf("RS232: Couldn't obtain hook for irq %d\n", rs->irq);
	} else {
		if (sys_irqenable(&rs->irq_hook_kernel_id) != OK)  {
			printf("RS232: Couldn't enable irq %d (hooked)\n",
				rs->irq);
		}
	}

	/*
	 * When we get called back we get called back using the original
	 * hook_id bit set. e.g. if we register with hook_id 5 the callback
	 * calls us with the 5 th bit set
	 */
	rs_irq_set |= (1 << (rs->irq_hook_id ));

	/* Enable interrupts */
	rs_reset(rs);
	rs_config(rs);

	/* Fill in TTY function hooks. */
	tp->tty_devread = rs_read;
	tp->tty_devwrite = rs_write;
	tp->tty_echo = rs_echo;
	tp->tty_icancel = rs_icancel;
	tp->tty_ocancel = rs_ocancel;
	tp->tty_ioctl = rs_ioctl;
	tp->tty_break_on = rs_break_on;
	tp->tty_break_off = rs_break_off;
	tp->tty_open = rs_open;
	tp->tty_close = rs_close;

	serial_out(rs, PL011_IMSC, PL011_RXRIS);
}

void
rs_interrupt(message *m)
{
	unsigned long irq_set;
	int line;
	rs232_t *rs;

	irq_set = m->m_notify.interrupts;
	for (line = 0, rs = rs_lines; line < NR_RS_LINES; line++, rs++) {
		if ((irq_set & (1 << rs->irq_hook_id)) && (rs->phys_base != 0)) {
			rs232_handler(rs);
			if (sys_irqenable(&rs->irq_hook_kernel_id) != OK)
				panic("unable to enable interrupts");
		}
	}
}

static int
rs_icancel(tty_t *tp, int UNUSED(dummy))
{
	return 0;	/* dummy */
}

static int
rs_ocancel(tty_t *tp, int UNUSED(dummy))
{
	/* Cancel pending output. */
	rs232_t *rs = tp->tty_priv;

	rs->ostate &= ~(ODONE | OQUEUED);
	rs->ocount = 0;
	rs->otail = rs->ohead;

	return 0;	/* dummy */
}

static int
rs_read(tty_t *tp, int try)
{
	/* Process characters from the circular input buffer. */
	rs232_t *rs = tp->tty_priv;
	int icount, count, ostate;

	if (!(tp->tty_termios.c_cflag & CLOCAL)) {
		if (try) return 1;

		/* Send a SIGHUP if hangup detected. */
		ostate = rs->ostate;
		rs->ostate &= ~ODEVHUP;		/* save ostate, clear DEVHUP */
		if (ostate & ODEVHUP) {
			sigchar(tp, SIGHUP, 1);
			tp->tty_termios.c_ospeed = B0;/* Disable further I/O.*/
			return 0;
		}
	}

	if (try) {
		return(rs->icount > 0);
	}

	while ((count = rs->icount) > 0) {
		icount = bufend(rs->ibuf) - rs->itail;
		if (count > icount) count = icount;

		/* Perform input processing on (part of) the input buffer. */
		if ((count = in_process(tp, rs->itail, count)) == 0) break;
		rs->icount -= count;

		if ((rs->itail += count) == bufend(rs->ibuf))
			rs->itail = rs->ibuf;
	}

	return 0;
}

static void
rs_ostart(rs232_t *rs)
{
	/* Tell RS232 there is something waiting in the output buffer. */
	rs->ostate |= OQUEUED;
	write_chars(rs);

	serial_out(rs, PL011_IMSC, PL011_TXRIS|PL011_RXRIS);
}

static int
rs_break_on(tty_t *tp, int UNUSED(dummy))
{
	/* Raise break condition */
	return 0;	/* dummy */
}

static int
rs_break_off(tty_t *tp, int UNUSED(dummy))
{
	/* Clear break condition */
	return 0;	/* dummy */
}

static int
rs_open(tty_t *tp, int UNUSED(dummy))
{
	/* Set the speed to 115200 by default */
	tp->tty_termios.c_ospeed = DFLT_BAUD;
	return 0;
}

static int
rs_close(tty_t *tp, int UNUSED(dummy))
{
	/* The line is closed; optionally hang up. */
	return 0;	/* dummy */
}

/* Low level (interrupt) routines. */

static void
rs232_handler(struct rs232 *rs)
{
	/* Handle interrupt of a UART port */
	unsigned int ris;

	ris = serial_in(rs, PL011_RIS);

	if (ris & PL011_RXRIS) {
		/* Data ready interrupt */
		read_chars(rs);
	}
	rs->ostate |= ODEVREADY;
	if (ris & PL011_TXRIS) {
		/* Ready to send and space available */
		write_chars(rs);
	}

	serial_out(rs, PL011_ICR, ris);
}

static void
read_chars(rs232_t *rs)
{
	unsigned char c;

	/* check the line status to know if there are more chars */
	while ((serial_in(rs, PL011_FR) & PL011_RXFE) == 0) {
		c = serial_in(rs, PL011_DR);
		if (!(rs->ostate & ORAW)) {
			if (c == rs->oxoff) {
				rs->ostate &= ~OSWREADY;
			} else if (!(rs->ostate & OSWREADY)) {
				rs->ostate = OSWREADY;
			}
		}

		if (rs->icount == buflen(rs->ibuf)) {
			/* no buffer space? keep reading */
			continue;
		}

		++rs->icount;

		*rs->ihead = c;
		if (++rs->ihead == bufend(rs->ibuf)) {
			rs->ihead = rs->ibuf;
		}

		if (rs->icount == 1) {
			rs->tty->tty_events = 1;
		}
	}
}

static void
write_chars(rs232_t *rs)
{
	/*
	 * If there is output to do and everything is ready, do it (local device is
	 * known ready).
	 * Notify TTY when the buffer goes empty.
	 */
	while ((rs->ostate >= (OQUEUED | OSWREADY)) && ((serial_in(rs, PL011_FR) & PL011_TXFF) == 0)) {
		/* Bit test allows ORAW and requires the others. */
		serial_out(rs, PL011_DR, *rs->otail);
		if (++rs->otail == bufend(rs->obuf))
			rs->otail = rs->obuf;
		if (--rs->ocount == 0) {
			serial_out(rs, PL011_IMSC, PL011_RXRIS);
			/* Turn on ODONE flag, turn off OQUEUED */
			rs->ostate ^= (ODONE | OQUEUED);
			rs->tty->tty_events = 1;

		} else  {
			if (rs->icount == RS_OLOWWATER)
				rs->tty->tty_events = 1;
		}
	}
}

#endif /* NR_RS_LINES > 0 */
