/*
pci_set_acl.c
*/

#include "pci.h"
#include "syslib.h"
#include <unistd.h>
#include <minix/rs.h>
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_set_acl				     *
 *===========================================================================*/
PUBLIC int pci_set_acl(rs_pci)
struct rs_pci *rs_pci;
{
	int r;
	cp_grant_id_t gid;
	message m;

	if (pci_procnr == ANY)
	{
		r= _pm_findproc("pci", &pci_procnr);
		if (r != 0)
		{
			panic("pci",
				"pci_set_acl: _pm_findproc failed for 'pci'",
				r);
		}
	}


	gid= cpf_grant_direct(pci_procnr, (vir_bytes)rs_pci, sizeof(*rs_pci),
		CPF_READ);
	if (gid == -1)
	{
		printf("pci_set_acl: cpf_grant_direct failed: %d\n",
			errno);
		return EINVAL;
	}

	m.m_type= BUSC_PCI_ACL;
	m.m1_i1= gid;

	r= sendrec(pci_procnr, &m);
	cpf_revoke(gid);
	if (r != 0)
		panic("pci", "pci_set_acl: can't talk to PCI", r);

	return r;
}

