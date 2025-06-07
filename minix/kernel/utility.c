/* This file contains a collection of miscellaneous procedures:
 *   panic:    abort MINIX due to a fatal error
 *   kputc:          buffered putc used by kernel printf
 */

#include "kernel/kernel.h"
#include "arch_proto.h"

// Userspace includes to remove:
// #include <minix/syslib.h> // Removed
// #include <unistd.h> // Removed
// #include <signal.h> // Removed
// #include <string.h> // Removed

// Kernel includes to add:
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>
#include <klib/include/kprintf.h> // This includes <stdarg.h> for the stub

#include <minix/sys_config.h> // Keep if it's kernel-level, otherwise remove

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
	kprintf_stub("kernel panic: "); // MODIFIED
  	va_start(arg, fmt);
	kvprintf_stub(fmt, arg); // MODIFIED
	va_end(arg);
	kprintf_stub("\n"); // MODIFIED
  }

  kprintf_stub("kernel on CPU %d: ", cpuid); // MODIFIED
  util_stacktrace();

#if 0
  if(get_cpulocal_var(proc_ptr)) {
	  kprintf_stub("current process : "); // MODIFIED (though in #if 0)
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
      kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
      kmess.kmess_buf[kmess.blpos] = c;
      if (kmess.km_size < sizeof(kmess.km_buf))
          kmess.km_size += 1;
      kmess.km_next = (kmess.km_next + 1) % _KMESS_BUF_SIZE;
      if(kmess.blpos == maxblpos) {
	kmemmove(kmess.kmess_buf, // MODIFIED
		kmess.kmess_buf+1, sizeof(kmess.kmess_buf)-1);
      } else kmess.blpos++;
  } else if (!(kinfo.minix_panicing || kinfo.do_serial_debug)) {
	/* send_diag_sig(); FIXME: Requires kernel signal mechanism */ // MODIFIED
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
