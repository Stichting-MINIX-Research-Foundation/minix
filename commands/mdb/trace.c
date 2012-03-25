/* 
 * trace.c for mdb 
 */

#include "mdb.h"
#include <stdio.h>
#include <sys/ptrace.h>
#include "proto.h"

/* mdbtrace()
 * Call ptrace and check for error if debugging running process
 * Otherwise read 'core' file
 */ 
long mdbtrace(req, pid, addr, data)
int req, pid;
long addr, data;
{
  long val;
  int i;
  int segment;

#ifdef  DEBUG
  if (debug) Printf("ptrace: req=%d pid=%d addr=%lx data=%lx\n",
		req, pid, addr, data);
#endif

  if (corepid < 0) 
  {
	errno = 0;
	/* Call normal ptrace and check for error */
	val = ptrace(req, pid, addr, data);
	if (errno != 0) {
		do_error("mdb ptrace error ");
		mdb_error("\n");
	}
#ifdef  DEBUG
	if (debug) Printf("ptrace: val=>%lx\n", val);
#endif
	return val;
  } 
  else
	return read_core(req, addr, data);
}

/* Used by disassembler */
u32_t peek_dword(addr)
off_t addr;
{
    return mdbtrace(T_GETINS, curpid, addr, 0L);
}

