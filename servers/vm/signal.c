
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
#include <sys/sigcontext.h>

#include <errno.h>
#include <env.h>

#include "glo.h"
#include "vm.h"
#include "proto.h"
#include "util.h"

#define DATA_CHANGED       1    /* flag value when data segment size changed */
#define STACK_CHANGED      2    /* flag value when stack size changed */

/*===========================================================================*
 *				do_push_sig				     *
 *===========================================================================*/
PUBLIC int do_push_sig(message *msg)
{
	int r, n;
	endpoint_t ep;
	vir_bytes sp;
	struct vmproc *vmp;

	ep = msg->VMPS_ENDPOINT;

	if((r=vm_isokendpt(ep, &n)) != OK) {
		printf("VM: bogus endpoint %d from %d\n", ep, msg->m_source);
		return r;
	}
	vmp = &vmproc[n];

        if ((r=get_stack_ptr(ep, &sp)) != OK)
                vm_panic("couldn't get new stack pointer (for sig)",r);
	
	/* Save old SP for caller */
	msg->VMPS_OLD_SP = (char *) sp;
   
        /* Make room for the sigcontext and sigframe struct. */
        sp -= sizeof(struct sigcontext)
                                 + 3 * sizeof(char *) + 2 * sizeof(int);

        if ((r=adjust(vmp, vmp->vm_arch.vm_seg[D].mem_len, sp)) != OK) {
		printf("VM: do_push_sig: adjust() failed: %d\n", r);
		return r;
	}

	return OK;
}

