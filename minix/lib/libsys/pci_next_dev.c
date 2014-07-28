/*
pci_next_dev.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_next_dev				     *
 *===========================================================================*/
int pci_next_dev(devindp, vidp, didp)
int *devindp;
u16_t *vidp;
u16_t *didp;
{
	int r;
	message m;

	m.m_type= BUSC_PCI_NEXT_DEV;
	m.m1_i1= *devindp;

	r= ipc_sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_next_dev: can't talk to PCI: %d", r);

	if (m.m_type == 1)
	{
		*devindp= m.m1_i1;
		*vidp= m.m1_i2;
		*didp= m.m1_i3;
#if 0
		printf("pci_next_dev: got device %d, %04x/%04x\n", 
			*devindp, *vidp, *didp);
#endif
		return 1;
	}
	if (m.m_type != 0)
		panic("pci_next_dev: got bad reply from PCI: %d", m.m_type);

	return 0;
}

