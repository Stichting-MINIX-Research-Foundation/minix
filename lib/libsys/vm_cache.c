
#include "syslib.h"

#include <string.h>
#include <assert.h>

#include <sys/mman.h>
#include <minix/vm.h>
#include <minix/sysutil.h>
#include <machine/vmparam.h>

int vm_cachecall(message *m, int call, void *addr, u32_t dev, u64_t dev_offset,
	u64_t ino, u64_t ino_offset, u32_t *flags, int blocksize)
{
    if(blocksize % PAGE_SIZE)
    	panic("blocksize %d should be a multiple of pagesize %d\n",
		blocksize, PAGE_SIZE);

    if(ino_offset % PAGE_SIZE)
    	panic("inode offset %d should be a multiple of pagesize %d\n",
		ino_offset, PAGE_SIZE);

    if(dev_offset % PAGE_SIZE)
    	panic("dev offset offset %d should be a multiple of pagesize %d\n",
		dev_offset, PAGE_SIZE);

    memset(m, 0, sizeof(*m));

    assert(dev != NO_DEV);

    m->m_u.m_vmmcp.dev_offset_pages = dev_offset/PAGE_SIZE;
    m->m_u.m_vmmcp.ino_offset_pages = ino_offset/PAGE_SIZE;
    m->m_u.m_vmmcp.ino = ino;
    m->m_u.m_vmmcp.block = addr;
    m->m_u.m_vmmcp.flags_ptr = flags;
    m->m_u.m_vmmcp.dev = dev;
    m->m_u.m_vmmcp.pages = blocksize / PAGE_SIZE;
    m->m_u.m_vmmcp.flags = 0;

    return _taskcall(VM_PROC_NR, call, m);
}

void *vm_map_cacheblock(u32_t dev, u64_t dev_offset,
	u64_t ino, u64_t ino_offset, u32_t *flags, int blocksize)
{
	message m;

	if(vm_cachecall(&m, VM_MAPCACHEPAGE, NULL, dev, dev_offset,
		ino, ino_offset, flags, blocksize) != OK)
		return MAP_FAILED;

	return m.m_u.m_vmmcp_reply.addr;
}

int vm_set_cacheblock(void *block, u32_t dev, u64_t dev_offset,
	u64_t ino, u64_t ino_offset, u32_t *flags, int blocksize)
{
	message m;

	return vm_cachecall(&m, VM_SETCACHEPAGE, block, dev, dev_offset,
		ino, ino_offset, flags, blocksize);
}
