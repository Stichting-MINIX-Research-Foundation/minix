/*
pci_init.c
*/

#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_init				     *
 *===========================================================================*/
PUBLIC void pci_init()
{
	int r;
	message m;

	m.m_type= BUSC_PCI_INIT;
	r= sendrec(PCI_PROC_NR, &m);
	if (r != 0)
		panic("pci", "pci_init: can't talk to PCI", r);
	if (m.m_type != 0)
		panic("pci", "pci_init: got bad reply from PCI", m.m_type);
}

