#ifndef PHYS_REGION_H
#define PHYS_REGION_H 1

#include <stddef.h>

typedef struct phys_region {
	struct phys_block	*ph;
	struct vir_region	*parent; /* vir_region or NULL if yielded */
	vir_bytes		offset;	/* offset from start of vir region */
#if SANITYCHECKS
	int			written;	/* written to pagetable */
#endif

	/* list of phys_regions that reference the same phys_block */
	struct phys_region	*next_ph_list;	

	/* AVL fields */
	struct phys_region	*less, *greater;
	int			factor;
} phys_region_t;

#endif
