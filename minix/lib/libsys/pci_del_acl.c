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
int pci_del_acl(proc_ep)
endpoint_t proc_ep;
{
	int r;
	message m;
	endpoint_t endpoint;

	if (pci_procnr == ANY)
	{
		r= ds_retrieve_label_endpt("pci", &endpoint);
		if (r != 0)
		{
			panic("pci_del_acl: ds_retrieve_label_endpt failed for 'pci': %d", r);
		}
		pci_procnr = endpoint;
	}


	m.m_type= BUSC_PCI_DEL_ACL;
	m.m1_i1= proc_ep;

	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_del_acl: can't talk to PCI: %d", r);

	return m.m_type;
}

