
#include "syslib.h"

#include <string.h>
#include <assert.h>

#include <sys/mman.h>

#include <minix/vm.h>
#include <minix/sysutil.h>

#include <machine/param.h>
#include <machine/vmparam.h>

int vm_cachecall(message *m, int call, void *addr, dev_t dev, off_t dev_offset,
	ino_t ino, off_t ino_offset, u32_t *flags, int blocksize)
{
    if(blocksize % PAGE_SIZE)
    	panic("blocksize %d should be a multiple of pagesize %d\n",
		blocksize, PAGE_SIZE);

    if(ino_offset % PAGE_SIZE)
    	panic("inode offset %lld should be a multiple of pagesize %d\n",
		ino_offset, PAGE_SIZE);

    if(dev_offset % PAGE_SIZE)
    	panic("dev offset offset %lld should be a multiple of pagesize %d\n",
		dev_offset, PAGE_SIZE);

    memset(m, 0, sizeof(*m));

    assert(dev != NO_DEV);

    m->m_vmmcp.dev_offset = dev_offset;
    m->m_vmmcp.ino_offset = ino_offset;
    m->m_vmmcp.ino = ino;
    m->m_vmmcp.block = addr;
    m->m_vmmcp.flags_ptr = flags;
    m->m_vmmcp.dev = dev;
    m->m_vmmcp.pages = blocksize / PAGE_SIZE;
    m->m_vmmcp.flags = 0;

    return _taskcall(VM_PROC_NR, call, m);
}

void *vm_map_cacheblock(dev_t dev, off_t dev_offset,
	ino_t ino, off_t ino_offset, u32_t *flags, int blocksize)
{
	message m;

	if(vm_cachecall(&m, VM_MAPCACHEPAGE, NULL, dev, dev_offset,
		ino, ino_offset, flags, blocksize) != OK)
		return MAP_FAILED;

	return m.m_vmmcp_reply.addr;
}

int vm_set_cacheblock(void *block, dev_t dev, off_t dev_offset,
	ino_t ino, off_t ino_offset, u32_t *flags, int blocksize)
{
	message m;

	return vm_cachecall(&m, VM_SETCACHEPAGE, block, dev, dev_offset,
		ino, ino_offset, flags, blocksize);
}

int
vm_clear_cache(dev_t dev)
{
	message m;

	assert(dev != NO_DEV);

	memset(&m, 0, sizeof(m));

	m.m_vmmcp.dev = dev;

	return _taskcall(VM_PROC_NR, VM_CLEARCACHE, &m);
}
