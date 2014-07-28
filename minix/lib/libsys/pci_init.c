/*
pci_init.c
*/

#include "syslib.h"
#include <minix/ds.h>
#include <minix/sysutil.h>

endpoint_t pci_procnr= ANY;

/*===========================================================================*
 *				pci_init				     *
 *===========================================================================*/
void pci_init(void)
{
	int r;
	message m;

	r= ds_retrieve_label_endpt("pci", &pci_procnr);
	if (r != 0)
		panic("pci_init: unable to obtain label for 'pci': %d", r);

	m.m_type= BUSC_PCI_INIT;
	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_init: can't talk to PCI: %d", r);
	if (m.m_type != 0)
		panic("pci_init: got bad reply from PCI: %d", m.m_type);
}

