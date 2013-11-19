/*
pci_attr_r16.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_attr_r16				     *
 *===========================================================================*/
u16_t pci_attr_r16(devind, port)
int devind;
int port;
{
	int r;
	message m;

	m.m_type= BUSC_PCI_ATTR_R16;
	m.m2_i1= devind;
	m.m2_i2= port;

	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_attr_r16: can't talk to PCI: %d", r);

	if (m.m_type != 0)
		panic("pci_attr_r16: got bad reply from PCI: %d", m.m_type);

	return m.m2_l1;
}

