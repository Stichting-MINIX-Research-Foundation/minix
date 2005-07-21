/* A server must occasionally print some message.  It uses a simple version of 
 * printf() found in the system lib that calls kputc() to output characters.
 * Printing is done with a call to the kernel, and not by going through FS.
 * This way system messages end up in the kernel messages buffer and can be 
 * reviewed at a later time. 
 *
 * This routine can only be used by servers and device drivers.  The kernel
 * must define its own kputc(). Note that the log driver also defines its own 
 * kputc() to directly call the TTY instead of going through this library.
 */

#include "sysutil.h"

/*===========================================================================*
 *				kputc					     *
 *===========================================================================*/
void kputc(c)
int c;
{
/* Accumulate another character.  If 0 or buffer full, print it. */

  static int buf_count;		/* # characters in the buffer */
  static char print_buf[80];	/* output is buffered here */
  message m;

  if ((c == 0 && buf_count > 0) || buf_count == sizeof(print_buf)) {
	/* Send the buffer to the system task, or, if this process is not a
	 * server yet, to standard error.
	 */
	m.DIAG_BUF_COUNT = buf_count;
	m.DIAG_PRINT_BUF = print_buf;
	m.DIAG_PROC_NR = SELF;
	m.m_type = DIAGNOSTICS;
	if (_sendrec(PRINTF_PROC, &m) != 0) {
		m.m1_i1 = 2;
		m.m1_i2 = buf_count;
		m.m1_p1 = print_buf;
		m.m_type = WRITE;
		(void) _sendrec(FS, &m);
	}
	buf_count = 0;
  }
  if (c != 0) print_buf[buf_count++] = c;
}
