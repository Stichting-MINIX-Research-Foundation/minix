/* This file contains a collection of miscellaneous procedures:
 *   panic:	    abort MINIX due to a fatal error
 *   kprintf:       (from lib/sysutil/kprintf.c)
 *   kputc:         buffered putc used by kernel kprintf
 */

#include "kernel.h"
#include "proc.h"

#include <unistd.h>
#include <signal.h>

/*===========================================================================*
 *				panic                                        *
 *===========================================================================*/
PUBLIC void panic(mess,nr)
_CONST char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  static int panicking = 0;
  if (panicking ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
	kprintf("\nKernel panic: %s", mess);
	if (nr != NO_NUM) kprintf(" %d", nr);
	kprintf("\n");
  }

  /* Abort MINIX. */
  prepare_shutdown(RBT_PANIC);
}


/* Include system printf() implementation named kprintf() */

#define printf kprintf
#include "../lib/sysutil/kprintf.c"
#define END_OF_KMESS 	0

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
      if (do_serial_debug) {
	if(c == '\n')
      		ser_putc('\r');
      	ser_putc(c);

      }
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

