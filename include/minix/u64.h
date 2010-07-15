/*	minix/u64.h					Author: Kees J. Bot
 *								7 Dec 1995
 * Functions to manipulate 64 bit disk addresses.
 */
#ifndef _MINIX__U64_H
#define _MINIX__U64_H

#ifndef _TYPES_H
#include <minix/types.h>
#endif

u64_t add64(u64_t i, u64_t j);
u64_t add64u(u64_t i, unsigned j);
u64_t add64ul(u64_t i, unsigned long j);
u64_t sub64(u64_t i, u64_t j);
u64_t sub64u(u64_t i, unsigned j);
u64_t sub64ul(u64_t i, unsigned long j);
int bsr64(u64_t i);
unsigned diff64(u64_t i, u64_t j);
u64_t cvu64(unsigned i);
u64_t cvul64(unsigned long i);
unsigned cv64u(u64_t i);
unsigned long cv64ul(u64_t i);
u64_t div64(u64_t i, u64_t j);
unsigned long div64u(u64_t i, unsigned j);
u64_t div64u64(u64_t i, unsigned j);
u64_t rem64(u64_t i, u64_t j);
unsigned rem64u(u64_t i, unsigned j);
u64_t mul64(u64_t i, u64_t j);
u64_t mul64u(unsigned long i, unsigned j);
int cmp64(u64_t i, u64_t j);
int cmp64u(u64_t i, unsigned j);
int cmp64ul(u64_t i, unsigned long j);
unsigned long ex64lo(u64_t i);
unsigned long ex64hi(u64_t i);
u64_t make64(unsigned long lo, unsigned long hi);
u64_t rrotate64(u64_t x, unsigned short b);
u64_t rshift64(u64_t x, unsigned short b);
u64_t xor64(u64_t a, u64_t b);
u64_t and64(u64_t a, u64_t b);
u64_t not64(u64_t a);

#define is_zero64(i)	((i).lo == 0 && (i).hi == 0)
#define make_zero64(i)	do { (i).lo = (i).hi = 0; } while(0)

#endif /* _MINIX__U64_H */
