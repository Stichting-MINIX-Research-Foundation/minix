
/* i386-specific clock functions. */

#include <ibm/ports.h>
#include <minix/portio.h>

#include "../../kernel.h"

#define CLOCK_ACK_BIT   0x80    /* PS/2 clock interrupt acknowledge bit */

/* Clock parameters. */
#define COUNTER_FREQ (2*TIMER_FREQ) /* counter frequency using square wave */
#define LATCH_COUNT     0x00    /* cc00xxxx, c = channel, x = any */
#define SQUARE_WAVE     0x36    /* ccaammmb, a = access, m = mode, b = BCD */
                                /*   11x11, 11 = LSB then MSB, x11 = sq wave */
#define TIMER_FREQ  1193182    /* clock frequency for timer in PC and AT */
#define TIMER_COUNT (TIMER_FREQ/HZ) /* initial value for counter*/

/*===========================================================================*
 *				arch_init_clock				     *
 *===========================================================================*/
PUBLIC int arch_init_clock(void)
{
	/* Initialize channel 0 of the 8253A timer to, e.g., 60 Hz,
	 * and register the CLOCK task's interrupt handler to be run
	 * on every clock tick.
	 */
	outb(TIMER_MODE, SQUARE_WAVE);  /* run continuously */
	outb(TIMER0, (TIMER_COUNT & 0xff)); /* timer low byte */
	outb(TIMER0, TIMER_COUNT >> 8); /* timer high byte */ 

	return OK;
}

/*===========================================================================*
 *				clock_stop				     *
 *===========================================================================*/
PUBLIC void clock_stop(void)
{
	/* Reset the clock to the BIOS rate. (For rebooting.) */
	outb(TIMER_MODE, 0x36);
	outb(TIMER0, 0);
	outb(TIMER0, 0);
}

/*===========================================================================*
 *				read_clock				     *
 *===========================================================================*/
PUBLIC clock_t read_clock(void)
{
	/* Read the counter of channel 0 of the 8253A timer.  This counter
	 * counts down at a rate of TIMER_FREQ and restarts at
	 * TIMER_COUNT-1 when it reaches zero. A hardware interrupt
	 * (clock tick) occurs when the counter gets to zero and restarts
	 * its cycle.
	 */     
	u32_t count;

	outb(TIMER_MODE, LATCH_COUNT);
	count = inb(TIMER0);
	count |= (inb(TIMER0) << 8);
 
	return count;
}

