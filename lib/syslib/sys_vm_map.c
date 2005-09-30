#include "syslib.h"

/*===========================================================================*
 *                                sys_vm_map				     *
 *===========================================================================*/
PUBLIC int sys_vm_map(proc_nr, do_map, base, size, offset)
int proc_nr;
int do_map;
phys_bytes base;
phys_bytes size;
phys_bytes offset;
{
    message m;
    int result;

    m.m4_l1= proc_nr;
    m.m4_l2= do_map;
    m.m4_l3= base;
    m.m4_l4= size;
    m.m4_l5= offset;

    result = _taskcall(SYSTASK, SYS_VM_MAP, &m);
    return(result);
}

