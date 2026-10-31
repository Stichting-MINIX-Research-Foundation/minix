/*	$NetBSD: gpt_uuid.c,v 1.19 2020/03/30 10:41:53 martin Exp $	*/

/*-
 * Copyright (c) 2014 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __RCSID
__RCSID("$NetBSD: gpt_uuid.c,v 1.19 2020/03/30 10:41:53 martin Exp $");
#endif

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

#if defined(HAVE_SYS_ENDIAN_H) || ! defined(HAVE_NBTOOL_CONFIG_H)
#include <sys/endian.h>
#endif


const gpt_uuid_t gpt_uuid_nil;

struct dce_uuid {
	uint32_t	time_low;
	uint16_t	time_mid;
	uint16_t	time_hi_and_version;
	uint8_t		clock_seq_hi_and_reserved;
	uint8_t		clock_seq_low;
	uint8_t		node[6];
};

static const struct {
	struct dce_uuid u;
	const char *n;
	const char *d;
} gpt_nv[] = {
	{ GPT_ENT_TYPE_APPLE_HFS, "apple", "Apple HFS" },
	{ GPT_ENT_TYPE_APPLE_UFS, "apple-ufs", "Apple UFS" },
	{ GPT_ENT_TYPE_BIOS, "bios", "BIOS Boot" },
	{ GPT_ENT_TYPE_EFI, "efi", "EFI System" },
	{ GPT_ENT_TYPE_FREEBSD, "fbsd-legacy", "FreeBSD legacy" },
	{ GPT_ENT_TYPE_FREEBSD_SWAP, "fbsd-swap", "FreeBSD swap" },
	{ GPT_ENT_TYPE_FREEBSD_UFS, "fbsd-ufs", "FreeBSD UFS/UFS2" },
	{ GPT_ENT_TYPE_FREEBSD_VINUM, "fbsd-vinum", "FreeBSD vinum" },
	{ GPT_ENT_TYPE_FREEBSD_ZFS, "zfs", "ZFS" },
	{ GPT_ENT_TYPE_LINUX_DATA, "linux-data", "Linux data" },
	{ GPT_ENT_TYPE_LINUX_RAID, "linux-raid", "Linux RAID" },
	{ GPT_ENT_TYPE_LINUX_SWAP, "linux-swap", "Linux swap" },
	{ GPT_ENT_TYPE_LINUX_LVM, "linux-lvm", "Linux LVM" },
	{ GPT_ENT_TYPE_MS_BASIC_DATA, "windows", "Windows basic data" },
	{ GPT_ENT_TYPE_MS_RESERVED, "windows-reserved", "Windows reserved" },
	{ GPT_ENT_TYPE_NETBSD_CCD, "ccd", "NetBSD ccd component" },
	{ GPT_ENT_TYPE_NETBSD_CGD, "cgd", "NetBSD Cryptographic Disk" },
	{ GPT_ENT_TYPE_NETBSD_FFS, "ffs", "NetBSD FFSv1/FFSv2" },
	{ GPT_ENT_TYPE_NETBSD_LFS, "lfs", "NetBSD LFS" },
	{ GPT_ENT_TYPE_NETBSD_RAIDFRAME, "raid",
	    "NetBSD RAIDFrame component" },
	{ GPT_ENT_TYPE_NETBSD_SWAP, "swap", "NetBSD swap" },
	{ GPT_ENT_TYPE_MINIX_MFS, "mfs", "MINIX mfs" },
	{ GPT_ENT_TYPE_VMWARE_VMKCORE, "vmcore", "VMware VMkernel core dump" },
	{ GPT_ENT_TYPE_VMWARE_VMFS, "vmfs", "VMware VMFS" },
	{ GPT_ENT_TYPE_VMWARE_RESERVED, "vmresered", "VMware reserved" },
};

static void
gpt_uuid_to_dce(const gpt_uuid_t buf, struct dce_uuid *uuid)
{
	const uint8_t *p = buf;
	size_t i;

	uuid->time_low = le32dec(p);
	uuid->time_mid = le16dec(p + 4);
	uuid->time_hi_and_version = le16dec(p + 6);
	uuid->clock_seq_hi_and_reserved = p[8];
	uuid->clock_seq_low = p[9];
	for (i = 0; i < sizeof(uuid->node); i++)
		uuid->node[i] = p[10 + i];
}

static void
gpt_dce_to_uuid(const struct dce_uuid *uuid, uint8_t *buf)
{
	uint8_t *p = buf;
	size_t i;

	le32enc(p, uuid->time_low);
	le16enc(p + 4, uuid->time_mid);
	le16enc(p + 6, uuid->time_hi_and_version);
	p[8] = uuid->clock_seq_hi_and_reserved;
	p[9] = uuid->clock_seq_low;
	for (i = 0; i < sizeof(uuid->node); i++)
		p[10 + i] = uuid->node[i];
}

static int
gpt_uuid_numeric(char *buf, size_t bufsiz, const struct dce_uuid *u)
{
	return snprintf(buf, bufsiz,
	    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    u->time_low, u->time_mid, u->time_hi_and_version,
	    u->clock_seq_hi_and_reserved, u->clock_seq_low, u->node[0],
	    u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
}


static int
gpt_uuid_symbolic(char *buf, size_t bufsiz, const struct dce_uuid *u)
{
	size_t i;

	for (i = 0; i < __arraycount(gpt_nv); i++)
		if (memcmp(&gpt_nv[i].u, u, sizeof(*u)) == 0)
			return (int)strlcpy(buf, gpt_nv[i].n, bufsiz);
	return -1;
}

static int
gpt_uuid_descriptive(char *buf, size_t bufsiz, const struct dce_uuid *u)
{
	size_t i;

	for (i = 0; i < __arraycount(gpt_nv); i++)
		if (memcmp(&gpt_nv[i].u, u, sizeof(*u)) == 0)
			return (int)strlcpy(buf, gpt_nv[i].d, bufsiz);
	return -1;
}

int
gpt_uuid_snprintf(char *buf, size_t bufsiz, const char *fmt,
    const gpt_uuid_t uu)
{
	struct dce_uuid u;
	gpt_uuid_to_dce(uu, &u);

	if (fmt[1] == 's') {
		int r;
		if ((r = gpt_uuid_symbolic(buf, bufsiz, &u)) != -1)
			return r;
	}
	if (fmt[1] == 'l') {
		int r;
		if ((r = gpt_uuid_descriptive(buf, bufsiz, &u)) != -1)
			return r;
	}
	return gpt_uuid_numeric(buf, bufsiz, &u);
}

static int
gpt_uuid_parse_numeric(const char *s, struct dce_uuid *u)
{
	int n;

	if (s == NULL || *s == '\0') {
		memset(u, 0, sizeof(*u));
		return 0;
	}

	n = sscanf(s,
	    "%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
	    &u->time_low, &u->time_mid, &u->time_hi_and_version,
	    &u->clock_seq_hi_and_reserved, &u->clock_seq_low, &u->node[0],
	    &u->node[1], &u->node[2], &u->node[3], &u->node[4], &u->node[5]);

	/* Make sure we have all conversions. */
	if (n != 11)
		return -1;

	/* We have a successful scan. Check semantics... */
	n = u->clock_seq_hi_and_reserved;
	if ((n & 0x80) != 0x00 &&			/* variant 0? */
	    (n & 0xc0) != 0x80 &&			/* variant 1? */
	    (n & 0xe0) != 0xc0) 			/* variant 2? */
		return -1;
	return 0;
}

