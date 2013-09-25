/* Physical memory layout */

#ifndef _ARM_MEMORY_H
#define _ARM_MEMORY_H

#if defined(DM37XX) || defined(AM335X)
/* omap */
#define PHYS_MEM_BEGIN 0x80000000
#define PHYS_MEM_END 0xbfffffff
#endif  /* defined(DM37XX) || defined(AM335X) */

#endif /* _ARM_MEMORY_H */
