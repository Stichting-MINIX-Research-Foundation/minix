#ifndef MB_UTILS_H
#define MB_UTILS_H

#include "kernel/kernel.h"

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


void direct_cls(void);
void direct_print(const char*);
void direct_print_char(char);
int direct_read_char(unsigned char*);

#endif
