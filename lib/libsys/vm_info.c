
#include "syslib.h"

#include <string.h>
#include <minix/vm.h>

/*===========================================================================*
 *                                vm_info_stats				     *
 *===========================================================================*/
int vm_info_stats(struct vm_stats_info *vsi)
{
    message m;

    memset(&m, 0, sizeof(m));
    m.m_lsys_vm_info.what = VMIW_STATS;
    m.m_lsys_vm_info.ptr = vsi;

    return _taskcall(VM_PROC_NR, VM_INFO, &m);
}

/*===========================================================================*
 *                                vm_info_usage				     *
 *===========================================================================*/
int vm_info_usage(endpoint_t who, struct vm_usage_info *vui)
{
    message m;

    memset(&m, 0, sizeof(m));
    m.m_lsys_vm_info.what = VMIW_USAGE;
    m.m_lsys_vm_info.ep = who;
    m.m_lsys_vm_info.ptr = vui;

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

    memset(&m, 0, sizeof(m));
    m.m_lsys_vm_info.what = VMIW_REGION;
    m.m_lsys_vm_info.ep = who;
    m.m_lsys_vm_info.count = count;
    m.m_lsys_vm_info.ptr = vri;
    m.m_lsys_vm_info.next = *next;

    if ((result = _taskcall(VM_PROC_NR, VM_INFO, &m)) != OK)
        return result;

    *next = m.m_lsys_vm_info.next;
    return m.m_lsys_vm_info.count;
}

