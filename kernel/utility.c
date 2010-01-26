/* This file contains a collection of miscellaneous procedures:
 *   minix_panic:    abort MINIX due to a fatal error
 *   kprintf:       (from lib/sysutil/kprintf.c)
 *   kputc:         buffered putc used by kernel kprintf
 */

#include "kernel.h"
#include "proc.h"

#include <unistd.h>
#include <signal.h>

#include <minix/sys_config.h>

/*===========================================================================*
 *			panic                                        *
 *===========================================================================*/
PUBLIC void panic(char *what, char *mess,int nr)
{
/* This function is for when a library call wants to panic.
 * The library call calls printf() and tries to exit a process,
 * which isn't applicable in the kernel.
 */
	minix_panic(mess, nr);
}

/*===========================================================================*
 *			minix_panic                                        *
 *===========================================================================*/
PUBLIC void minix_panic(char *mess,int nr)
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
if (minix_panicing++) {
	arch_monitor();
}

  if (mess != NULL) {
	kprintf("kernel panic: %s", mess);
	if(nr != NO_NUM)
		kprintf(" %d", nr);
	kprintf("\n");
  }

  kprintf("kernel: ");
  util_stacktrace();

  /* Abort MINIX. */
  minix_shutdown(NULL);
}


/* Include system printf() implementation named kprintf() */

#define printf kprintf
#include "../lib/sysutil/kprintf.c"

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
      if(!(minix_panicing || do_serial_debug)) {
	      for(p = 0; outprocs[p] != NONE; p++) {
		 if(isokprocn(outprocs[p]) && !isemptyn(outprocs[p])) {
       	    send_sig(outprocs[p], SIGKMESS);
		 }
      	}
     }
  }
  return;
}
