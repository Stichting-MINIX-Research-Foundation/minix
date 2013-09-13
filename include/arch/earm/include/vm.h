#ifndef __SYS_VM_ARM_H__
#define __SYS_VM_ARM_H__
/*
arm/vm.h
*/


/*
 * We are using the following setup
 * the system is configured to have the TRE (Tex remap enable bit) set to 0
 * The TEX[2:0] B and C bits are used to determins memory attributes.
 * These bits together with the S Bit (Shareability Bit) determines the 
 * memory attributes.
 *
 * The S bit is ignored when the other attribute define the memory as 
 * "device" or "strongly ordered"
 * 
 * We are setting the tex[2] bit to one to end up with the following 
 * encoding
 * 
 * 00 00 Non cacheable 
 * 01 01 Write back, Write allocate
 * 10 10 Write trough, No write allocate
 * 11 11 Write back , Write alloc
 */
#define ARM_PAGE_SIZE		4096 /* small page on ARM  */
#define ARM_SECTION_SIZE	(1024 * 1024) /* 1 MB section */

/* Second level page table entries */
#define ARM_VM_PAGETABLE	(1 << 1)  /* Page table */
#define ARM_VM_PTE_PRESENT	(1 << 1)  /* Page is present */
#define ARM_VM_PTE_B		(1 << 2)  /* B bit  */
#define ARM_VM_PTE_C		(1 << 3)  /* C bit */
#define ARM_VM_PTE_SUPER	(0x1 << 4) /* Super access only AP[1:0] */
#define ARM_VM_PTE_USER		(0x3 << 4) /* Super/User access AP[1:0] */
#define ARM_VM_PTE_TEX0		(1 << 6) /* TEX[0] */
#define ARM_VM_PTE_TEX1		(1 << 7) /* TEX[1] */
#define ARM_VM_PTE_TEX2		(1 << 8) /* TEX[2] */
#define ARM_VM_PTE_RO		(1 << 9)   /* Read only access AP[2] */
#define ARM_VM_PTE_RW		(0 << 9)   /* Read-write access AP[2] */

#define ARM_VM_PTE_S		(1 << 10) /* "Shareable" */


#define ARM_VM_PTE_NOTGLOBAL	(1 << 11) /* Not Global */

/* inner and outer write-back, write-allocate */
#define ARM_VM_PTE_WB		(ARM_VM_PTE_TEX2 | ARM_VM_PTE_TEX0 | ARM_VM_PTE_B)
/* inner and outer write-through, no write-allocate */
#define ARM_VM_PTE_WT		(ARM_VM_PTE_TEX2 | ARM_VM_PTE_TEX1 | ARM_VM_PTE_C)

#define ARM_VM_PTE_CACHED ARM_VM_PTE_WT
/* shareable device */
#define ARM_VM_PTE_DEVICE	(ARM_VM_PTE_B)

#define ARM_VM_ADDR_MASK        0xFFFFF000 /* physical address */
#define ARM_VM_ADDR_MASK_1MB    0xFFF00000 /* physical address */
#define ARM_VM_OFFSET_MASK_1MB  0x000FFFFF /* physical address */

/* Big page (1MB section) specific flags. */
#define ARM_VM_SECTION			(1 << 1)  /* 1MB section */
#define ARM_VM_SECTION_PRESENT		(1 << 1)  /* Section is present */
#define ARM_VM_SECTION_B		(1 << 2)  /* B Bit */
#define ARM_VM_SECTION_C		(1 << 3)  /* C Bit */
#define ARM_VM_SECTION_DOMAIN		(0xF << 5) /* Domain Number */
#define ARM_VM_SECTION_SUPER		(0x1 << 10) /* Super access only AP[1:0] */
#define ARM_VM_SECTION_USER		(0x3 << 10) /* Super/User access AP[1:0] */
#define ARM_VM_SECTION_TEX0		(1 << 12) /* TEX[0] */
#define ARM_VM_SECTION_TEX1		(1 << 13) /* TEX[1] */
#define ARM_VM_SECTION_TEX2		(1 << 14) /* TEX[2] */
#define ARM_VM_SECTION_RO		(1 << 15)   /* Read only access AP[2] */
#define ARM_VM_SECTION_SHAREABLE	(1 << 16)  /* Shareable */
#define ARM_VM_SECTION_NOTGLOBAL	(1 << 17)  /* Not Global */

/* inner and outer write-back, write-allocate */
#define ARM_VM_SECTION_WB	(ARM_VM_SECTION_TEX2 | ARM_VM_SECTION_TEX0 | ARM_VM_SECTION_B)
/* inner and outer write-through, no write-allocate */
#define ARM_VM_SECTION_WT	(ARM_VM_SECTION_TEX2 | ARM_VM_SECTION_TEX1 | ARM_VM_SECTION_C)
/* shareable device */

#define ARM_VM_SECTION_CACHED ARM_VM_SECTION_WT
#define ARM_VM_SECTION_DEVICE	(ARM_VM_SECTION_B)

/* Page directory specific flags. */
#define ARM_VM_PAGEDIR		(1 << 0)  /* Page directory */
#define ARM_VM_PDE_PRESENT	(1 << 0)  /* Page directory is present */
#define ARM_VM_PDE_DOMAIN	(0xF << 5) /* Domain Number */

