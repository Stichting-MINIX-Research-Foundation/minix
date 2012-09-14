
#define	_SYSTEM 1

#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/debug.h>
#include <minix/bitmap.h>
#include <minix/hash.h>

#include <sys/mman.h>

#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <memory.h>
#include <sys/param.h>

#include "vm.h"
#include "proto.h"
#include "util.h"
#include "glo.h"
#include "region.h"
#include "sanitycheck.h"
#include "physravl.h"
#include "memlist.h"

struct	phys_block *pb_new(phys_bytes phys)
{
	struct phys_block *newpb;

	if(!SLABALLOC(newpb)) {
		printf("vm: pb_new: couldn't allocate phys block\n");
		return NULL;
	}

	assert(!(phys % VM_PAGE_SIZE));
	assert(phys != MAP_NONE);
	
USE(newpb,
	newpb->phys = phys;
	newpb->refcount = 0;
	newpb->firstregion = NULL;
	);

	return newpb;
}

struct	phys_region *pb_reference(struct phys_block *newpb, vir_bytes offset, struct vir_region *region)
{
	struct phys_region *newphysr;

	if(!SLABALLOC(newphysr)) {
	printf("vm: pb_reference: couldn't allocate phys region\n");
	return NULL;
	}

	/* New physical region. */
USE(newphysr,
	newphysr->offset = offset;
	newphysr->ph = newpb;
	newphysr->parent = region;
	newphysr->next_ph_list = newpb->firstregion;
	newpb->firstregion = newphysr;);

	newpb->refcount++;
	physr_insert(region->phys, newphysr);

	return newphysr;
}

/*===========================================================================*
 *				pb_unreferenced				     *
 *===========================================================================*/
void pb_unreferenced(struct vir_region *region, struct phys_region *pr, int rm)
{
	struct phys_block *pb;

	pb = pr->ph;
	assert(pb->refcount > 0);
	USE(pb, pb->refcount--;);
	assert(pb->refcount >= 0);

	if(pb->firstregion == pr) {
		USE(pb, pb->firstregion = pr->next_ph_list;);
	} else {
		struct phys_region *others;

		for(others = pb->firstregion; others;
			others = others->next_ph_list) {
			assert(others->ph == pb);
			if(others->next_ph_list == pr) {
				USE(others, others->next_ph_list = pr->next_ph_list;);
				break;
			}
		}

		assert(others); /* Otherwise, wasn't on the list. */
	}

	if(pb->refcount == 0) {
		assert(!pb->firstregion);
		if(region->flags & VR_ANON) {
			free_mem(ABS2CLICK(pb->phys), 1);
		} else if(region->flags & VR_DIRECT) {
			; /* No action required. */
		} else {
			panic("strange phys flags");
		}
		SLABFREE(pb);
	}

	if(rm) physr_remove(region->phys, pr->offset);
}
