/* The MINIX model of memory allocation reserves a fixed amount of memory for
 * the combined text, data, and stack segments.  The amount used for a child
 * process created by FORK is the same as the parent had.  If the child does
 * an EXEC later, the new size is taken from the header of the file EXEC'ed.
 *
 * The layout in memory consists of the text segment, followed by the data
 * segment, followed by a gap (unused memory), followed by the stack segment.
 * The data segment grows upward and the stack grows downward, so each can
 * take memory from the gap.  If they meet, the process must be killed.  The
 * procedures in this file deal with the growth of the data and stack segments.
 *
 * The entry points into this file are:
 *   do_brk:      BRK/SBRK system calls to grow or shrink the data segment
 */

#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/keymap.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>

#include <errno.h>
#include <env.h>

#include "glo.h"
#include "vm.h"
#include "proto.h"
#include "util.h"

#define DATA_CHANGED       1    /* flag value when data segment size changed */
#define STACK_CHANGED      2    /* flag value when stack size changed */

/*===========================================================================*
 *				do_brk					     *
 *===========================================================================*/
int do_brk(message *msg)
{
/* Perform the brk(addr) system call.
 * The parameter, 'addr' is the new virtual address in D space.
 */
	int proc;

	if(vm_isokendpt(msg->VMB_ENDPOINT, &proc) != OK) {
		printf("VM: bogus endpoint VM_BRK %d\n", msg->VMB_ENDPOINT);
		return EINVAL;
	}

	return real_brk(&vmproc[proc], (vir_bytes) msg->VMB_ADDR);
}

/*===========================================================================*
 *				real_brk				     *
 *===========================================================================*/
int real_brk(vmp, v)
struct vmproc *vmp;
vir_bytes v;
{
	if(map_region_extend_upto_v(vmp, v) == OK) {
		return OK;
	}

	return(ENOMEM);
}
