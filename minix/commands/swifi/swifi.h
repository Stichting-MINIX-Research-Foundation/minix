#ifndef _LINUX_SWIFI_H
#define _LINUX_SWIFI_H

#include <stdlib.h>

#include "swifi-user.h"

long
swifi_inject_fault(char * nook_name,
		   unsigned long faultType,
		   unsigned long randSeed,
		   unsigned long numFaults,
		   void * results,
		   unsigned long do_inject);


long 
sys_inject_fault(char * module,
		 unsigned long argFaultType,
		 unsigned long argRandomSeed,
		 unsigned long argNumFaults,
		 pswifi_result_t result_record,
		 unsigned long argInjectFault);

void
swifi_kfree(const void *addr);


void
swifi_vfree(void *addr);


void *
swifi_memmove_fn(void *to, void *from, size_t len);


void *
swifi_memcpy_fn(void *to, void *from, size_t len);


void *
memmove_fn(void *to, void *from, size_t len);

void *
memcpy_fn(void *to, void *from, size_t len);

unsigned long
swifi___generic_copy_from_user (void *kaddr, void *udaddr, unsigned long len);

unsigned long	
swifi___generic_copy_to_user(void *udaddr, void *kaddr, unsigned long len);


void *
swifi_kmalloc(size_t size, int flags);


#if 0
void *
swifi___vmalloc(unsigned long size, int gfp_mask, pgprot_t prot);
#endif

#endif /* _LINUX_SWIFI_H */

