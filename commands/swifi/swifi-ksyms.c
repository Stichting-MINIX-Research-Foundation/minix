/*
 * swifi-ksyms.c -- exported symbols for wrappers od system functions
 *
 * Copyright (C) 2003 Mike Swift
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  
 * No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include "swifi.h"


EXPORT_SYMBOL(sys_inject_fault);
 
EXPORT_SYMBOL(swifi_memmove_fn);
EXPORT_SYMBOL(swifi_memcpy_fn);
EXPORT_SYMBOL(memmove_fn);
EXPORT_SYMBOL(memcpy_fn);
EXPORT_SYMBOL(swifi_kfree);
EXPORT_SYMBOL(swifi_vfree);
EXPORT_SYMBOL(swifi_kmalloc);
EXPORT_SYMBOL(swifi___vmalloc);
EXPORT_SYMBOL(swifi___generic_copy_from_user);
EXPORT_SYMBOL(swifi___generic_copy_to_user);


