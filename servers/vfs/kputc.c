/* A server must occasionally print some message.  It uses a simple version of 
 * printf() found in the system lib that calls kputc() to output characters.
 * Printing is done with a call to the kernel, and not by going through FS.
 *
 * This routine can only be used by servers and device drivers.  The kernel
 * must define its own kputc(). Note that the log driver also defines its own 
 * kputc() to directly call the TTY instead of going through this library.
 */

#include "fs.h"
#include <string.h>
#include <minix/com.h>

#define OVERFLOW_STR "[...]\n"

#define PRINTPROCS (sizeof(procs)/sizeof(procs[0]))

static char print_buf[80];	/* output is buffered here */

int kputc_use_private_grants= 0;

static int buf_count = 0;	/* # characters in the buffer */
static int buf_offset = 0;	/* Start of current line in buffer */
static int procs[] = OUTPUT_PROCS_ARRAY;
static cp_grant_id_t printgrants[PRINTPROCS];
static int procbusy[PRINTPROCS];
static int do_flush = FALSE;
static int overflow = FALSE;

/*===========================================================================*
 *				kputc					     *
 *===========================================================================*/
void kputc(c)
int c;
{
/* Accumulate another character.  If 0 or buffer full, print it. */
  int p;
  message m;

  static int firstprint = 1;

  if (c == 0)
  {
	if (buf_count > buf_offset)
		do_flush= TRUE;
  }
  else if (buf_count >= sizeof(print_buf))
  {
	overflow= TRUE;
	if (buf_count > buf_offset)
		do_flush= TRUE;
  }
  else
  	print_buf[buf_count++] = c;

  if (!do_flush || buf_offset != 0)
	return;

	buf_offset= buf_count;
	if (kputc_use_private_grants)
	{
		for (p= 0; p<PRINTPROCS; p++)
			printgrants[p]= GRANT_INVALID;
		firstprint= 0;
	}
	if(firstprint) {
		for(p = 0; procs[p] != NONE; p++) {
			printgrants[p] = GRANT_INVALID;
		}

		firstprint = 0;

		/* First time? Initialize grant table;
		 * Grant printing processes read copy access to our
		 * print buffer forever. (So buffer can't be on stack!)
		 */
		for(p = 0; procs[p] != NONE; p++) {
			printgrants[p] = cpf_grant_direct(procs[p],
				(vir_bytes) print_buf,
				sizeof(print_buf), CPF_READ);
		}
	}

	do_flush= FALSE;
	for(p = 0; procs[p] != NONE; p++) {
		/* Send the buffer to this output driver. */
		m.DIAG_BUF_COUNT = buf_count;
		if(GRANT_VALID(printgrants[p])) {
			m.m_type = DIAGNOSTICS_S;
			m.DIAG_PRINT_BUF_G = (char *) printgrants[p];
		} else {
			m.m_type = DIAGNOSTICS;
			m.DIAG_PRINT_BUF_G = print_buf;
		}
		if (procs[p] == LOG_PROC_NR)
		{
			procbusy[p]= TRUE;
			(void) asynsend(procs[p], &m);
		}
		else
		{
			sendrec(procs[p], &m);
		}
	}
}

PUBLIC void diag_repl()
{
	endpoint_t driver_e;
	int p;

	driver_e= m_in.m_source;

	/* Find busy flag to clear */
	for(p = 0; procs[p] != NONE; p++)
	{
		if (procs[p] == driver_e)
			break;
	}
	if (procs[p] == NONE)
	{
		/* Message from wrong source */
		return;
	}
	procbusy[p]= FALSE;

	/* Wait for more replies? */
	for(p = 0; procs[p] != NONE; p++)
	{
		if (procbusy[p])
			return;
	}
	if (buf_count > buf_offset)
	{
		memmove(&print_buf[0], &print_buf[buf_offset], 
			buf_count-buf_offset);
	}
	buf_count -= buf_offset;
	buf_offset= 0;
	if (overflow)
	{
		if (buf_count + sizeof(OVERFLOW_STR) > sizeof(print_buf))
			buf_count= sizeof(print_buf)-sizeof(OVERFLOW_STR);
		overflow= FALSE;
		do_flush= FALSE;
		printf("%s", OVERFLOW_STR);
	}
	kputc(0);
}
