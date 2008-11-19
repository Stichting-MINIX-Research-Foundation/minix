/* A server must occasionally print some message.  It uses a simple version of 
 * printf() found in the system lib that calls kputc() to output characters.
 * Printing is done with a call to the kernel, and not by going through FS.
 *
 * This routine can only be used by servers and device drivers.  The kernel
 * must define its own kputc(). Note that the log driver also defines its own 
 * kputc() to directly call the TTY instead of going through this library.
 */

#include "sysutil.h"

static char print_buf[80*25];	/* output is buffered here */

int kputc_use_private_grants= 0;

/*===========================================================================*
 *				kputc					     *
 *===========================================================================*/
void kputc(c)
int c;
{
/* Accumulate another character.  If 0 or buffer full, print it. */
  static int buf_count;		/* # characters in the buffer */
  message m;

  if ((c == 0 && buf_count > 0) || buf_count == sizeof(print_buf)) {
#define PRINTPROCS (sizeof(procs)/sizeof(procs[0]))
	int procs[] = OUTPUT_PROCS_ARRAY;
	static int firstprint = 1;
	static cp_grant_id_t printgrants[PRINTPROCS];
	int p;

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

	for(p = 0; procs[p] != NONE; p++) {
		/* Send the buffer to this output driver. */
		int may_asyn = 0;
		m.DIAG_BUF_COUNT = buf_count;
		if(GRANT_VALID(printgrants[p])) {
			m.m_type = DIAGNOSTICS_S;
			m.DIAG_PRINT_BUF_G = (char *) printgrants[p];
			may_asyn = 1;
		} else {
			m.m_type = DIAGNOSTICS;
			m.DIAG_PRINT_BUF_G = print_buf;
		}
		if(may_asyn && procs[p] == LOG_PROC_NR) {
			m.m_type = ASYN_DIAGNOSTICS;
			(void) asynsend(procs[p], &m);
		} else {
			sendrec(procs[p], &m);
		}
	}

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
