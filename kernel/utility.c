/* This file contains a collection of miscellaneous procedures:
 *   panic:    abort MINIX due to a fatal error
 *   kputc:          buffered putc used by kernel printf
 */

#include "kernel.h"
#include "proc.h"

#include <minix/syslib.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>

#include <minix/sys_config.h>

#define ARE_PANICING 0xDEADC0FF

/*===========================================================================*
 *			panic                                          *
 *===========================================================================*/
PUBLIC void panic(const char *fmt, ...)
{
  va_list arg;
  /* The system has run aground of a fatal kernel error. Terminate execution. */
  if (minix_panicing == ARE_PANICING) {
	arch_monitor();
  }
  minix_panicing = ARE_PANICING;
  if (fmt != NULL) {
	printf("kernel panic: ");
  	va_start(arg, fmt);
	vprintf(fmt, arg);
	printf("\n");
  }

  printf("kernel: ");
  util_stacktrace();

  /* Abort MINIX. */
  minix_shutdown(NULL);
}

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
      int p;
      endpoint_t outprocs[] = OUTPUT_PROCS_ARRAY;
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

PUBLIC void cpu_print_freq(unsigned cpu)
{
	u64_t freq;

	freq = cpu_get_freq(cpu);
	printf("CPU %d freq %lu MHz\n", cpu, div64u(freq, 1000000));
}
