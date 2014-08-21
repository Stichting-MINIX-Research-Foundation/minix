#include "kernel/kernel.h"

/* This is the user-visible struct that has pointers to other bits of data. */
struct minix_kerninfo minix_kerninfo __section(".usermapped");

/* Kernel information structures. */
struct kinfo kinfo __section(".usermapped");		/* kernel information for users */
struct machine machine __section(".usermapped");	/* machine information for users */
struct kmessages kmessages __section(".usermapped");	/* diagnostic messages in kernel */
struct loadinfo loadinfo __section(".usermapped");	/* status of load average */

