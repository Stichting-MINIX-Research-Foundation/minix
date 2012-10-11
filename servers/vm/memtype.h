
#ifndef _MEMTYPE_H
#define _MEMTYPE_H 1

struct vmproc;
struct vir_region;
struct phys_region;

typedef struct mem_type {
	char *name;	/* human-readable name */
	int (*ev_new)(struct vir_region *region);
	void (*ev_delete)(struct vir_region *region);
	int (*ev_reference)(struct phys_region *pr);
	int (*ev_unreference)(struct phys_region *pr);
	int (*ev_pagefault)(struct vmproc *vmp, struct vir_region *region,
	        struct phys_region *ph, int write);
	int (*ev_resize)(struct vmproc *vmp, struct vir_region *vr, vir_bytes len);
	int (*writable)(struct phys_region *pr);
	int (*ev_sanitycheck)(struct phys_region *pr, char *file, int line);
        int (*ev_copy)(struct vir_region *vr, struct vir_region *newvr);
	u32_t (*regionid)(struct vir_region *vr);
        int (*refcount)(struct vir_region *vr);
} mem_type_t;

#endif

