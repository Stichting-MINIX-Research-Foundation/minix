#ifndef _ARM_ARM32_VMPARAM_H_
#define _ARM_ARM32_VMPARAM_H_

/*
 * Virtual Memory parameters common to all arm32 platforms.
 */

#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)
#define	PAGE_MASK	(PAGE_SIZE - 1)

#endif /* _ARM_ARM32_VMPARAM_H_ */
