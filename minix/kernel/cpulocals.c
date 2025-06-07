#include "kernel/kernel.h"

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>

struct __cpu_local_vars __cpu_local_vars CPULOCAL_ARRAY;
