/* This file contains a collection of miscellaneous procedures:
 *   minix_panic:    abort MINIX due to a fatal error
 *   kprintf:       (from lib/sysutil/kprintf.c)
 *   kputc:         buffered putc used by kernel kprintf
 */

#include "kernel.h"
#include "proc.h"

#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <minix/sysutil.h>
#include <minix/sys_config.h>

/*===========================================================================*
 *			minix_panic                                        *
 *===========================================================================*/
PUBLIC void minix_panic(mess,nr)
char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  if (minix_panicing ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
	kprintf("kernel panic: %s", mess);
	if(nr != NO_NUM)
		kprintf(" %d", nr);
	kprintf("\n");
	kprintf("kernel stacktrace: ");
	util_stacktrace();
  }

  /* Abort MINIX. */
  minix_shutdown(NULL);
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
      if (kmess.km_size < sizeof(kmess.km_buf))
          kmess.km_size += 1;		
      kmess.km_next = (kmess.km_next + 1) % _KMESS_BUF_SIZE;
  } else {
      int p, outprocs[] = OUTPUT_PROCS_ARRAY;
      if(minix_panicing) return;
      for(p = 0; outprocs[p] != NONE; p++) {
	 if(isokprocn(outprocs[p]) && !isemptyn(outprocs[p])) {
           send_sig(outprocs[p], SIGKMESS);
	 }
      }
  }
}
