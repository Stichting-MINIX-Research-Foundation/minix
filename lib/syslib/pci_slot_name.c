/*
pci_slot_name.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_slot_name				     *
 *===========================================================================*/
PUBLIC char *pci_slot_name(devind)
int devind;
{
	static char name[80];	/* We need a better interface for this */

	int r;
	message m;

	m.m_type= BUSC_PCI_SLOT_NAME;
	m.m1_i1= devind;
	m.m1_i2= sizeof(name);
	m.m1_p1= name;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci", "pci_slot_name: can't talk to PCI", r);

	if (m.m_type != 0)
		panic("pci", "pci_slot_name: got bad reply from PCI", m.m_type);

	name[sizeof(name)-1]= '\0';	/* Make sure that the string is NUL
					 * terminated.
					 */

	printf("pci_slot_name: got name %s\n", name);
	return name;
}

