/*	$NetBSD: crc32.c,v 1.4 2009/03/26 22:18:14 he Exp $	*/

/* crc32.c -- compute the CRC-32 of a data stream
 *
 * Adapted from zlib's crc code.
 *
 * Copyright (C) 1995-2005 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of data at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 */

/* @(#) Id */

#include <sys/param.h>
#include <machine/endian.h>

typedef uint32_t u4;

/* Definitions for doing the crc four data bytes at a time. */
#define REV(w) (((w)>>24)+(((w)>>8)&0xff00)+ \
               (((w)&0xff00)<<8)+(((w)&0xff)<<24))

/* ========================================================================
 * Tables of CRC-32s of all single-byte values, made by make_crc_table().
 */
#include <lib/libkern/libkern.h>
#include "crc32.h"

#if BYTE_ORDER == LITTLE_ENDIAN
/* ========================================================================= */
#define DOLIT4 c ^= *buf4++; \
        c = crc_table[3][c & 0xff] ^ crc_table[2][(c >> 8) & 0xff] ^ \
            crc_table[1][(c >> 16) & 0xff] ^ crc_table[0][c >> 24]
#define DOLIT32 DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4; DOLIT4

/* ========================================================================= */
uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
    register u4 c;
    register const u4 *buf4;

    if (buf == NULL) return 0UL;

    c = (u4)crc;
    c = ~c;
    while (len && ((uintptr_t)buf & 3)) {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
        len--;
    }

    buf4 = (const u4 *)(const void *)buf;
    while (len >= 32) {
        DOLIT32;
        len -= 32;
    }
    while (len >= 4) {
        DOLIT4;
        len -= 4;
    }
    buf = (const unsigned char *)buf4;

    if (len) do {
        c = crc_table[0][(c ^ *buf++) & 0xff] ^ (c >> 8);
    } while (--len);
    c = ~c;
    return (uint32_t)c;
}

#else /* BIG_ENDIAN */

/* ========================================================================= */
#define DOBIG4 c ^= *++buf4; \
        c = crc_table[4][c & 0xff] ^ crc_table[5][(c >> 8) & 0xff] ^ \
            crc_table[6][(c >> 16) & 0xff] ^ crc_table[7][c >> 24]
#define DOBIG32 DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4; DOBIG4

/* ========================================================================= */
uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
    register u4 c;
    register const u4 *buf4;

    if (buf == NULL) return 0UL;

    c = REV((u4)crc);
    c = ~c;
    while (len && ((uintptr_t)buf & 3)) {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
        len--;
    }

    buf4 = (const u4 *)(const void *)buf;
    buf4--;
    while (len >= 32) {
        DOBIG32;
        len -= 32;
    }
    while (len >= 4) {
        DOBIG4;
        len -= 4;
    }
    buf4++;
    buf = (const unsigned char *)buf4;

    if (len) do {
        c = crc_table[4][(c >> 24) ^ *buf++] ^ (c << 8);
    } while (--len);
    c = ~c;
    return (uint32_t)(REV(c));
}
#endif
