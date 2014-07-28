#include "kernel/kernel.h"

/* This is the user-visible struct that has pointers to other bits of data. */
struct minix_kerninfo minix_kerninfo;

/* Kernel information structures. */
struct kinfo kinfo;               /* kernel information for users */
struct machine machine;           /* machine information for users */
struct kmessages kmessages;       /* diagnostic messages in kernel */
struct loadinfo loadinfo;        /* status of load average */

