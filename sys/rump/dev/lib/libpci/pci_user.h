/*
 * Possible userfeature macro flags:
 *
 *   RUMPCOMP_USERFEATURE_PCI_DMAFREE:
 *	Support free'ing DMA memory.  If not, panic() when free() is called.
 *
 *   RUMPCOMP_USERFEATURE_PCI_IOSPACE
 *	Support for PCI I/O space.  If yes, rumpcomp_pci_iospace_init()
 *	must be provided.
 */

#include "rumpcomp_userfeatures_pci.h"

void *rumpcomp_pci_map(unsigned long, unsigned long);
int rumpcomp_pci_confread(unsigned, unsigned, unsigned, int, unsigned int *);
int rumpcomp_pci_confwrite(unsigned, unsigned, unsigned, int, unsigned int); 

int rumpcomp_pci_irq_map(unsigned, unsigned, unsigned, int, unsigned);
void *rumpcomp_pci_irq_establish(unsigned, int (*)(void *), void *);

/* XXX: needs work: support boundary-restricted allocations */
int rumpcomp_pci_dmalloc(size_t, size_t, unsigned long *, unsigned long *);
#ifdef RUMPCOMP_USERFEATURE_PCI_DMAFREE
void rumpcomp_pci_dmafree(unsigned long, size_t);
#endif

struct rumpcomp_pci_dmaseg {
	unsigned long ds_pa;
	unsigned long ds_len;
	unsigned long ds_vacookie;
};
int rumpcomp_pci_dmamem_map(struct rumpcomp_pci_dmaseg *, size_t, size_t,
			    void **);

unsigned long rumpcomp_pci_virt_to_mach(void *);

#ifdef RUMPCOMP_USERFEATURE_PCI_IOSPACE
int rumpcomp_pci_iospace_init(void);
#endif
