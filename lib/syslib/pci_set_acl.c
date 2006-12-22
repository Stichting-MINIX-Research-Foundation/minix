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


printf("pci_set_acl: before cpf_grant_direct\n");
	gid= cpf_grant_direct(pci_procnr, (vir_bytes)rs_pci, sizeof(*rs_pci),
		CPF_READ);
printf("pci_set_acl: after cpf_grant_direct: gid %d\n", gid);
	if (gid == -1)
	{
		printf("pci_set_acl: cpf_grant_direct failed: %d\n",
			errno);
		return EINVAL;
	}

	m.m_type= BUSC_PCI_ACL;
	m.m1_i1= gid;

printf("pci_set_acl: before sendrec to %d\n", pci_procnr);
	r= sendrec(pci_procnr, &m);
printf("pci_set_acl: after sendrec to %d\n", pci_procnr);
	cpf_revoke(gid);
printf("pci_set_acl: after cpf_revoke\n");
	if (r != 0)
		panic("pci", "pci_set_acl: can't talk to PCI", r);

	return r;
}

