/*
pci_init1.c
*/

#include "pci.h"
#include "syslib.h"
#include <string.h>
#include <unistd.h>
#include <minix/sysutil.h>

int pci_procnr= ANY;

/*===========================================================================*
 *				pci_init1				     *
 *===========================================================================*/
PUBLIC void pci_init1(name)
char *name;
{
	int r;
	size_t len;
	message m;

	r= _pm_findproc("pci", &pci_procnr);
	if (r != 0)
		panic("pci", "pci_init1: _pm_findproc failed for 'pci'", r);

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
		panic("pci", "pci_init1: can't talk to PCI", r);
	if (m.m_type != 0)
		panic("pci", "pci_init1: got bad reply from PCI", m.m_type);
}

