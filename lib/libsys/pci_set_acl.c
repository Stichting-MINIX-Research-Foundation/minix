/*
pci_set_acl.c
*/

#include "pci.h"
#include "syslib.h"
#include <unistd.h>
#include <minix/rs.h>
#include <minix/ds.h>
#include <minix/sysutil.h>

/*===========================================================================*
 *				pci_set_acl				     *
 *===========================================================================*/
int pci_set_acl(rs_pci)
struct rs_pci *rs_pci;
{
	int r;
	cp_grant_id_t gid;
	message m;
	endpoint_t endpoint;

	if (pci_procnr == ANY)
	{
		r= ds_retrieve_label_endpt("pci", &endpoint);
		if (r != 0)
		{
			panic("pci_set_acl: ds_retrieve_label_endpt failed for 'pci': %d", r);
		}
		pci_procnr = endpoint;
	}


	gid= cpf_grant_direct(pci_procnr, (vir_bytes)rs_pci, sizeof(*rs_pci),
		CPF_READ);
	if (gid == -1)
	{
		printf("pci_set_acl: cpf_grant_direct failed: %d\n",
			errno);
		return EINVAL;
	}

	m.m_type= BUSC_PCI_SET_ACL;
	m.m1_i1= gid;

	r= ipc_sendrec(pci_procnr, &m);
	cpf_revoke(gid);
	if (r != 0)
		panic("pci_set_acl: can't talk to PCI: %d", r);

	return m.m_type;
}

