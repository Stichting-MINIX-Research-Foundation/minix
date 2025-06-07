#ifndef _BSP_PADCONF_H_
#define _BSP_PADCONF_H_

#ifndef __ASSEMBLY__

// Added kernel headers
#include <minix/kernel_types.h> // For k_size_t and to ensure u32_t is appropriately defined if it's a kernel type
#include <klib/include/kprintf.h> // For KASSERT_PLACEHOLDER if used in headers this includes
#include <klib/include/kstring.h> // Precautionary
#include <klib/include/kmemory.h> // Precautionary

void bsp_padconf_init(void);
int bsp_padconf_set(u32_t padconf, u32_t mask, u32_t value); // u32_t should be from a kernel-safe header

#endif /* __ASSEMBLY__ */

#endif /* _BSP_PADCONF_H_ */
