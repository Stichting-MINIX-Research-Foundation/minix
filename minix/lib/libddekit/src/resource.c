#include "common.h"
#include <ddekit/panic.h>
#include <ddekit/resources.h>
#include <ddekit/pgtab.h>

#include <minix/vm.h>

#ifdef DDEBUG_LEVEL_RESOURCE
#undef DDEBUG
#define DDEBUG DDEBUG_LEVEL_RESOURCE
#endif

#include "debug.h"
#include "util.h"

/****************************************************************************/
/*      ddekit_release_dma                                                  */
/****************************************************************************/
int ddekit_request_dma(int nr) { 
	WARN_UNIMPL;
	/* do we stil use isa dma ? imho no.*/ 
	return -1;
}

/****************************************************************************/
/*      ddekit_request_dma                                                  */
/****************************************************************************/
int ddekit_release_dma(int nr) { 
	WARN_UNIMPL;
	/* do we stil use isa dma ? imho no.*/ 
	return -1;
}

/* 
 * In minix we don't need to explicitly request IO-ports, ...
 */
/****************************************************************************/
/*      ddekit_release/request_io                                           */
/****************************************************************************/
int ddekit_request_io (ddekit_addr_t start, ddekit_addr_t count) {
	return 0;	
}
int ddekit_release_io (ddekit_addr_t start, ddekit_addr_t count) { 
	return 0;
}

/****************************************************************************/
/*      ddekit_request_mem                                                  */
/****************************************************************************/
int ddekit_request_mem
(ddekit_addr_t start, ddekit_addr_t size, ddekit_addr_t *vaddr) {
	
	*vaddr = (ddekit_addr_t) vm_map_phys(SELF, (void *)start, size);
	
	DDEBUG_MSG_VERBOSE("start: %x, size: %d, virt: %x", start, size, *vaddr);
	
	if( *vaddr == (ddekit_addr_t) NULL) {
		ddekit_panic("unable to map IO memory from %p of size %d",
		    start, size);
	}
	return (vaddr==NULL); 
}

/****************************************************************************/
/*      ddekit_release_mem                                                  */
/****************************************************************************/
int ddekit_release_mem(ddekit_addr_t start, ddekit_addr_t size) 
{
	return	vm_unmap_phys(SELF,(void *) start, size );
}

/****************************************************************************/
/*      ddekit_inb                                                          */
/****************************************************************************/
unsigned char ddekit_inb(ddekit_addr_t port) { 
	u32_t ret;
	if (sys_inb(port, &ret)) {
		ddekit_panic("sys_inb failed.");
	}
	DDEBUG_MSG_VERBOSE("read port %x: %x", port, ret);
	return (char) ret;
}

/****************************************************************************/
/*      ddekit_inw                                                          */
/****************************************************************************/
unsigned short ddekit_inw(ddekit_addr_t port) { 
	u32_t ret;
	if (sys_inw(port, &ret)) {
		ddekit_panic("sys_inw failed.");
	}
	DDEBUG_MSG_VERBOSE("read port %x: %x", port, ret);
	return (short) ret;
}

/****************************************************************************/
/*      ddekit_inl                                                          */
/****************************************************************************/
unsigned long ddekit_inl(ddekit_addr_t port){ 
	u32_t ret;
	if (sys_inl(port, &ret)) { 
		ddekit_panic("sys_outl failed.");
	}
	DDEBUG_MSG_VERBOSE("read port %x: %x", port, ret);
	return ret;
}

/****************************************************************************/
/*      ddekit_outb                                                         */
/****************************************************************************/
void ddekit_outb(ddekit_addr_t port, unsigned char val) { 
	if (sys_outb(port,val)) {
		ddekit_panic("sys_outb failed.");
	}
	DDEBUG_MSG_VERBOSE("write port %x: %x", port, val);
}

/****************************************************************************/
/*      ddekit_outw                                                         */
/****************************************************************************/
void ddekit_outw(ddekit_addr_t port, unsigned short val) {
	if (sys_outw(port,val)) {
		ddekit_panic("sys_outw failed.");
	}
	DDEBUG_MSG_VERBOSE("write port %x: %x", port, val);
}

/****************************************************************************/
/*      ddekit_outl                                                         */
/****************************************************************************/
void ddekit_outl(ddekit_addr_t port, unsigned long val) { 
	if (sys_outl(port,val)) {
		ddekit_panic("sys_outl failed.");
	}
	DDEBUG_MSG_VERBOSE("write port %x: %x", port, val);
}


