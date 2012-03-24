/*
 * \brief   Virtual page-table facility
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \date    2006-11-03
 */

#ifndef _DDEKIT_PGTAB_H
#define _DDEKIT_PGTAB_H

#include <ddekit/ddekit.h>

#include <ddekit/types.h>

/* FIXME Region types may be defined by pgtab users. Do we really need them
 * here? */
enum ddekit_pgtab_type
{
	PTE_TYPE_OTHER, PTE_TYPE_LARGE, PTE_TYPE_UMA, PTE_TYPE_CONTIG
};


/**
 * Set virtual->physical mapping for VM region
 *
 * \param virt      virtual start address for region
 * \param phys      physical start address for region
 * \param pages     number of pages in region
 * \param type      pgtab type for region
 */
void ddekit_pgtab_set_region(void *virt, ddekit_addr_t phys, int pages,
	int type);


/**
 * Set virtual->physical mapping for VM region given a specific size in bytes.
 *
 * Internally, DDEKit manages regions with pages. However, DDEs do not need to tangle
 * with the underlying mechanism and therefore can use this function that takes care
 * of translating a size to an amount of pages.
 */
void ddekit_pgtab_set_region_with_size(void *virt, ddekit_addr_t phys,
	int size, int type);


/**
 * Clear virtual->physical mapping for VM region
 *
 * \param virt      virtual start address for region
 * \param type      pgtab type for region
 */
void ddekit_pgtab_clear_region(void *virt, int type);

/**
 * Get physical address for virtual address
 *
 * \param virt     virtual address
 *
 * \return physical address
 */
ddekit_addr_t ddekit_pgtab_get_physaddr(const void *virt);

/**
 * Get virtual address for physical address
 *
 * \param physical  physical address
 *
 * \return virtual address
 */
ddekit_addr_t ddekit_pgtab_get_virtaddr(const ddekit_addr_t physical);

/**
 * Get type of VM region.
 *
 * \param virt      virtual address

 * \return VM region type
 */
int ddekit_pgtab_get_type(const void *virt);

/**
 * Get size of VM region.
 *
 * \param virt      virtual address
 *
 * \return VM region size (in bytes)
 */
int ddekit_pgtab_get_size(const void *virt);

#endif
