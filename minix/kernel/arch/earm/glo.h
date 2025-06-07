#ifndef __GLO_ARM_H__
#define __GLO_ARM_H__

#include "kernel/kernel.h"
#include "arch_proto.h"

// Added kernel headers
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h> // For KASSERT_PLACEHOLDER if used in headers this includes
#include <klib/include/kstring.h> // Precautionary
#include <klib/include/kmemory.h> // Precautionary


EXTERN struct tss_s tss[CONFIG_MAX_CPUS];

#endif /* __GLO_ARM_H__ */
