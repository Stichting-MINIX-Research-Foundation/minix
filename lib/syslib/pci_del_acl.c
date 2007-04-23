/*
pci_del_acl.c
*/

#include "pci.h"
#include "syslib.h"
#include <unistd.h>
#include <minix/rs.h>
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_del_acl				     *
 *===========================================================================*/
PUBLIC int pci_del_acl(proc_nr)
endpoint_t proc_nr;
{
	int r;
	message m;

	if (pci_procnr == ANY)
	{
		r= _pm_findproc("pci", &pci_procnr);
		if (r != 0)
		{
			panic("pci",
				"pci_del_acl: _pm_findproc failed for 'pci'",
				r);
		}
	}


	m.m_type= BUSC_PCI_DEL_ACL;
	m.m1_i1= proc_nr;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci", "pci_del_acl: can't talk to PCI", r);

	return m.m_type;
}

