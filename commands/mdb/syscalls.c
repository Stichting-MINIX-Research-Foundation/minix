/* 
 * syscall.c for mdb 
 */
#include "mdb.h"
#ifdef SYSCALLS_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include "proto.h"

#define SYSCALL_NAME	"__sendrec"
#ifdef __i386
#define SYSCALL_OFFSET	0xF
#define SYSCALL_OLD	0x21CD
#else
#define SYSCALL_OFFSET  0xE
#define SYSCALL_OLD	0x20CD
#endif

static long intaddr;

void start_syscall(addr)
long addr;
{
long old; 

  syscalls = FALSE;

  if ( addr == 0 ) {
	intaddr = symbolvalue( SYSCALL_NAME, TRUE );
  	if ( intaddr == 0 ) 
		return;
	intaddr += SYSCALL_OFFSET;
  }
  else {
	intaddr = addr;
	Printf("Using %lx as syscall address\n",addr);
  }

  old = breakpt(intaddr,"\n");

  /* Check instruction */
  if ( (old & 0xFFFF) == SYSCALL_OLD)
	syscalls = TRUE;

}

void do_syscall(addr)
long addr;
{
  unsigned reg_ax,reg_bx;

  if ( addr != intaddr ) return;

  Printf("syscall to ");

  reg_ax = get_reg(curpid,reg_addr("AX"));

  switch (reg_ax) {
  case 0:	Printf(" PM ");
		break;
  case 1:	Printf(" VFS ");
		break;
  case 2:	Printf(" INET ");
		break;
  default:	Printf("Invalid dest = %d", reg_ax);
		exit(0);
  }


  reg_bx = get_reg(curpid,reg_addr("BX"));
  decode_message(reg_bx);

  /* Single step */	
  tstart(T_STEP, 0, 0, 1);

  /* Check return code */
  reg_ax = get_reg(curpid,reg_addr("AX"));
  if ( reg_ax != 0 )
	Printf("syscall failed AX=%d\n",reg_ax);
  else 
	decode_result();
}

#endif /* SYSCALLS_SUPPORT */
