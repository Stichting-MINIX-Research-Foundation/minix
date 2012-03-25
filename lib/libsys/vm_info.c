
#include "syslib.h"

#include <minix/vm.h>

/*===========================================================================*
 *                                vm_info_stats				     *
 *===========================================================================*/
int vm_info_stats(struct vm_stats_info *vsi)
{
    message m;

    m.VMI_WHAT = VMIW_STATS;
    m.VMI_PTR = (void *) vsi;

    return _taskcall(VM_PROC_NR, VM_INFO, &m);
}

/*===========================================================================*
 *                                vm_info_usage				     *
 *===========================================================================*/
int vm_info_usage(endpoint_t who, struct vm_usage_info *vui)
{
    message m;

    m.VMI_WHAT = VMIW_USAGE;
    m.VMI_EP = who;
    m.VMI_PTR = (void *) vui;

    return _taskcall(VM_PROC_NR, VM_INFO, &m);
}

/*===========================================================================*
 *                                vm_info_region			     *
 *===========================================================================*/
int vm_info_region(endpoint_t who, struct vm_region_info *vri,
	int count, vir_bytes *next)
{
    message m;
    int result;

    m.VMI_WHAT = VMIW_REGION;
    m.VMI_EP = who;
    m.VMI_COUNT = count;
    m.VMI_PTR = (void *) vri;
    m.VMI_NEXT = *next;

    if ((result = _taskcall(VM_PROC_NR, VM_INFO, &m)) != OK)
        return result;

    *next = m.VMI_NEXT;
    return m.VMI_COUNT;
}

