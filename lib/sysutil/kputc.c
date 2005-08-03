/* A server must occasionally print some message.  It uses a simple version of 
 * printf() found in the system lib that calls kputc() to output characters.
 * Printing is done with a call to the kernel, and not by going through FS.
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

	/* Send the buffer to the OUTPUT_PROC_NR driver. */
	m.DIAG_BUF_COUNT = buf_count;
	m.DIAG_PRINT_BUF = print_buf;
	m.DIAG_PROC_NR = SELF;
	m.m_type = DIAGNOSTICS;
	(void) _sendrec(OUTPUT_PROC_NR, &m);
	buf_count = 0;

	/* If the output fails, e.g., due to an ELOCKED, do not retry output
         * at the FS as if this were a normal user-land printf(). This may 
         * result in even worse problems. 
         */
  }
  if (c != 0) { 
        
        /* Append a single character to the output buffer. */
  	print_buf[buf_count++] = c;
  }
}
