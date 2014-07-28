
#define _SYSTEM 1

#include <minix/callnr.h>
#include <minix/com.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/ds.h>
#include <minix/endpoint.h>
#include <minix/minlib.h>
#include <minix/type.h>
#include <minix/ipc.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/bitmap.h>

#include <errno.h>
#include <assert.h>
#include <env.h>

#include "glo.h"
#include "proto.h"
#include "util.h"
#include "sanitycheck.h"

static void reset_vm_rusage(struct vmproc *vmp)
{
	vmp->vm_total = 0;
	vmp->vm_total_max = 0;
	vmp->vm_minor_page_fault = 0;
	vmp->vm_major_page_fault = 0;
}

void free_proc(struct vmproc *vmp)
{
	map_free_proc(vmp);
	pt_free(&vmp->vm_pt);
	region_init(&vmp->vm_regions_avl);
#if VMSTATS
	vmp->vm_bytecopies = 0;
#endif
	vmp->vm_region_top = 0;
	reset_vm_rusage(vmp);
}

void clear_proc(struct vmproc *vmp)
{
	region_init(&vmp->vm_regions_avl);
	acl_clear(vmp);
	vmp->vm_flags = 0;		/* Clear INUSE, so slot is free. */
#if VMSTATS
	vmp->vm_bytecopies = 0;
#endif
	vmp->vm_region_top = 0;
	reset_vm_rusage(vmp);
}

/*===========================================================================*
 *				do_exit					     *
 *===========================================================================*/
int do_exit(message *msg)
{
	int proc;
	struct vmproc *vmp;

SANITYCHECK(SCL_FUNCTIONS);

	if(vm_isokendpt(msg->VME_ENDPOINT, &proc) != OK) {
		printf("VM: bogus endpoint VM_EXIT %d\n", msg->VME_ENDPOINT);
		return EINVAL;
	}
	vmp = &vmproc[proc];

	if(!(vmp->vm_flags & VMF_EXITING)) {
		printf("VM: unannounced VM_EXIT %d\n", msg->VME_ENDPOINT);
		return EINVAL;
	}

	{
		/* Free pagetable and pages allocated by pt code. */
SANITYCHECK(SCL_DETAIL);
		free_proc(vmp);
SANITYCHECK(SCL_DETAIL);
	} 
SANITYCHECK(SCL_DETAIL);

	/* Reset process slot fields. */
	clear_proc(vmp);

SANITYCHECK(SCL_FUNCTIONS);
	return OK;
}

/*===========================================================================*
 *				do_willexit				     *
 *===========================================================================*/
int do_willexit(message *msg)
{
	int proc;
	struct vmproc *vmp;

	if(vm_isokendpt(msg->VMWE_ENDPOINT, &proc) != OK) {
		printf("VM: bogus endpoint VM_EXITING %d\n",
			msg->VMWE_ENDPOINT);
		return EINVAL;
	}
	vmp = &vmproc[proc];

	vmp->vm_flags |= VMF_EXITING;

	return OK;
}

int do_procctl(message *msg, int transid)
{
	endpoint_t proc;
	struct vmproc *vmp;

	if(vm_isokendpt(msg->VMPCTL_WHO, &proc) != OK) {
		printf("VM: bogus endpoint VM_PROCCTL %ld\n",
			msg->VMPCTL_WHO);
		return EINVAL;
	}
	vmp = &vmproc[proc];

	switch(msg->VMPCTL_PARAM) {
		case VMPPARAM_CLEAR:
			if(msg->m_source != RS_PROC_NR
				&& msg->m_source != VFS_PROC_NR)
				return EPERM;
			free_proc(vmp);
			if(pt_new(&vmp->vm_pt) != OK)
				panic("VMPPARAM_CLEAR: pt_new failed");
			pt_bind(&vmp->vm_pt, vmp);
			return OK;
		case VMPPARAM_HANDLEMEM:
		{
			if(msg->m_source != VFS_PROC_NR)
				return EPERM;

                        handle_memory_start(vmp, msg->VMPCTL_M1,
				msg->VMPCTL_LEN, msg->VMPCTL_FLAGS,
                                VFS_PROC_NR, VFS_PROC_NR, transid, 1);

			return SUSPEND;
		}
		default:
			return EINVAL;
	}

	return OK;
}

