/*
pci_reserve.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_reserve				     *
 *===========================================================================*/
void pci_reserve(devind)
int devind;
{
	int r;
	message m;

	m.m_type= BUSC_PCI_RESERVE;
	m.m1_i1= devind;

	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_reserve: can't talk to PCI: %d", r);

	if (m.m_type != 0)
		panic("pci_reserve: got bad reply from PCI: %d", m.m_type);
}

/*===========================================================================*
 *                              pci_reserve_ok                               *
 *===========================================================================*/
int pci_reserve_ok(int devind)
{
        message m;

        m.m1_i1= devind;

        return(_taskcall(pci_procnr, BUSC_PCI_RESERVE, &m));
}

