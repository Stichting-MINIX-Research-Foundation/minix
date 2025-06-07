#ifndef _BSP_INIT_H_
#define _BSP_INIT_H_

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h> // For KASSERT_PLACEHOLDER if used in headers this includes
#include <klib/include/kstring.h> // Precautionary
#include <klib/include/kmemory.h> // Precautionary

/* BSP init */
void bsp_init(void);

#endif /* __BSP_INIT_H__ */
