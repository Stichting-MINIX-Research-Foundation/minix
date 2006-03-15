/*
 * printf for the kernel
 *
 * Changes:
 *   Dec 10, 2004   kernel printing to circular buffer  (Jorrit N. Herder)
 * 
 * This file contains the routines that take care of kernel messages, i.e.,
 * diagnostic output within the kernel. Kernel messages are not directly
 * displayed on the console, because this must be done by the output driver. 
 * Instead, the kernel accumulates characters in a buffer and notifies the
 * output driver when a new message is ready. 
 */

#include "kernel.h"
#include "proc.h"
#include <signal.h>

#define printf kprintf

#include "../lib/sysutil/kprintf.c"

#define END_OF_KMESS 	0
FORWARD _PROTOTYPE( void ser_putc, (char c));

/*===========================================================================*
 *				kputc				     	     *
 *===========================================================================*/
PUBLIC void kputc(c)
int c;					/* character to append */
{
/* Accumulate a single character for a kernel message. Send a notification
 * to the output driver if an END_OF_KMESS is encountered. 
 */
  if (c != END_OF_KMESS) {
      if (do_serial_debug)
      	ser_putc(c);
      kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
      if (kmess.km_size < KMESS_BUF_SIZE)
          kmess.km_size += 1;		
      kmess.km_next = (kmess.km_next + 1) % KMESS_BUF_SIZE;
  } else {
      int p, outprocs[] = OUTPUT_PROCS_ARRAY;
      for(p = 0; outprocs[p] != NONE; p++) {
	 if(isokprocn(outprocs[p]) && !isemptyn(outprocs[p])) {
           send_sig(outprocs[p], SIGKMESS);
	 }
      }
  }
}

#define COM1_BASE	0x3F8
#define COM1_THR	(COM1_BASE + 0)
#define	  LSR_THRE	0x20
#define COM1_LSR	(COM1_BASE + 5)

PRIVATE void ser_putc(char c)
{
	int i;
	int lsr, thr;

	return;

	lsr= COM1_LSR;
	thr= COM1_THR;
	for (i= 0; i<100000; i++)
	{
		if (inb(lsr) & LSR_THRE)
			break;
	}
	outb(thr, c);
}