#define ARM_VM_PT_ENT_SIZE	4	/* Size of a page table entry */
#define ARM_VM_DIR_ENT_SIZE	4	/* Size of a page dir entry */
#define ARM_VM_DIR_ENTRIES	4096	/* Number of entries in a page dir */
#define ARM_VM_DIR_ENT_SHIFT	20	/* Shift to get entry in page dir. */
#define ARM_VM_PT_ENT_SHIFT	12	/* Shift to get entry in page table */
#define ARM_VM_PT_ENT_MASK	0xFF	/* Mask to get entry in page table */
#define ARM_VM_PT_ENTRIES	256	/* Number of entries in a page table */


#define ARM_VM_PTE(v)	(((v) >> ARM_VM_PT_ENT_SHIFT) & ARM_VM_PT_ENT_MASK)
#define ARM_VM_PDE(v)	( (v) >> ARM_VM_DIR_ENT_SHIFT)
#define ARM_VM_PFA(e)	( (e) & ARM_VM_ADDR_MASK)

/* Second level small pages entry(Page Table Entry) points to 4K */
#define ARM_VM_PTE_SHIFT	12
#define ARM_VM_PTE_MASK		(~((1 << ARM_VM_PTE_SHIFT) - 1))

/* First level entry(Page Directory Entry) to a second level small page PTE */
#define ARM_VM_PDE_SHIFT	10
#define ARM_VM_PDE_MASK		(~((1 << ARM_VM_PDE_SHIFT) - 1))

/* First level entry(Page Directory Entry) to a 1MB section */
#define ARM_VM_SECTION_SHIFT	20
#define ARM_VM_SECTION_MASK	(~((1 << ARM_VM_SECTION_SHIFT) - 1))

#define ARM_VM_DIR_SIZE		(ARM_VM_DIR_ENTRIES * ARM_VM_DIR_ENT_SIZE)
#define ARM_PAGEDIR_SIZE	(ARM_VM_DIR_SIZE)
#define ARM_VM_PT_SIZE		(ARM_VM_PT_ENTRIES * ARM_VM_PT_ENT_SIZE)
#define ARM_PAGETABLE_SIZE	(ARM_VM_PT_SIZE)

/* ARM pagefault status bits */
#define ARM_VM_PFE_ALIGN          0x01 /* Pagefault caused by Alignment fault */
#define ARM_VM_PFE_IMAINT         0x04 /* Caused by Instruction cache
					  maintenance fault */
#define ARM_VM_PFE_TTWALK_L1ABORT 0x0c /* Caused by Synchronous external abort
					* on translation table walk (Level 1)
					*/
#define ARM_VM_PFE_TTWALK_L2ABORT 0x0e /* Caused by Synchronous external abort
					* on translation table walk (Level 2)
					*/
#define ARM_VM_PFE_TTWALK_L1PERR  0x1c /* Caused by Parity error
					* on translation table walk (Level 1)
					*/
#define ARM_VM_PFE_TTWALK_L2PERR  0x1e /* Caused by Parity error
					* on translation table walk (Level 2)
					*/
#define ARM_VM_PFE_L1TRANS        0x05 /* Caused by Translation fault (Level 1)
					*/
#define ARM_VM_PFE_L2TRANS        0x07 /* Caused by Translation fault (Level 2)
					*/
#define ARM_VM_PFE_L1ACCESS       0x03 /* Caused by Access flag fault (Level 1)
					*/
#define ARM_VM_PFE_L2ACCESS       0x06 /* Caused by Access flag fault (Level 2)
					*/
#define ARM_VM_PFE_L1DOMAIN       0x09 /* Caused by Domain fault (Level 1)
					*/
#define ARM_VM_PFE_L2DOMAIN       0x0b /* Caused by Domain fault (Level 2)
					*/
#define ARM_VM_PFE_L1PERM         0x0d /* Caused by Permission fault (Level 1)
					*/
#define ARM_VM_PFE_L2PERM         0x0f /* Caused by Permission fault (Level 2)
					*/
#define ARM_VM_PFE_DEBUG          0x02 /* Caused by Debug event */
#define ARM_VM_PFE_ABORT          0x08 /* Caused by Synchronous external abort
					*/
#define ARM_VM_PFE_TLB_CONFLICT   0x10 /* Caused by TLB conflict abort
					*/

#define ARM_VM_PFE_W	  (1<<11)  /* Caused by write (otherwise read) */
#define ARM_VM_PFE_FS4    (1<<10)  /* Fault status (bit 4) */
#define ARM_VM_PFE_FS3_0   0xf     /* Fault status (bits 3:0) */

/* Fault status */
#define ARM_VM_PFE_FS(s) \
    ((((s) & ARM_VM_PFE_FS4) >> 6) | ((s) & ARM_VM_PFE_FS3_0))

#ifndef __ASSEMBLY__

#include <minix/type.h>

/* structure used by VM to pass data to the kernel while enabling paging */
struct vm_ep_data {
	struct mem_map	* mem_map;
	vir_bytes	data_seg_limit;
};
#endif

#endif /* __SYS_VM_ARM_H__ */
