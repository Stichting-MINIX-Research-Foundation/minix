/*
pci_dev_name.c
*/

#include "pci.h"
#include "syslib.h"
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_dev_name				     *
 *===========================================================================*/
PUBLIC char *pci_dev_name(vid, did)
u16_t vid;
u16_t did;
{
	static char name[80];	/* We need a better interface for this */

	int r;
	message m;

	m.m_type= BUSC_PCI_DEV_NAME;
	m.m1_i1= vid;
	m.m1_i2= did;
	m.m1_i3= sizeof(name);
	m.m1_p1= name;

	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci", "pci_dev_name: can't talk to PCI", r);

	if (m.m_type == ENOENT)
	{
		printf("pci_dev_name: got no name\n");
		return NULL;	/* No name for this device */
	}
	if (m.m_type != 0)
		panic("pci", "pci_dev_name: got bad reply from PCI", m.m_type);

	name[sizeof(name)-1]= '\0';	/* Make sure that the string is NUL
					 * terminated.
					 */

#if DEBUG
	printf("pci_dev_name: got name %s\n", name);
#endif
	return name;
}

