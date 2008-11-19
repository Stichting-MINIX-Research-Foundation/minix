/*
sys/vm_i386.h
*/

#define I386_PAGE_SIZE	4096

/* i386 paging constants */
#define I386_VM_PRESENT	0x001	/* Page is present */
#define I386_VM_WRITE	0x002	/* Read/write access allowed */
#define I386_VM_USER	0x004	/* User access allowed */
#define I386_VM_PWT	0x008	/* Write through */
#define I386_VM_PCD	0x010	/* Cache disable */
#define I386_VM_ACC	0x020	/* Accessed */
#define I386_VM_ADDR_MASK 0xFFFFF000 /* physical address */

/* Page directory specific flags. */
#define I386_VM_BIGPAGE	0x080	/* 4MB page */

/* Page table specific flags. */
#define I386_VM_DIRTY	0x040	/* Dirty */
#define I386_VM_PTAVAIL1 0x080	/* Available for use. */
#define I386_VM_PTAVAIL2 0x100	/* Available for use. */

#define I386_VM_PT_ENT_SIZE	4	/* Size of a page table entry */
#define I386_VM_DIR_ENTRIES	1024	/* Number of entries in a page dir */
#define I386_VM_DIR_ENT_SHIFT	22	/* Shift to get entry in page dir. */
#define I386_VM_PT_ENT_SHIFT	12	/* Shift to get entry in page table */
#define I386_VM_PT_ENT_MASK	0x3FF	/* Mask to get entry in page table */
#define I386_VM_PT_ENTRIES	1024	/* Number of entries in a page table */
#define I386_VM_PFA_SHIFT	22	/* Page frame address shift */

#define I386_CR0_PG		0x80000000	/* Enable paging */		

/* i386 paging 'functions' */
#define I386_VM_PTE(v)	(((v) >> I386_VM_PT_ENT_SHIFT) & I386_VM_PT_ENT_MASK)
#define I386_VM_PDE(v)	( (v) >> I386_VM_DIR_ENT_SHIFT)
#define I386_VM_PFA(e)	( (e) & I386_VM_ADDR_MASK)
#define I386_VM_PAGE(v)	( (v) >> I386_VM_PFA_SHIFT)

/* i386 pagefault error code bits */
#define I386_VM_PFE_P	0x01	/* Pagefault caused by non-present page.
				 * (otherwise protection violation.)
				 */
#define I386_VM_PFE_W	0x02	/* Caused by write (otherwise read) */
#define I386_VM_PFE_U	0x04	/* CPU in user mode (otherwise supervisor) */
