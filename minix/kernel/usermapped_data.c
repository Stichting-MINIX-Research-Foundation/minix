#include "kernel/kernel.h"

// Added kernel headers (precautionary for consistency)
#include <minix/kernel_types.h>
#include <klib/include/kprintf.h>
#include <klib/include/kstring.h>
#include <klib/include/kmemory.h>


/* This is the user-visible struct that has pointers to other bits of data. */
struct minix_kerninfo minix_kerninfo __section(".usermapped");

/* Kernel information structures. */
struct kinfo kinfo __section(".usermapped");		/* kernel information for services */
struct machine machine __section(".usermapped");	/* machine information for services */
struct kmessages kmessages __section(".usermapped");	/* diagnostic messages in kernel */
struct loadinfo loadinfo __section(".usermapped");	/* status of load average */
struct kuserinfo kuserinfo __section(".usermapped");
	/* kernel information for users */
struct arm_frclock arm_frclock __section(".usermapped");
	/* ARM free running timer information */
struct kclockinfo kclockinfo __section(".usermapped");	/* clock information */
