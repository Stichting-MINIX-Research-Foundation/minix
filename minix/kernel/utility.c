/* This file contains a collection of miscellaneous procedures:
 *   panic:    abort MINIX due to a fatal error
 *   kputc:          buffered putc used by kernel printf
 */

#include "kernel/kernel.h"
#include "arch_proto.h"

#include <minix/syslib.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>

#include <minix/sys_config.h>


#ifdef __x86_64__
/* direct_com_print_char() is available from the unpaged x86_64 bootstrap. */
extern void direct_com_print_char(char);
#endif

#define ARE_PANICING 0xDEADC0FF

/*===========================================================================*
 *			panic                                          *
 *===========================================================================*/
void panic(const char *fmt, ...)
{
  va_list arg;
  /* The system has run aground of a fatal kernel error. Terminate execution. */
  if (kinfo.minix_panicing == ARE_PANICING) {
  	reset();
  }
  kinfo.minix_panicing = ARE_PANICING;
  if (fmt != NULL) {
	printf("kernel panic: ");
  	va_start(arg, fmt);
	vprintf(fmt, arg);
	va_end(arg);
	printf("\n");
  }

  printf("kernel on CPU %d: ", (int)cpuid);
  util_stacktrace();

#if 0
  if(get_cpulocal_var(proc_ptr)) {
	  printf("current process : ");
	  proc_stacktrace(get_cpulocal_var(proc_ptr));
  }
#endif

  /* Abort MINIX. */
  minix_shutdown(0);
}

/*===========================================================================*
 *				kputc				     	     *
 *===========================================================================*/
void kputc(
  int c					/* character to append */
)
{
/* Accumulate a single character for a kernel message. Send a notification
 * to the output drivers if an END_OF_KMESS is encountered.
 */
  if (c != END_OF_KMESS) {
      int maxblpos = sizeof(kmess.kmess_buf) - 2;
#ifdef DEBUG_SERIAL
      if (kinfo.do_serial_debug) {
	if(c == '\n')
      		ser_putc('\r');
      	ser_putc(c);
      }
#endif
#ifdef __x86_64__
      /*
       * Kernel printf() output is not normally mirrored to COM1 on the
       * UEFI boot path.  Once panic() marks the kernel as panicking, mirror
       * its message and stack trace to COM1 so failures after switch_to_user()
       * remain visible even when DEBUG_SERIAL is disabled.
       *
       * If DEBUG_SERIAL already sends the panic output, do not duplicate it.
       */
      if (kinfo.minix_panicing == ARE_PANICING) {
#if defined(DEBUG_SERIAL)
	if (!kinfo.do_serial_debug)
#endif
	{
		if (c == '\n')
			direct_com_print_char('\r');
		direct_com_print_char(c);
	}
      }
#endif
      kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
      kmess.kmess_buf[kmess.blpos] = c;
      if (kmess.km_size < sizeof(kmess.km_buf))
          kmess.km_size += 1;
      kmess.km_next = (kmess.km_next + 1) % _KMESS_BUF_SIZE;
      if(kmess.blpos == maxblpos) {
      	memmove(kmess.kmess_buf,
		kmess.kmess_buf+1, sizeof(kmess.kmess_buf)-1);
      } else kmess.blpos++;
  } else if (!(kinfo.minix_panicing || kinfo.do_serial_debug)) {
	send_diag_sig();
  }
}

/*===========================================================================*
 *				_exit				     	     *
 *===========================================================================*/
void _exit(
  int e					/* error code */
)
{
  panic("_exit called from within the kernel, should not happen. (err %i)", e);
}
