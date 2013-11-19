/*
pci_rescan_bus.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_rescan_bus				     *
 *===========================================================================*/
void pci_rescan_bus(u8_t busnr)
{
	int r;
	message m;

	m.m_type= BUSC_PCI_RESCAN;
	m.m1_i1= busnr;

	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_rescan_bus: can't talk to PCI: %d", r);

	if (m.m_type != 0)
	{
		panic("pci_rescan_bus: got bad reply from PCI: %d", m.m_type);
	}
}

