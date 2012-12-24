
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
#include <sys/param.h>

#include "vm.h"
#include "proto.h"
#include "util.h"
#include "glo.h"
#include "region.h"
#include "sanitycheck.h"
#include "memlist.h"

struct	phys_block *pb_new(phys_bytes phys)
{
	struct phys_block *newpb;

	if(!SLABALLOC(newpb)) {
		printf("vm: pb_new: couldn't allocate phys block\n");
		return NULL;
	}

	if(phys != MAP_NONE)
		assert(!(phys % VM_PAGE_SIZE));
	
USE(newpb,
	newpb->phys = phys;
	newpb->refcount = 0;
	newpb->firstregion = NULL;
	);

	return newpb;
}

void pb_free(struct phys_block *pb)
{
	if(pb->phys != MAP_NONE)
		free_mem(ABS2CLICK(pb->phys), 1);
	SLABFREE(pb);
}

void pb_link(struct phys_region *newphysr, struct phys_block *newpb,
	vir_bytes offset, struct vir_region *parent)
{
USE(newphysr,
	newphysr->offset = offset;
	newphysr->ph = newpb;
	newphysr->parent = parent;
	newphysr->next_ph_list = newpb->firstregion;
	newphysr->memtype = parent->memtype;
	newpb->firstregion = newphysr;);
	newpb->refcount++;
}

struct	phys_region *pb_reference(struct phys_block *newpb,
	vir_bytes offset, struct vir_region *region)
{
	struct phys_region *newphysr;

	if(!SLABALLOC(newphysr)) {
	printf("vm: pb_reference: couldn't allocate phys region\n");
	return NULL;
	}

	/* New physical region. */
	pb_link(newphysr, newpb, offset, region);

	physblock_set(region, offset, newphysr);

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
		int r;
		if((r = region->memtype->ev_unreference(pr)) != OK)
			panic("unref failed, %d", r);

		SLABFREE(pb);
	}

	pr->ph = NULL;

	if(rm) physblock_set(region, pr->offset, NULL);
}
