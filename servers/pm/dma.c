
#include "pm.h"

#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <minix/vm.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "mproc.h"

/*===========================================================================*
 *				do_adddma				     *
 *===========================================================================*/
PUBLIC int do_adddma()
{
	endpoint_t req_proc_e, target_proc_e;
	int proc_n, r;
	phys_bytes base, size;

	if (mp->mp_effuid != SUPER_USER)
		return EPERM;

	req_proc_e= m_in.m_source;
	target_proc_e= m_in.m2_i1;
	base= m_in.m2_l1;
	size= m_in.m2_l2;

	if((r = vm_adddma(req_proc_e, target_proc_e, base, size)) != OK) {
		printf("pm:do_adddma: vm_adddma failed (%d)\n", r);
		return r;
	}

	/* Find target process */
	if (pm_isokendpt(target_proc_e, &proc_n) != OK)
	{
		printf("pm:do_adddma: endpoint %d not found\n", target_proc_e);
		return EINVAL;
	}

	return OK;
}

/*===========================================================================*
 *				do_deldma				     *
 *===========================================================================*/
PUBLIC int do_deldma()
{
	endpoint_t req_proc_e, target_proc_e;
	phys_bytes base, size;

	if (mp->mp_effuid != SUPER_USER)
		return EPERM;

	req_proc_e= m_in.m_source;
	target_proc_e= m_in.m2_i1;
	base= m_in.m2_l1;
	size= m_in.m2_l2;

	return vm_deldma(req_proc_e, target_proc_e, base, size);
}

/*===========================================================================*
 *				do_getdma				     *
 *===========================================================================*/
PUBLIC int do_getdma()
{
	endpoint_t req_proc_e, proc;
	int r;
	phys_bytes base, size;

	if (mp->mp_effuid != SUPER_USER)
		return EPERM;

	req_proc_e= m_in.m_source;

	if((r=vm_getdma(req_proc_e, &proc, &base, &size)) != OK)
		return r;

	printf("pm:do_getdma: setting reply to 0x%lx@0x%lx proc %d\n",
		size, base, proc);

	mp->mp_reply.m2_i1= proc;
	mp->mp_reply.m2_l1= base;
	mp->mp_reply.m2_l2= size;

	return OK;
}

