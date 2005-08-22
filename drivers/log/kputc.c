/* A server must occasionally print some message.  It uses a simple version of 
 * printf() found in the system library that calls putk() to output characters.
 * The LOG driver cannot use the regular putk().  Hence, it uses a special
 * version of putk() that directly sends to the TTY task. 
 *
 * Changes:
 *	21 July 2005:	Created  (Jorrit N. Herder)
 */

#include "log.h"

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
	m.m_type = DIAGNOSTICS;		/* request TTY to output this buffer */
	_sendrec(TTY_PROC_NR, &m);	/* if it fails, we give up */ 
	buf_count = 0;			/* clear buffer for next batch */
  }
  if (c != 0) {
  	print_buf[buf_count++] = c;
  }
}

