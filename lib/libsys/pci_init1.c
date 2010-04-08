/*
pci_init1.c
*/

#include "pci.h"
#include "syslib.h"
#include <string.h>
#include <unistd.h>
#include <minix/ds.h>
#include <minix/sysutil.h>

int pci_procnr= ANY;

/*===========================================================================*
 *				pci_init1				     *
 *===========================================================================*/
PUBLIC void pci_init1(name)
char *name;
{
	int r;
	endpoint_t endpoint;
	size_t len;
	message m;

	r= ds_retrieve_label_endpt("pci", &endpoint);
	if (r != 0)
		panic("pci_init1: ds_retrieve_label_endpt failed for 'pci': %d", r);
	pci_procnr= endpoint;

	m.m_type= BUSC_PCI_INIT;
	len= strlen(name);
	if (len+1 <= sizeof(m.m3_ca1))
		strcpy(m.m3_ca1, name);
	else
	{
		len= sizeof(m.m3_ca1)-1;
		memcpy(m.m3_ca1, name, len);
		m.m3_ca1[len]= '\0';
	}
	r= sendrec(pci_procnr, &m);
	if (r != 0)
		panic("pci_init1: can't talk to PCI: %d", r);
	if (m.m_type != 0)
		panic("pci_init1: got bad reply from PCI: %d", m.m_type);
}

