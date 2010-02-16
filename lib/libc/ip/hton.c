/*
hton.c
*/

#include <sys/types.h>
#include <minix/config.h>
#include <net/hton.h>

u16_t _tmp;
u32_t _tmp_l;

#if _WORD_SIZE > 2
u16_t (htons)(u16_t x) { return HTONS(x); }
u16_t (ntohs)(u16_t x) { return NTOHS(x); }
u32_t (htonl)(u32_t x) { return HTONL(x); }
u32_t (ntohl)(u32_t x) { return NTOHL(x); }
#endif

