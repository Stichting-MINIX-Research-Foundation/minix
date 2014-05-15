/*
pci_get_bar.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_get_bar				     *
 *===========================================================================*/
int pci_get_bar(devind, port, base, size, ioflag)
int devind;
int port;
u32_t *base;
u32_t *size;
int *ioflag;
{
	int r;
	message m;

	m.m_type= BUSC_PCI_GET_BAR;
	m.m_lsys_pci_busc_get_bar.devind = devind;
	m.m_lsys_pci_busc_get_bar.port = port;

	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_get_bar: can't talk to PCI: %d", r);

	if (m.m_type == 0)
	{
		*base= m.m_pci_lsys_busc_get_bar.base;
		*size= m.m_pci_lsys_busc_get_bar.size;
		*ioflag= m.m_pci_lsys_busc_get_bar.flags;
	}
	return m.m_type;
}

