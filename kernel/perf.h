#ifndef PERF_H
#define PERF_H

/* This header file defines all performance-related constants and macros. */

/* Enable copy-on-write optimization for safecopy. */
#define PERF_USE_COW_SAFECOPY 0

/* Use a private page table for critical system processes. */
#define PERF_SYS_CORE_FULLVM 0

#endif /* PERF_H */
