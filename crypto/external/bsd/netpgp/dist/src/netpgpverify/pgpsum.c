/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "digest.h"
#include "pgpsum.h"

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

/* add the ascii armor line endings (except for last line) */
static size_t
don_armor(digest_t *hash, uint8_t *in, size_t insize, int doarmor)
{
	uint8_t	*from;
	uint8_t	*newp;
	uint8_t	*p;
	uint8_t	 dos_line_end[2];

	dos_line_end[0] = '\r';
	dos_line_end[1] = '\n';
	for (from = in ; (p = memchr(from, '\n', insize - (size_t)(from - in))) != NULL ; from = p + 1) {
		for (newp = p ; doarmor == 'w' && newp > from ; --newp) {
			if (*(newp - 1) != ' ' && *(newp - 1) != '\t') {
				break;
			}
		}
		digest_update(hash, from, (size_t)(newp - from));
		digest_update(hash, dos_line_end, sizeof(dos_line_end));
	}
	digest_update(hash, from, insize - (size_t)(from - in));
	return 1;
}

#ifdef NETPGPV_DEBUG
/* just for giggles, write what we're about to checksum */
static int
writefile(uint8_t *mem, size_t insize)
{
	size_t	cc;
	size_t	wc;
	char	template[256];
	int	fd;

	snprintf(template, sizeof(template), "netpgpvmd.XXXXXX");
	if ((fd = mkstemp(template)) < 0) {
		fprintf(stderr, "can't mkstemp %s\n", template);
		return 0;
	}
	for (cc = 0 ; cc < insize ; cc += wc) {
		if ((wc = write(fd, &mem[cc], insize - cc)) <= 0) {
			fprintf(stderr, "short write\n");
			break;
		}
	}
	close(fd);
	return 1;
}
#endif

/* return non-zero if this is actually an armored piece already */
static int
already_armored(uint8_t *in, size_t insize)
{
	uint8_t	*from;
	uint8_t	*p;

	for (from = in ; (p = memchr(from, '\n', insize - (size_t)(from - in))) != NULL ; from = p + 1) {
		if (*(p - 1) != '\r') {
			return 0;
		}
	}
	return 1;
}

/* calculate the checksum for the data we have */
static int
calcsum(uint8_t *out, size_t size, uint8_t *mem, size_t cc, const uint8_t *hashed, size_t hashsize, int doarmor)
{
	digest_t	 hash;
	uint32_t	 len32;
	uint16_t	 len16;
	uint8_t		 hashalg;
	uint8_t		 trailer[6];

	USE_ARG(size);
	/* hashed data is non-null (previously checked) */
	hashalg = hashed[3];
	memcpy(&len16, &hashed[4], sizeof(len16));
	len32 = pgp_ntoh16(len16) + 6;
	len32 = pgp_hton32(len32);
	trailer[0] = 0x04;
	trailer[1] = 0xff;
	memcpy(&trailer[2], &len32, sizeof(len32));
#ifdef NETPGPV_DEBUG
	writefile(mem, cc);
#endif
	digest_init(&hash, (const unsigned)hashalg);
	if (strchr("tw", doarmor) != NULL && !already_armored(mem, cc)) {
		/* this took me ages to find - something causes gpg to truncate its input */
		don_armor(&hash, mem, cc - 1, doarmor);
	} else {
		digest_update(&hash, mem, cc);
	}
	if (hashed) {
		digest_update(&hash, hashed, hashsize);
	}
	digest_update(&hash, trailer, sizeof(trailer));
	return digest_final(out, &hash);
}

/* used to byteswap 16 bit words */
typedef union {
	uint16_t	i16;
	uint8_t		i8[2];
} u16;

/* used to byte swap 32 bit words */
typedef union {
	uint32_t	i32;
	uint8_t		i8[4];
} u32;

static inline uint16_t
swap16(uint16_t in)
{
	u16	u;

	u.i16 = in;
	return (u.i8[0] << 8) | u.i8[1];
}

static inline uint32_t
swap32(uint32_t in)
{
	u32	u;

	u.i32 = in;
	return (u.i8[0] << 24) | (u.i8[1] << 16) | (u.i8[2] << 8) | u.i8[3];
}

static inline int
is_little_endian(void)
{
	static const int	indian = 1;

	return (*(const char *)(const void *)&indian != 0);
}

/************************************************************/

/* exportable routines */

/* open the file, mmap it, and then get the checksum on that */
int
pgpv_digest_file(uint8_t *data, size_t size, const char *name, const uint8_t *hashed, size_t hashsize, int doarmor)
{
	struct stat	 st;
	uint8_t		*mem;
	size_t		 cc;
	FILE		*fp;
	int		 ret;

	if (hashed == NULL || data == NULL || name == NULL) {
		fprintf(stderr, "no hashed data provided\n");
		return 0;
	}
	ret = 0;
	mem = NULL;
	cc = 0;
	if ((fp = fopen(name, "r")) == NULL) {
		fprintf(stderr, "%s - not found", name);
		return 0;
	}
	if (fstat(fileno(fp), &st) < 0) {
		fprintf(stderr, "%s - can't stat", name);
		goto done;
	}
	cc = (size_t)(st.st_size);
	if ((mem = mmap(NULL, cc, PROT_READ, MAP_SHARED, fileno(fp), 0)) == MAP_FAILED) {
		fprintf(stderr, "%s - can't mmap", name);
		goto done;
	}
	ret = calcsum(data, size, mem, cc, hashed, hashsize, doarmor);
done:
	if (data) {
		munmap(mem, cc);
	}
	fclose(fp);
	return ret;
}

/* calculate the digest over memory too */
int
pgpv_digest_memory(uint8_t *data, size_t size, void *mem, size_t cc, const uint8_t *hashed, size_t hashsize, int doarmor)
{
	if (hashed == NULL || data == NULL || mem == NULL) {
		fprintf(stderr, "no hashed data provided\n");
		return 0;
	}
	return calcsum(data, size, mem, cc, hashed, hashsize, doarmor);
}

/* our 16bit byte swap if LE host */
uint16_t
pgp_ntoh16(uint16_t in)
{
	return (is_little_endian()) ? swap16(in) : in;
}

/* our 16bit byte swap if LE host */
uint16_t
pgp_hton16(uint16_t in)
{
	return (is_little_endian()) ? swap16(in) : in;
}

/* our 32bit byte swap if LE host */
uint32_t
pgp_ntoh32(uint32_t in)
{
	return (is_little_endian()) ? swap32(in) : in;
}

/* our 32bit byte swap if LE host */
uint32_t
pgp_hton32(uint32_t in)
{
	return (is_little_endian()) ? swap32(in) : in;
}
