/*
pci_del_acl.c
*/

#include "pci.h"
#include "syslib.h"
#include <unistd.h>
#include <minix/rs.h>
#include <minix/ds.h>
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_del_acl				     *
 *===========================================================================*/
PUBLIC int pci_del_acl(proc_ep)
endpoint_t proc_ep;
{
	int r;
	message m;
	u32_t u32;

	if (pci_procnr == ANY)
	{
		r= ds_retrieve_label_num("pci", &u32);
		if (r != 0)
		{
			panic("syslib/" __FILE__,
				"pci_del_acl: _pm_findproc failed for 'pci'",
				r);
		}
		pci_procnr = u32;
	}


	m.m_type= BUSC_PCI_DEL_ACL;
	m.m1_i1= proc_ep;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("syslib/" __FILE__, "pci_del_acl: can't talk to PCI", r);

	return m.m_type;
}

