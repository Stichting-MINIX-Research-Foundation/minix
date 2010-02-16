/*
pci_attr_w32.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_attr_w32				     *
 *===========================================================================*/
PUBLIC void pci_attr_w32(devind, port, value)
int devind;
int port;
u32_t value;
{
	int r;
	message m;

	m.m_type= BUSC_PCI_ATTR_W32;
	m.m2_i1= devind;
	m.m2_i2= port;
	m.m2_l1= value;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("syslib/" __FILE__, "pci_attr_w32: can't talk to PCI", r);

	if (m.m_type != 0)
		panic("syslib/" __FILE__, "pci_attr_w32: got bad reply from PCI", m.m_type);
}

