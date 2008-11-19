
#ifndef _REGION_H
#define _REGION_H 1

struct phys_block {
#if SANITYCHECKS
	u32_t			seencount;
#endif
	vir_bytes		offset;	/* offset from start of vir region */
	vir_bytes		length;	/* no. of contiguous bytes */
	phys_bytes		phys;	/* physical memory */
	u8_t			refcount;	/* Refcount of these pages */
};

struct phys_region {
	struct phys_region	*next;	/* next contiguous block */
	struct phys_block	*ph;
};

struct vir_region {
	struct vir_region *next; /* next virtual region in this process */
	vir_bytes	vaddr;	/* virtual address, offset from pagetable */
	vir_bytes	length;	/* length in bytes */
	struct	phys_region *first; /* phys regions in vir region */
	u16_t		flags;
	u32_t tag;		/* Opaque to mapping code. */
};

/* Mapping flags: */
#define VR_WRITABLE	0x01	/* Process may write here. */
#define VR_NOPF		0x02	/* May not generate page faults. */

/* Mapping type: */
#define VR_ANON		0x10	/* Memory to be cleared and allocated */
#define VR_DIRECT	0x20	/* Mapped, but not managed by VM */

/* Tag values: */
#define VRT_NONE	0xBEEF0000
#define VRT_HEAP	0xBEEF0001

/* map_page_region flags */
#define MF_PREALLOC	0x01
#define MF_CONTIG	0x02

#endif

