
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_exec_newmem			     *
 *===========================================================================*/
PUBLIC int vm_exec_newmem(endpoint_t ep, struct exec_newmem *args,
	int argssize, char **ret_stack_top, int *ret_flags)
{
    message m;
    int result;

    m.VMEN_ENDPOINT = ep;
    m.VMEN_ARGSPTR = (void *) args;
    m.VMEN_ARGSSIZE = argssize;

    result = _taskcall(VM_PROC_NR, VM_EXEC_NEWMEM, &m);

    *ret_stack_top = m.VMEN_STACK_TOP;
    *ret_flags = m.VMEN_FLAGS;

    return result;
}