static int
gpt_uuid_parse_symbolic(const char *s, struct dce_uuid *u)
{
	size_t i;

	for (i = 0; i < __arraycount(gpt_nv); i++)
		if (strcmp(gpt_nv[i].n, s) == 0) {
			*u = gpt_nv[i].u;
			return 0;
		}
	return -1;
}

int
gpt_uuid_parse(const char *s, gpt_uuid_t uuid)
{
	struct dce_uuid u;

	if (gpt_uuid_parse_numeric(s, &u) != -1) {
		gpt_dce_to_uuid(&u, uuid);
		return 0;
	}

	if (gpt_uuid_parse_symbolic(s, &u) == -1)
		return -1;

	gpt_dce_to_uuid(&u, uuid);
	return 0;
}

size_t
gpt_uuid_query(
    void (*func)(const char *uuid, const char *short_name, const char *desc))
{
	size_t i;
	char buf[64];

	if (func != NULL) {
		for (i = 0; i < __arraycount(gpt_nv); i++) {
			gpt_uuid_numeric(buf, sizeof(buf), &gpt_nv[i].u);
			(*func)(buf, gpt_nv[i].n, gpt_nv[i].d);
		}
	}
	return __arraycount(gpt_nv);
}

#ifndef GPT_UUID_QUERY_ONLY
void
gpt_uuid_help(const char *prefix)
{
	size_t i;

	for (i = 0; i < __arraycount(gpt_nv); i++)
		printf("%s%18.18s\t%s\n", prefix, gpt_nv[i].n, gpt_nv[i].d);
}

void
gpt_uuid_create(gpt_type_t t, gpt_uuid_t u, uint16_t *b, size_t s)
{
	gpt_dce_to_uuid(&gpt_nv[t].u, u);
	if (b)
		utf8_to_utf16((const uint8_t *)gpt_nv[t].d, b, s / sizeof(*b));
}

static int
gpt_uuid_random(gpt_t gpt, void *v, size_t n)
{
	int fd;
	uint8_t *p;
	ssize_t nread;

	/* Randomly generate the content.  */
	fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd == -1) {
		gpt_warn(gpt, "Can't open `/dev/urandom'");
		return -1;
	}
	for (p = v;  n > 0; p += nread, n -= (size_t)nread) {
		nread = read(fd, p, n);
		if (nread < 0) {
			gpt_warn(gpt, "Can't read `/dev/urandom'");
			goto out;
		}
		if (nread == 0) {
			gpt_warn(gpt, "EOF from /dev/urandom");
			goto out;
		}
		if ((size_t)nread > n) {
			gpt_warnx(gpt, "read too much: %zd > %zu", nread, n);
			goto out;
		}
	}
	(void)close(fd);
	return 0;
out:
	(void)close(fd);
	return -1;
}

static int 
gpt_uuid_tstamp(gpt_t gpt, void *v, size_t l)
{
	uint8_t *p;
	// Perhaps use SHA?
	uint32_t x = (uint32_t)gpt->timestamp;

	for (p = v; l > 0; p += sizeof(x), l -= sizeof(x))
		memcpy(p, &x, sizeof(x));

	return 0;
}

int
gpt_uuid_generate(gpt_t gpt, gpt_uuid_t t)
{
	int rv;
	struct dce_uuid u;
	if (gpt && (gpt->flags & GPT_TIMESTAMP))
		rv = gpt_uuid_tstamp(gpt, &u, sizeof(u));
	else
		rv = gpt_uuid_random(gpt, &u, sizeof(u));

	if (rv == -1)
		return -1;

	/* Set the version number to 4.  */
	u.time_hi_and_version &= (uint16_t)~0xf000;
	u.time_hi_and_version |= 0x4000;

	/* Fix the reserved bits.  */
	u.clock_seq_hi_and_reserved &= (uint8_t)~0x40;
	u.clock_seq_hi_and_reserved |= 0x80;

	gpt_dce_to_uuid(&u, t);
	return 0;
}
#endif
