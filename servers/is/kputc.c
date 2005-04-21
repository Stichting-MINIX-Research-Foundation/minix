/* A server must occasionally print some message.  It uses a simple version of 
 * printf() found in the system library that calls putk() to output characters.
 * The IS server cannot use the regular putk() since we do not want to over-
 * write kernel messages with the output of the IS.  Hence, it uses a special
 * putk that directly sends to the TTY task. 
 */

#include "is.h"


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
	m.DIAG_BUF_COUNT = buf_count;
	m.DIAG_PRINT_BUF = print_buf;
	m.DIAG_PROC_NR = SELF;
	m.m_type = DIAGNOSTICS;	/* request TTY to output this buffer */
	_sendrec(TTY, &m);	/* if it fails, we cannot do better */ 
	buf_count = 0;		/* clear buffer for next batch */
  }
  if (c != 0) {
  	print_buf[buf_count++] = c;
  }
}

