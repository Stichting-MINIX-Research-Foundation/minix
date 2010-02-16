/*
pci_rescan_bus.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_rescan_bus				     *
 *===========================================================================*/
PUBLIC void pci_rescan_bus(busnr)
u8_t busnr;
{
	int r;
	message m;

	m.m_type= BUSC_PCI_RESCAN;
	m.m1_i1= busnr;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("syslib/" __FILE__, "pci_rescan_bus: can't talk to PCI", r);

	if (m.m_type != 0)
	{
		panic("syslib/" __FILE__, "pci_rescan_bus: got bad reply from PCI",
			m.m_type);
	}
}

