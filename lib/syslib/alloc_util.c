
#include "syslib.h"

#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <minix/sysutil.h>

int sys_umap_data_fb(endpoint_t ep, vir_bytes buf, vir_bytes len, phys_bytes *phys)
{
	int r;

        if((r=sys_umap(ep, VM_D, buf, len, phys)) != OK) {
		if(r != EINVAL)
			return r;
        	r = sys_umap(ep, D, buf, len, phys);
	}


	return r;
}


void *alloc_contig(size_t len, int flags, phys_bytes *phys)
{
	int r;
	vir_bytes buf;
	int mmapflags = MAP_PREALLOC|MAP_CONTIG|MAP_ANON;

	if(flags & AC_LOWER16M)
		mmapflags |= MAP_LOWER16M;
	if(flags & AC_LOWER1M)
		mmapflags |= MAP_LOWER1M;
	if(flags & AC_ALIGN64K)
		mmapflags |= MAP_ALIGN64K;

	/* First try to get memory with mmap. This is gauranteed
	 * to be page-aligned, and we can tell VM it has to be
	 * pre-allocated and contiguous.
	 */
	errno = 0;
	buf = (vir_bytes) mmap(0, len, PROT_READ|PROT_WRITE, mmapflags, -1, 0);

	/* If that failed, maybe we're not running in paged mode.
	 * If that's the case, ENXIO will be returned.
	 * Memory returned with malloc() will be preallocated and 
	 * contiguous, so fallback on that, and ask for a little extra
	 * so we can page align it ourselves.
	 */
	if(buf == (vir_bytes) MAP_FAILED) {
		u32_t align = 0;
		if(errno != (_SIGN ENXIO)) {
			return NULL;
		}
		if(flags & AC_ALIGN4K)
			align = 4*1024;
		if(flags & AC_ALIGN64K)
			align = 64*1024;
		if(len + align < len)
			return NULL;
		len += align;
		if(!(buf = (vir_bytes) malloc(len))) {
			return NULL;
		}
		if(align)
			buf += align - (buf % align);
	}

	/* Get physical address, if requested. */
        if(phys != NULL && sys_umap_data_fb(SELF, buf, len, phys) != OK)
		panic("alloc_contig.c", "sys_umap_data_fb failed", NO_NUM);

	return (void *) buf;
}

