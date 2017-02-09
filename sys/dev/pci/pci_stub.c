#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_stub.c,v 1.3 2015/08/24 23:55:04 pooka Exp $");

#ifdef _KERNEL_OPT
#include "opt_pci.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

int default_pci_bus_devorder(pci_chipset_tag_t, int, uint8_t *, int);
int default_pci_chipset_tag_create(pci_chipset_tag_t, uint64_t,
    const struct pci_overrides *, void *, pci_chipset_tag_t *);
void default_pci_chipset_tag_destroy(pci_chipset_tag_t);
void *default_pci_intr_establish_xname(pci_chipset_tag_t, pci_intr_handle_t,
    int, int (*)(void *), void *, const char *);

__strict_weak_alias(pci_bus_devorder, default_pci_bus_devorder);
__strict_weak_alias(pci_chipset_tag_create, default_pci_chipset_tag_create);
__strict_weak_alias(pci_chipset_tag_destroy, default_pci_chipset_tag_destroy);

__strict_weak_alias(pci_intr_establish_xname, default_pci_intr_establish_xname);

int
default_pci_bus_devorder(pci_chipset_tag_t pc, int bus, uint8_t *devs,
    int maxdevs)
{
	int i, n;

	n = MIN(pci_bus_maxdevs(pc, bus), maxdevs);
	for (i = 0; i < n; i++)
		devs[i] = i;

	return n;
}

void
default_pci_chipset_tag_destroy(pci_chipset_tag_t pc)
{
}

int
default_pci_chipset_tag_create(pci_chipset_tag_t opc, const uint64_t present,
    const struct pci_overrides *ov, void *ctx, pci_chipset_tag_t *pcp)
{
	return EOPNOTSUPP;
}

void *
default_pci_intr_establish_xname(pci_chipset_tag_t pc, pci_intr_handle_t ih,
    int level, int (*func)(void *), void *arg, const char *__nouse)
{

	return pci_intr_establish(pc, ih, level, func, arg);
}
