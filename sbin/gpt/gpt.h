/*	$NetBSD: gpt.h,v 1.42 2020/05/24 18:42:20 jmcneill Exp $	*/

/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 *
 * $FreeBSD: src/sbin/gpt/gpt.h,v 1.11 2006/06/22 22:05:28 marcel Exp $
 */

#ifndef _GPT_H_
#define	_GPT_H_

#ifndef HAVE_NBTOOL_CONFIG_H
#include <util.h>
#else
#include "opendisk.h"
#include "namespace.h"
#endif

#include "gpt_uuid.h"

struct mbr_part {
	uint8_t		part_flag;		/* bootstrap flags */
	uint8_t		part_shd;		/* starting head */
	uint8_t		part_ssect;		/* starting sector */
	uint8_t		part_scyl;		/* starting cylinder */
	uint8_t		part_typ;		/* partition type */
	uint8_t		part_ehd;		/* end head */
	uint8_t		part_esect;		/* end sector */
	uint8_t		part_ecyl;		/* end cylinder */
	uint16_t	part_start_lo;		/* absolute starting ... */
	uint16_t	part_start_hi;		/* ... sector number */
	uint16_t	part_size_lo;		/* partition size ... */
	uint16_t	part_size_hi;		/* ... in sectors */
};

struct mbr {
	uint8_t		mbr_code[446];
	struct mbr_part	mbr_part[4];
	uint16_t	mbr_sig;
#define	MBR_SIG		0xAA55
};

typedef struct gpt *gpt_t;
typedef struct map *map_t;

struct gpt_cmd {
	const char *name;
	int (*fptr)(gpt_t, int, char *[]);
	const char **help;
	size_t hlen;
	int flags;
};

uint32_t crc32(const void *, size_t);
void	gpt_close(gpt_t);
int	gpt_gpt(gpt_t, off_t, int);
gpt_t	gpt_open(const char *, int, int, off_t, u_int, time_t);
#define GPT_READONLY	0x01
#define GPT_MODIFIED	0x02
#define GPT_QUIET	0x04
#define GPT_NOSYNC	0x08
#define GPT_FILE	0x10
#define GPT_TIMESTAMP	0x20
#define GPT_SYNC	0x40
#define GPT_HYBRID	0x80
#define GPT_OPTDEV      0x8000

void*	gpt_read(gpt_t, off_t, size_t);
off_t	gpt_last(gpt_t);
off_t	gpt_create(gpt_t, off_t, u_int, int);
int	gpt_write(gpt_t, map_t);
int	gpt_write_crc(gpt_t, map_t, map_t);
int	gpt_write_primary(gpt_t);
int	gpt_write_backup(gpt_t);
struct gpt_hdr *gpt_hdr(gpt_t);
void	gpt_msg(gpt_t, const char *, ...) __printflike(2, 3);
void	gpt_warn(gpt_t, const char *, ...) __printflike(2, 3);
void	gpt_warnx(gpt_t, const char *, ...) __printflike(2, 3);
void	gpt_create_pmbr_part(struct mbr_part *, off_t, int);
struct gpt_ent *gpt_ent(map_t, map_t, unsigned int);
struct gpt_ent *gpt_ent_primary(gpt_t, unsigned int);
struct gpt_ent *gpt_ent_backup(gpt_t, unsigned int);
int	gpt_usage(const char *, const struct gpt_cmd *);

void 	utf16_to_utf8(const uint16_t *, size_t, uint8_t *, size_t);
void	utf8_to_utf16(const uint8_t *, uint16_t *, size_t);

#define GPT_FIND "ab:i:L:s:t:"

struct gpt_find {
	int all;
	gpt_uuid_t type;
	off_t block, size;
	unsigned int entry;
	uint8_t *name, *label;
	const char *msg;
};
int	gpt_change_ent(gpt_t, const struct gpt_find *,
    void (*)(struct gpt_ent *, void *, int), void *);
int	gpt_change_hdr(gpt_t, const struct gpt_find *,
    void (*)(struct gpt_hdr *, void *, int), void *);
int	gpt_add_find(gpt_t, struct gpt_find *, int);

#define GPT_AIS "a:i:s:"
int	gpt_add_ais(gpt_t, off_t *, u_int *, off_t *, int);
off_t	gpt_check_ais(gpt_t, off_t, u_int, off_t);

int	gpt_attr_get(gpt_t, uint64_t *);
const char *gpt_attr_list(char *, size_t, uint64_t);
void	gpt_attr_help(const char *);
int	gpt_attr_update(gpt_t, u_int, uint64_t, uint64_t);
int	gpt_uint_get(gpt_t, u_int *);
int	gpt_human_get(gpt_t, off_t *);
int	gpt_uuid_get(gpt_t, gpt_uuid_t *);
int	gpt_name_get(gpt_t, void *);
int	gpt_add_hdr(gpt_t, int, off_t);
void	gpt_show_num(const char *, uintmax_t);

#endif /* _GPT_H_ */
