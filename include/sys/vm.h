/*
sys/vm.h
*/

#define PAGE_SIZE	4096

/* MIOCMAP */
struct mapreq
{
	void *base;
	size_t size;
	off_t offset;
	int readonly;
};

/* i386 paging constants */
#define I386_VM_PRESENT	0x001	/* Page is present */
#define I386_VM_WRITE	0x002	/* Read/write access allowed */
#define I386_VM_USER	0x004	/* User access allowed */
#define I386_VM_PWT	0x008	/* Write through */
#define I386_VM_PCD	0x010	/* Cache disable */
#define I386_VM_ADDR_MASK 0xFFFFF000 /* physical address */

#define I386_VM_PT_ENT_SIZE	4	/* Size of a page table entry */
#define I386_VM_DIR_ENTRIES	1024	/* Number of entries in a page dir */
#define I386_VM_DIR_ENT_SHIFT	22	/* Shift to get entry in page dir. */
#define I386_VM_PT_ENT_SHIFT	12	/* Shift to get entry in page table */
#define I386_VM_PT_ENT_MASK	0x3FF	/* Mask to get entry in page table */

#define I386_CR0_PG		0x80000000	/* Enable paging */		
