#ifndef MINIX_UTILS_H
#define MINIX_UTILS_H
#include <minix/u64.h>
#if !defined(__LONG_LONG_SUPPORTED)
u64_t lshift64(u64_t x, unsigned short b);
#endif
#endif
