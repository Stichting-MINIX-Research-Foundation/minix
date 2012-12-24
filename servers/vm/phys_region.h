#ifndef PHYS_REGION_H
#define PHYS_REGION_H 1

#include <stddef.h>

#include "memtype.h"

typedef struct phys_region {
	struct phys_block	*ph;
	struct vir_region	*parent; /* vir_region or NULL if yielded */
	vir_bytes		offset;	/* offset from start of vir region */
#if SANITYCHECKS
	int			written;	/* written to pagetable */
#endif

	mem_type_t		*memtype;

	/* list of phys_regions that reference the same phys_block */
	struct phys_region	*next_ph_list;	
} phys_region_t;

#endif
