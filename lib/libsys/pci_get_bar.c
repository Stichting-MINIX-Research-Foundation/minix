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
	m.BUSC_PGB_DEVIND= devind;
	m.BUSC_PGB_PORT= port;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_get_bar: can't talk to PCI: %d", r);

	if (m.m_type == 0)
	{
		*base= m.BUSC_PGB_BASE;
		*size= m.BUSC_PGB_SIZE;
		*ioflag= m.BUSC_PGB_IOFLAG;
	}
	return m.m_type;
}

