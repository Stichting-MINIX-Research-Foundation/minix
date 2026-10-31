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
 * CRC32 code derived from work by Gary S. Brown.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD: src/sbin/gpt/gpt.c,v 1.16 2006/07/07 02:44:23 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: gpt.c,v 1.82 2020/05/24 18:42:20 jmcneill Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/bootblock.h>
#ifdef __minix
#include <sys/ioc_disk.h>
#include <minix/partition.h>
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t
crc32(const void *buf, size_t size)
{
	const uint8_t *p;
	uint32_t crc;

	p = buf;
	crc = ~0U;

	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}

/*
 * Produce a NUL-terminated utf-8 string from the non-NUL-terminated
 * utf16 string.
 */
void
utf16_to_utf8(const uint16_t *s16, size_t s16len, uint8_t *s8, size_t s8len)
{
	size_t s8idx, s16idx;
	uint32_t utfchar;
	unsigned int c;

	for (s16idx = 0; s16idx < s16len; s16idx++)
		if (s16[s16idx] == 0)
			break;

	s16len = s16idx;
	s8idx = s16idx = 0;
	while (s16idx < s16len) {
		utfchar = le16toh(s16[s16idx++]);
		if ((utfchar & 0xf800) == 0xd800) {
			c = le16toh(s16[s16idx]);
			if ((utfchar & 0x400) != 0 || (c & 0xfc00) != 0xdc00)
				utfchar = 0xfffd;
			else
				s16idx++;
		}
		if (utfchar < 0x80) {
			if (s8idx + 1 >= s8len)
				break;
			s8[s8idx++] = (uint8_t)utfchar;
		} else if (utfchar < 0x800) {
			if (s8idx + 2 >= s8len)
				break;
			s8[s8idx++] = (uint8_t)(0xc0 | (utfchar >> 6));
			s8[s8idx++] = (uint8_t)(0x80 | (utfchar & 0x3f));
		} else if (utfchar < 0x10000) {
			if (s8idx + 3 >= s8len)
				break;
			s8[s8idx++] = (uint8_t)(0xe0 | (utfchar >> 12));
			s8[s8idx++] = (uint8_t)(0x80 | ((utfchar >> 6) & 0x3f));
			s8[s8idx++] = (uint8_t)(0x80 | (utfchar & 0x3f));
		} else if (utfchar < 0x200000) {
			if (s8idx + 4 >= s8len)
				break;
			s8[s8idx++] = (uint8_t)(0xf0 | (utfchar >> 18));
			s8[s8idx++] = (uint8_t)(0x80 | ((utfchar >> 12) & 0x3f));
			s8[s8idx++] = (uint8_t)(0x80 | ((utfchar >> 6) & 0x3f));
			s8[s8idx++] = (uint8_t)(0x80 | (utfchar & 0x3f));
		}
	}
	s8[s8idx] = 0;
}

/*
 * Produce a non-NUL-terminated utf-16 string from the NUL-terminated
 * utf8 string.
 */
void
utf8_to_utf16(const uint8_t *s8, uint16_t *s16, size_t s16len)
{
	size_t s16idx, s8idx, s8len;
	uint32_t utfchar = 0;
	unsigned int c, utfbytes;

	s8len = 0;
	while (s8[s8len++] != 0)
		;
	s8idx = s16idx = 0;
	utfbytes = 0;
	do {
		c = s8[s8idx++];
		if ((c & 0xc0) != 0x80) {
			/* Initial characters. */
			if (utfbytes != 0) {
				/* Incomplete encoding. */
				s16[s16idx++] = htole16(0xfffd);
				if (s16idx == s16len) {
					s16[--s16idx] = 0;
					return;
				}
			}
			if ((c & 0xf8) == 0xf0) {
				utfchar = c & 0x07;
				utfbytes = 3;
			} else if ((c & 0xf0) == 0xe0) {
				utfchar = c & 0x0f;
				utfbytes = 2;
			} else if ((c & 0xe0) == 0xc0) {
				utfchar = c & 0x1f;
				utfbytes = 1;
			} else {
				utfchar = c & 0x7f;
				utfbytes = 0;
			}
		} else {
			/* Followup characters. */
			if (utfbytes > 0) {
				utfchar = (utfchar << 6) + (c & 0x3f);
				utfbytes--;
			} else if (utfbytes == 0)
				utfbytes = (u_int)~0;
		}
		if (utfbytes == 0) {
			if (utfchar >= 0x10000 && s16idx + 2 >= s16len)
				utfchar = 0xfffd;
			if (utfchar >= 0x10000) {
				s16[s16idx++] = htole16((uint16_t)
				    (0xd800 | ((utfchar>>10) - 0x40)));
				s16[s16idx++] = htole16((uint16_t)
				    (0xdc00 | (utfchar & 0x3ff)));
			} else
				s16[s16idx++] = htole16((uint16_t)utfchar);
			if (s16idx == s16len) {
				return;
			}
		}
	} while (c != 0);

	while (s16idx < s16len)
		s16[s16idx++] = 0;
}

void *
gpt_read(gpt_t gpt, off_t lba, size_t count)
{
	off_t ofs;
	void *buf;

	count *= gpt->secsz;
	buf = malloc(count);
	if (buf == NULL)
		return NULL;

	ofs = lba * gpt->secsz;
	if (lseek(gpt->fd, ofs, SEEK_SET) == ofs &&
	    read(gpt->fd, buf, count) == (ssize_t)count)
		return buf;

	free(buf);
	return NULL;
}

int
gpt_write(gpt_t gpt, map_t map)
{
	off_t ofs;
	size_t count;

	count = (size_t)(map->map_size * gpt->secsz);
	ofs = map->map_start * gpt->secsz;
	if (lseek(gpt->fd, ofs, SEEK_SET) != ofs ||
	    write(gpt->fd, map->map_data, count) != (ssize_t)count)
		return -1;
	gpt->flags |= GPT_MODIFIED;
	return 0;
}

static int
gpt_mbr(gpt_t gpt, off_t lba, unsigned int *next_index, off_t ext_offset)
{
	struct mbr *mbr;
	map_t m, p;
	off_t size, start;
	unsigned int i, pmbr;

	mbr = gpt_read(gpt, lba, 1);
	if (mbr == NULL) {
		gpt_warn(gpt, "Read failed");
		return -1;
	}

	if (mbr->mbr_sig != htole16(MBR_SIG)) {
		if (gpt->verbose)
			gpt_msg(gpt,
			    "MBR not found at sector %ju", (uintmax_t)lba);
		free(mbr);
		return 0;
	}

	/*
	 * Differentiate between a regular MBR and a PMBR. This is more
	 * convenient in general. A PMBR is one with a single partition
	 * of type 0xee.
	 */
	pmbr = 0;
	for (i = 0; i < 4; i++) {
		if (mbr->mbr_part[i].part_typ == MBR_PTYPE_UNUSED)
			continue;
		if (mbr->mbr_part[i].part_typ == MBR_PTYPE_PMBR)
			pmbr++;
		else if ((gpt->flags & GPT_HYBRID) == 0)
			break;
	}
	if (pmbr && i == 4 && lba == 0) {
		if (pmbr != 1)
			gpt_warnx(gpt, "Suspicious PMBR at sector %ju",
			    (uintmax_t)lba);
		else if (gpt->verbose > 1)
			gpt_msg(gpt, "PMBR at sector %ju", (uintmax_t)lba);
		p = map_add(gpt, lba, 1LL, MAP_TYPE_PMBR, mbr, 1);
		goto out;
	}
	if (pmbr)
		gpt_warnx(gpt, "Suspicious MBR at sector %ju", (uintmax_t)lba);
	else if (gpt->verbose > 1)
		gpt_msg(gpt, "MBR at sector %ju", (uintmax_t)lba);

	p = map_add(gpt, lba, 1LL, MAP_TYPE_MBR, mbr, 1);
	if (p == NULL)
		goto out;

	for (i = 0; i < 4; i++) {
		if (mbr->mbr_part[i].part_typ == MBR_PTYPE_UNUSED ||
		    mbr->mbr_part[i].part_typ == MBR_PTYPE_PMBR)
			continue;
		start = le16toh(mbr->mbr_part[i].part_start_hi);
		start = (start << 16) + le16toh(mbr->mbr_part[i].part_start_lo);
		size = le16toh(mbr->mbr_part[i].part_size_hi);
		size = (size << 16) + le16toh(mbr->mbr_part[i].part_size_lo);
		if (start == 0 && size == 0) {
			gpt_warnx(gpt, "Malformed MBR at sector %ju",
			    (uintmax_t)lba);
			continue;
		}
		if (gpt->verbose > 2)
			gpt_msg(gpt, "MBR part: flag=%#x type=%d, start=%ju, "
			    "size=%ju", mbr->mbr_part[i].part_flag,
			    mbr->mbr_part[i].part_typ,
			    (uintmax_t)start, (uintmax_t)size);
		if (!MBR_IS_EXTENDED(mbr->mbr_part[i].part_typ)) {
			start += lba;
			m = map_add(gpt, start, size, MAP_TYPE_MBR_PART, p, 0);
			if (m == NULL)
				return -1;
			m->map_index = *next_index;
			(*next_index)++;
		} else {
			start += ext_offset;
			if (gpt_mbr(gpt, start, next_index,
			    ext_offset ? ext_offset : start) == -1)
				return -1;
		}
	}
	return 0;
out:
	if (p == NULL) {
		free(mbr);
		return -1;
	}
	return 0;
}

int
gpt_gpt(gpt_t gpt, off_t lba, int found)
{
	off_t size;
	struct gpt_ent *ent;
	struct gpt_hdr *hdr;
	char *p;
	map_t m;
	size_t blocks, tblsz;
	unsigned int i;
	uint32_t crc;

	hdr = gpt_read(gpt, lba, 1);
	if (hdr == NULL)
		return -1;

	if (memcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)))
		goto fail_hdr;

	crc = le32toh(hdr->hdr_crc_self);
	hdr->hdr_crc_self = 0;
	if (crc32(hdr, le32toh(hdr->hdr_size)) != crc) {
		if (gpt->verbose)
			gpt_msg(gpt, "Bad CRC in GPT header at sector %ju",
			    (uintmax_t)lba);
		goto fail_hdr;
	}

	tblsz = le32toh(hdr->hdr_entries) * le32toh(hdr->hdr_entsz);
	blocks = tblsz / gpt->secsz + ((tblsz % gpt->secsz) ? 1 : 0);

	/* Use generic pointer to deal with hdr->hdr_entsz != sizeof(*ent). */
	p = gpt_read(gpt, (off_t)le64toh((uint64_t)hdr->hdr_lba_table), blocks);
	if (p == NULL) {
		if (found) {
			if (gpt->verbose)
				gpt_msg(gpt,
				    "Cannot read LBA table at sector %ju",
				    (uintmax_t)le64toh(hdr->hdr_lba_table));
			return -1;
		}
		goto fail_hdr;
	}

	if (crc32(p, tblsz) != le32toh(hdr->hdr_crc_table)) {
		if (gpt->verbose)
			gpt_msg(gpt, "Bad CRC in GPT table at sector %ju",
			    (uintmax_t)le64toh(hdr->hdr_lba_table));
		goto fail_ent;
	}

	if (gpt->verbose > 1)
		gpt_msg(gpt, "%s GPT at sector %ju",
		    (lba == 1) ? "Pri" : "Sec", (uintmax_t)lba);

	m = map_add(gpt, lba, 1, (lba == 1)
	    ? MAP_TYPE_PRI_GPT_HDR : MAP_TYPE_SEC_GPT_HDR, hdr, 1);
	if (m == NULL)
		return (-1);

	m = map_add(gpt, (off_t)le64toh((uint64_t)hdr->hdr_lba_table),
	    (off_t)blocks,
	    lba == 1 ? MAP_TYPE_PRI_GPT_TBL : MAP_TYPE_SEC_GPT_TBL, p, 1);
	if (m == NULL)
		return (-1);

	if (lba != 1)
		return (1);

	for (i = 0; i < le32toh(hdr->hdr_entries); i++) {
		ent = (void*)(p + i * le32toh(hdr->hdr_entsz));
		if (gpt_uuid_is_nil(ent->ent_type))
			continue;

		size = (off_t)(le64toh((uint64_t)ent->ent_lba_end) -
		    le64toh((uint64_t)ent->ent_lba_start) + 1LL);
		if (gpt->verbose > 2) {
			char buf[128];
			gpt_uuid_snprintf(buf, sizeof(buf), "%s", 
			    ent->ent_type);
			gpt_msg(gpt, "GPT partition: type=%s, start=%ju, "
			    "size=%ju", buf,
			    (uintmax_t)le64toh(ent->ent_lba_start),
			    (uintmax_t)size);
		}
		m = map_add(gpt, (off_t)le64toh((uint64_t)ent->ent_lba_start),
		    size, MAP_TYPE_GPT_PART, ent, 0);
		if (m == NULL)
			return (-1);
		m->map_index = i + 1;
	}
	return (1);

 fail_ent:
	free(p);

 fail_hdr:
	free(hdr);
	return (0);
}

gpt_t
gpt_open(const char *dev, int flags, int verbose, off_t mediasz, u_int secsz,
    time_t timestamp)
{
	int mode, found;
	off_t devsz;
	gpt_t gpt;
	unsigned int index;

	if ((gpt = calloc(1, sizeof(*gpt))) == NULL) {
		if (!(flags & GPT_QUIET))
			warn("Cannot allocate `%s'", dev);
		return NULL;
	}
	gpt->flags = flags;
	gpt->verbose = verbose;
	gpt->mediasz = mediasz;
	gpt->secsz = secsz;
	gpt->timestamp = timestamp;

	mode = (gpt->flags & GPT_READONLY) ? O_RDONLY : O_RDWR|O_EXCL;
		
	gpt->fd = opendisk(dev, mode, gpt->device_name,
	    sizeof(gpt->device_name), 0);
	if (gpt->fd == -1) {
		strlcpy(gpt->device_name, dev, sizeof(gpt->device_name));
		gpt_warn(gpt, "Cannot open");
		goto close;
	}

	if (fstat(gpt->fd, &gpt->sb) == -1) {
		gpt_warn(gpt, "Cannot stat");
		goto close;
	}

	if ((gpt->sb.st_mode & S_IFMT) != S_IFREG) {
#ifdef __minix
		/*
		 * MINIX block drivers do not implement DIOCGSECTORSIZE/
		 * DIOCGMEDIASIZE; they expose geometry via DIOCGETP, which
		 * returns the partition size in bytes.  Sector size is a
		 * fixed 512 bytes.
		 */
		struct part_geom geom;

		if (ioctl(gpt->fd, DIOCGETP, &geom) == -1) {
			gpt_warn(gpt, "Cannot get partition geometry");
			goto close;
		}
		if (gpt->secsz == 0)
			gpt->secsz = 512;
		if (gpt->mediasz == 0)
			gpt->mediasz = (off_t)geom.size;
		if (gpt->mediasz == 0) {
			gpt_warnx(gpt, "Media size can't be 0");
			goto close;
		}
#else
		if (gpt->secsz == 0) {
#ifdef DIOCGSECTORSIZE
			if (ioctl(gpt->fd, DIOCGSECTORSIZE, &gpt->secsz) == -1) {
				gpt_warn(gpt, "Cannot get sector size");
				goto close;
			}
#endif
			if (gpt->secsz == 0) {
				gpt_warnx(gpt, "Sector size can't be 0");
				goto close;
			}
		}
		if (gpt->mediasz == 0) {
#ifdef DIOCGMEDIASIZE
			if (ioctl(gpt->fd, DIOCGMEDIASIZE, &gpt->mediasz) == -1) {
				gpt_warn(gpt, "Cannot get media size");
				goto close;
			}
#endif
			if (gpt->mediasz == 0) {
				gpt_warnx(gpt, "Media size can't be 0");
				goto close;
			}
		}
#endif /* __minix */
	} else {
		gpt->flags |= GPT_FILE;
		if (gpt->secsz == 0)
			gpt->secsz = 512;	/* Fixed size for files. */
		if (gpt->mediasz == 0) {
			if (gpt->sb.st_size % gpt->secsz) {
				errno = EINVAL;
				goto close;
			}
			gpt->mediasz = gpt->sb.st_size;
		}
		gpt->flags |= GPT_NOSYNC;
	}

	/*
	 * We require an absolute minimum of 6 sectors. One for the MBR,
	 * 2 for the GPT header, 2 for the GPT table and one to hold some
	 * user data. Let's catch this extreme border case here so that
	 * we don't have to worry about it later.
	 */
	devsz = gpt->mediasz / gpt->secsz;
	if (devsz < 6) {
		gpt_warnx(gpt, "Need 6 sectors, we have %ju",
		    (uintmax_t)devsz);
		goto close;
	}

	if (gpt->verbose) {
		gpt_msg(gpt, "mediasize=%ju; sectorsize=%u; blocks=%ju",
		    (uintmax_t)gpt->mediasz, gpt->secsz, (uintmax_t)devsz);
	}

	if (map_init(gpt, devsz) == -1)
		goto close;

	index = 1;
	if (gpt_mbr(gpt, 0LL, &index, 0U) == -1)
		goto close;
	if ((found = gpt_gpt(gpt, 1LL, 1)) == -1)
		goto close;
	if (gpt_gpt(gpt, devsz - 1LL, found) == -1)
		goto close;

	return gpt;

 close:
	if (gpt->fd != -1)
		close(gpt->fd);
	free(gpt);
	return NULL;
}

void
gpt_close(gpt_t gpt)
{

	if (gpt == NULL)
		return;

	if (!(gpt->flags & GPT_MODIFIED) || !(gpt->flags & GPT_SYNC))
		goto out;

	if (!(gpt->flags & GPT_NOSYNC)) {
#ifdef DIOCMWEDGES
		int bits;
		if (ioctl(gpt->fd, DIOCMWEDGES, &bits) == -1)
			gpt_warn(gpt, "Can't update wedge information");
		else
			goto out;
#endif
	}
	if (!(gpt->flags & GPT_FILE))
		gpt_msg(gpt, "You need to run \"dkctl %s makewedges\""
		    " for the changes to take effect\n", gpt->device_name);

out:
	close(gpt->fd);
}

__printflike(2, 0)
static void
gpt_vwarnx(gpt_t gpt, const char *fmt, va_list ap, const char *e)
{
	if (gpt && (gpt->flags & GPT_QUIET))
		return;
	fprintf(stderr, "%s: ", getprogname());
	if (gpt)
		fprintf(stderr, "%s: ", gpt->device_name);
	vfprintf(stderr, fmt, ap);
	if (e)
		fprintf(stderr, " (%s)\n", e);
	else
		fputc('\n', stderr);
}

void
gpt_warnx(gpt_t gpt, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	gpt_vwarnx(gpt, fmt, ap, NULL);
	va_end(ap);
}

void
gpt_warn(gpt_t gpt, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	gpt_vwarnx(gpt, fmt, ap, strerror(errno));
	va_end(ap);
}

void
gpt_msg(gpt_t gpt, const char *fmt, ...)
{
	va_list ap;

	if (gpt && (gpt->flags & GPT_QUIET))
		return;
	if (gpt)
		printf("%s: ", gpt->device_name);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

struct gpt_hdr *
gpt_hdr(gpt_t gpt)
{
	gpt->gpt = map_find(gpt, MAP_TYPE_PRI_GPT_HDR);
	if (gpt->gpt == NULL) {
		gpt_warnx(gpt, "No primary GPT header; run create or recover");
		return NULL;
	}

	gpt->tpg = map_find(gpt, MAP_TYPE_SEC_GPT_HDR);
	if (gpt->tpg == NULL) {
		gpt_warnx(gpt, "No secondary GPT header; run recover");
		return NULL;
	}

	gpt->tbl = map_find(gpt, MAP_TYPE_PRI_GPT_TBL);
	gpt->lbt = map_find(gpt, MAP_TYPE_SEC_GPT_TBL);
	if (gpt->tbl == NULL || gpt->lbt == NULL) {
		gpt_warnx(gpt, "Corrupt maps, run recover");
		return NULL;
	}

	return gpt->gpt->map_data;
}

int
gpt_write_crc(gpt_t gpt, map_t map, map_t tbl)
{
	struct gpt_hdr *hdr = map->map_data;

	hdr->hdr_crc_table = htole32(crc32(tbl->map_data,
	    le32toh(hdr->hdr_entries) * le32toh(hdr->hdr_entsz)));
	hdr->hdr_crc_self = 0;
	hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));

	if (gpt_write(gpt, map) == -1) {
		gpt_warn(gpt, "Error writing crc map");
		return -1;
	}

	if (gpt_write(gpt, tbl) == -1) {
		gpt_warn(gpt, "Error writing crc table");
		return -1;
	}

	return 0;
}

int
gpt_write_primary(gpt_t gpt)
{
	return gpt_write_crc(gpt, gpt->gpt, gpt->tbl);
}


int
gpt_write_backup(gpt_t gpt)
{
	return gpt_write_crc(gpt, gpt->tpg, gpt->lbt);
}

void
gpt_create_pmbr_part(struct mbr_part *part, off_t last, int active)
{
	part->part_flag = active ? 0x80 : 0;
	part->part_shd = 0x00;
	part->part_ssect = 0x02;
	part->part_scyl = 0x00;
	part->part_typ = MBR_PTYPE_PMBR;
	part->part_ehd = 0xfe;
	part->part_esect = 0xff;
	part->part_ecyl = 0xff;
	part->part_start_lo = htole16(1);
	if (last > 0xffffffff) {
		part->part_size_lo = htole16(0xffff);
		part->part_size_hi = htole16(0xffff);
	} else {
		part->part_size_lo = htole16((uint16_t)last);
		part->part_size_hi = htole16((uint16_t)(last >> 16));
	}
}

struct gpt_ent *
gpt_ent(map_t map, map_t tbl, unsigned int i)
{
	struct gpt_hdr *hdr = map->map_data;
	return (void *)((char *)tbl->map_data + i * le32toh(hdr->hdr_entsz));
}

struct gpt_ent *
gpt_ent_primary(gpt_t gpt, unsigned int i)
{
	return gpt_ent(gpt->gpt, gpt->tbl, i);
}

struct gpt_ent *
gpt_ent_backup(gpt_t gpt, unsigned int i)
{
	return gpt_ent(gpt->tpg, gpt->lbt, i);
}

int
gpt_usage(const char *prefix, const struct gpt_cmd *cmd)
{
	const char **a = cmd->help;
	size_t hlen = cmd->hlen;
	size_t i;

	if (prefix == NULL) {
		const char *pname = getprogname();
		const char *d1, *d2, *d = " <device>";
		int len = (int)strlen(pname);
		if (strcmp(pname, "gpt") == 0) {
			d1 = "";
			d2 = d;
		} else {
			d2 = "";
			d1 = d;
		}
		fprintf(stderr, "Usage: %s%s %s %s%s\n", pname, 
		    d1, cmd->name, a[0], d2);
		for (i = 1; i < hlen; i++) {
			fprintf(stderr,
			    "       %*s%s %s %s%s\n", len, "",
			    d1, cmd->name, a[i], d2);
		}
	} else {
		for (i = 0; i < hlen; i++)
		    fprintf(stderr, "%s%s %s\n", prefix, cmd->name, a[i]);
	}
	return -1;
}

off_t
gpt_last(gpt_t gpt)
{
	return gpt->mediasz / gpt->secsz - 1LL;
}

off_t
gpt_create(gpt_t gpt, off_t last, u_int parts, int primary_only)
{
	off_t blocks;
	map_t map;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;
	void *p;

	if (map_find(gpt, MAP_TYPE_PRI_GPT_HDR) != NULL ||
	    map_find(gpt, MAP_TYPE_SEC_GPT_HDR) != NULL) {
		gpt_warnx(gpt, "Device already contains a GPT, "
		    "destroy it first");
		return -1;
	}

	/* Get the amount of free space after the MBR */
	blocks = map_free(gpt, 1LL, 0LL);
	if (blocks == 0LL) {
		gpt_warnx(gpt, "No room for the GPT header");
		return -1;
	}

	/* Don't create more than parts entries. */
	if ((uint64_t)(blocks - 1) * gpt->secsz >
	    parts * sizeof(struct gpt_ent)) {
		blocks = (off_t)((parts * sizeof(struct gpt_ent)) / gpt->secsz);
		if ((parts * sizeof(struct gpt_ent)) % gpt->secsz)
			blocks++;
		blocks++;		/* Don't forget the header itself */
	}

	/* Never cross the median of the device. */
	if ((blocks + 1LL) > ((last + 1LL) >> 1))
		blocks = ((last + 1LL) >> 1) - 1LL;

	/*
	 * Get the amount of free space at the end of the device and
	 * calculate the size for the GPT structures.
	 */
	map = map_last(gpt);
	if (map->map_type != MAP_TYPE_UNUSED) {
		gpt_warnx(gpt, "No room for the backup header");
		return -1;
	}

	if (map->map_size < blocks)
		blocks = map->map_size;
	if (blocks == 1LL) {
		gpt_warnx(gpt, "No room for the GPT table");
		return -1;
	}

	blocks--;		/* Number of blocks in the GPT table. */

	if (gpt_add_hdr(gpt, MAP_TYPE_PRI_GPT_HDR, 1) == -1)
		return -1;

	if ((p = calloc((size_t)blocks, gpt->secsz)) == NULL) {
		gpt_warnx(gpt, "Can't allocate the primary GPT table");
		return -1;
	}
	if ((gpt->tbl = map_add(gpt, 2LL, blocks,
	    MAP_TYPE_PRI_GPT_TBL, p, 1)) == NULL) {
		free(p);
		gpt_warnx(gpt, "Can't add the primary GPT table");
		return -1;
	}

	hdr = gpt->gpt->map_data;
	memcpy(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig));

	/*
	 * XXX struct gpt_hdr is not a multiple of 8 bytes in size and thus
	 * contains padding we must not include in the size.
	 */
	hdr->hdr_revision = htole32(GPT_HDR_REVISION);
	hdr->hdr_size = htole32(GPT_HDR_SIZE);
	hdr->hdr_lba_self = htole64((uint64_t)gpt->gpt->map_start);
	hdr->hdr_lba_alt = htole64((uint64_t)last);
	hdr->hdr_lba_start = htole64((uint64_t)(gpt->tbl->map_start + blocks));
	hdr->hdr_lba_end = htole64((uint64_t)(last - blocks - 1LL));
	if (gpt_uuid_generate(gpt, hdr->hdr_guid) == -1)
		return -1;
	hdr->hdr_lba_table = htole64((uint64_t)(gpt->tbl->map_start));
	hdr->hdr_entries = htole32((uint32_t)(((uint64_t)blocks * gpt->secsz) /
	    sizeof(struct gpt_ent)));
	if (le32toh(hdr->hdr_entries) > parts)
		hdr->hdr_entries = htole32(parts);
	hdr->hdr_entsz = htole32(sizeof(struct gpt_ent));

	ent = gpt->tbl->map_data;
	for (i = 0; i < le32toh(hdr->hdr_entries); i++) {
		if (gpt_uuid_generate(gpt, ent[i].ent_guid) == -1)
			return -1;
	}

	/*
	 * Create backup GPT if the user didn't suppress it.
	 */
	if (primary_only)
		return last;

	if (gpt_add_hdr(gpt, MAP_TYPE_SEC_GPT_HDR, last) == -1)
		return -1;

	if ((gpt->lbt = map_add(gpt, last - blocks, blocks,
	    MAP_TYPE_SEC_GPT_TBL, gpt->tbl->map_data, 0)) == NULL) {
		gpt_warnx(gpt, "Can't add the secondary GPT table");
		return -1;
	}

	memcpy(gpt->tpg->map_data, gpt->gpt->map_data, gpt->secsz);

	hdr = gpt->tpg->map_data;
	hdr->hdr_lba_self = htole64((uint64_t)gpt->tpg->map_start);
	hdr->hdr_lba_alt = htole64((uint64_t)gpt->gpt->map_start);
	hdr->hdr_lba_table = htole64((uint64_t)gpt->lbt->map_start);
	return last;
}

static int
gpt_size_get(gpt_t gpt, off_t *size)
{
	off_t sectors;
	int64_t human_num;
	char *p;

	if (*size > 0)
		return -1;
	sectors = strtoll(optarg, &p, 10);
	if (sectors < 1)
		return -1;
	if (*p == '\0' || ((*p == 's' || *p == 'S') && p[1] == '\0')) {
		*size = sectors * gpt->secsz;
		return 0;
	}
	if ((*p == 'b' || *p == 'B') && p[1] == '\0') {
		*size = sectors;
		return 0;
	}
	if (dehumanize_number(optarg, &human_num) < 0)
		return -1;
	*size = human_num;
	return 0;
}

int
gpt_human_get(gpt_t gpt, off_t *human)
{
	int64_t human_num;

	if (*human > 0) {
		gpt_warn(gpt, "Already set to %jd new `%s'", (intmax_t)*human,
		    optarg);
		return -1;
	}
	if (dehumanize_number(optarg, &human_num) < 0) {
		gpt_warn(gpt, "Bad number `%s'", optarg);
		return -1;
	}
	*human = human_num;
	if (*human < 1) {
		gpt_warn(gpt, "Number `%s' < 1", optarg);
		return -1;
	}
	return 0;
}

int
gpt_add_find(gpt_t gpt, struct gpt_find *find, int ch) 
{
	switch (ch) {
	case 'a':
		if (find->all > 0) {
			gpt_warn(gpt, "-a is already set");
			return -1;
		}
		find->all = 1;
		break;
	case 'b':
		if (gpt_human_get(gpt, &find->block) == -1)
			return -1;
		break;
	case 'i':
		if (gpt_uint_get(gpt, &find->entry) == -1)
			return -1;
		break;
	case 'L':
		if (gpt_name_get(gpt, &find->label) == -1)
			return -1;
		break;
	case 's':
		if (gpt_size_get(gpt, &find->size) == -1)
			return -1;
		break;
	case 't':
		if (!gpt_uuid_is_nil(find->type))
			return -1;
		if (gpt_uuid_parse(optarg, find->type) != 0)
			return -1;
		break;
	default:
		gpt_warn(gpt, "Unknown find option `%c'", ch);
		return -1;
	}
	return 0;
}

int
gpt_change_ent(gpt_t gpt, const struct gpt_find *find,
    void (*cfn)(struct gpt_ent *, void *, int), void *v)
{
	map_t m;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];

	if (!find->all ^
	    (find->block > 0 || find->entry > 0 || find->label != NULL
	    || find->size > 0 || !gpt_uuid_is_nil(find->type)))
		return -1;

	if ((hdr = gpt_hdr(gpt)) == NULL)
		return -1;

	/* Relabel all matching entries in the map. */
	for (m = map_first(gpt); m != NULL; m = m->map_next) {
		if (m->map_type != MAP_TYPE_GPT_PART || m->map_index < 1)
			continue;
		if (find->entry > 0 && find->entry != m->map_index)
			continue;
		if (find->block > 0 && find->block != m->map_start)
			continue;
		if (find->size > 0 && find->size != m->map_size)
			continue;

		i = m->map_index - 1;

		ent = gpt_ent_primary(gpt, i);
		if (find->label != NULL) {
			utf16_to_utf8(ent->ent_name,
			    __arraycount(ent->ent_name),
			    utfbuf, __arraycount(utfbuf));
			if (strcmp((char *)find->label, (char *)utfbuf) != 0)
				continue;
		}

		if (!gpt_uuid_is_nil(find->type) &&
		    !gpt_uuid_equal(find->type, ent->ent_type))
			continue;

		/* Change the primary entry. */
		(*cfn)(ent, v, 0);

		if (gpt_write_primary(gpt) == -1)
			return -1;

		ent = gpt_ent_backup(gpt, i);
		/* Change the secondary entry. */
		(*cfn)(ent, v, 1);

		if (gpt_write_backup(gpt) == -1)
			return -1;

		gpt_msg(gpt, "Partition %d %s", m->map_index, find->msg);
	}
	return 0;
}

int
gpt_change_hdr(gpt_t gpt, const struct gpt_find *find,
    void (*cfn)(struct gpt_hdr *, void *, int), void *v)
{
	struct gpt_hdr *hdr;

	if ((hdr = gpt_hdr(gpt)) == NULL)
		return -1;

	/* Change the primary header. */
	(*cfn)(hdr, v, 0);

	if (gpt_write_primary(gpt) == -1)
		return -1;

	hdr = gpt->tpg->map_data;
	/* Change the secondary header. */
	(*cfn)(hdr, v, 1);

	if (gpt_write_backup(gpt) == -1)
		return -1;

	gpt_msg(gpt, "Header %s", find->msg);

	return 0;
}

int
gpt_add_ais(gpt_t gpt, off_t *alignment, u_int *entry, off_t *size, int ch)
{
	switch (ch) {
	case 'a':
		if (gpt_human_get(gpt, alignment) == -1)
			return -1;
		return 0;
	case 'i':
		if (gpt_uint_get(gpt, entry) == -1)
			return -1;
		return 0;
	case 's':
		if (gpt_size_get(gpt, size) == -1)
			return -1;
		return 0;
	default:
		gpt_warn(gpt, "Unknown alignment/index/size option `%c'", ch);
		return -1;
	}
}

off_t
gpt_check_ais(gpt_t gpt, off_t alignment, u_int entry, off_t size)
{
	if (entry == 0) {
		gpt_warnx(gpt, "Entry not specified");
		return -1;
	}
	if (alignment % gpt->secsz != 0) {
		gpt_warnx(gpt, "Alignment (%#jx) must be a multiple of "
		    "sector size (%#x)", (uintmax_t)alignment, gpt->secsz);
		return -1;
	}

	if (size % gpt->secsz != 0) {
		gpt_warnx(gpt, "Size (%#jx) must be a multiple of "
		    "sector size (%#x)", (uintmax_t)size, gpt->secsz);
		return -1;
	}
	if (size > 0)
		return size / gpt->secsz;
	return 0;
}

static const struct nvd {
	const char *name;
	uint64_t mask;
	const char *description;
} gpt_attr[] = {
	{
		"biosboot",
		GPT_ENT_ATTR_LEGACY_BIOS_BOOTABLE,
		"Legacy BIOS boot partition",
	},
	{
		"bootme",
		GPT_ENT_ATTR_BOOTME,
		"Bootable partition",
	},
	{
		"bootfailed",
		GPT_ENT_ATTR_BOOTFAILED,
		"Partition that marked bootonce failed to boot",
	},
	{
		"bootonce",
		GPT_ENT_ATTR_BOOTONCE,
		"Attempt to boot this partition only once",
	},
	{
		"noblockio",
		GPT_ENT_ATTR_NO_BLOCK_IO_PROTOCOL,
		"UEFI won't recognize file system for block I/O",
	},
	{
		"required",
		GPT_ENT_ATTR_REQUIRED_PARTITION,
		"Partition required for platform to function",
	},
};

int
gpt_attr_get(gpt_t gpt, uint64_t *attributes)
{
	size_t i;
	int rv = 0;
	char *ptr;

	*attributes = 0;

	for (ptr = strtok(optarg, ","); ptr; ptr = strtok(NULL, ",")) {
		for (i = 0; i < __arraycount(gpt_attr); i++)
			if (strcmp(gpt_attr[i].name, ptr) == 0)
				break;
		if (i == __arraycount(gpt_attr)) {
			gpt_warnx(gpt, "Unrecognized attribute `%s'", ptr);
			rv = -1;
		} else
			*attributes |= gpt_attr[i].mask;
	}
	return rv;
}

void
gpt_attr_help(const char *prefix)
{
	size_t i;

	for (i = 0; i < __arraycount(gpt_attr); i++)
		printf("%s%10.10s\t%s\n", prefix, gpt_attr[i].name,
		    gpt_attr[i].description);
}

const char *
gpt_attr_list(char *buf, size_t len, uint64_t attributes)
{
	size_t i;
	strlcpy(buf, "", len);	

	for (i = 0; i < __arraycount(gpt_attr); i++)
		if (attributes & gpt_attr[i].mask) {
			strlcat(buf, buf[0] ? ", " : "", len); 
			strlcat(buf, gpt_attr[i].name, len);
		}
	return buf;
}

int
gpt_attr_update(gpt_t gpt, u_int entry, uint64_t set, uint64_t clr)
{
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;
	
	if (entry == 0 || (set == 0 && clr == 0)) {
		gpt_warnx(gpt, "Nothing to set");
		return -1;
	}

	if ((hdr = gpt_hdr(gpt)) == NULL)
		return -1;

	if (entry > le32toh(hdr->hdr_entries)) {
		gpt_warnx(gpt, "Index %u out of range (%u max)",
		    entry, le32toh(hdr->hdr_entries));
		return -1;
	}

	i = entry - 1;
	ent = gpt_ent_primary(gpt, i);
	if (gpt_uuid_is_nil(ent->ent_type)) {
		gpt_warnx(gpt, "Entry at index %u is unused", entry);
		return -1;
	}

	ent->ent_attr &= ~clr;
	ent->ent_attr |= set;

	if (gpt_write_primary(gpt) == -1)
		return -1;

	ent = gpt_ent_backup(gpt, i);
	ent->ent_attr &= ~clr;
	ent->ent_attr |= set;

	if (gpt_write_backup(gpt) == -1)
		return -1;
	gpt_msg(gpt, "Partition %d attributes updated", entry);
	return 0;
}

int
gpt_uint_get(gpt_t gpt, u_int *entry)
{
	char *p;
	if (*entry > 0)
		return -1;
	*entry = (u_int)strtoul(optarg, &p, 10);
	if (*p != 0 || *entry < 1) {
		gpt_warn(gpt, "Bad number `%s'", optarg);
		return -1;
	}
	return 0;
}
int
gpt_uuid_get(gpt_t gpt, gpt_uuid_t *uuid)
{
	if (!gpt_uuid_is_nil(*uuid))
		return -1;
	if (gpt_uuid_parse(optarg, *uuid) != 0) {
		gpt_warnx(gpt, "Can't parse uuid/type `%s'", optarg);
		return -1;
	}
	return 0;
}

int
gpt_name_get(gpt_t gpt, void *v)
{
	char **name = v;
	if (*name != NULL)
		return -1;
	*name = strdup(optarg);
	if (*name == NULL) {
		gpt_warn(gpt, "Can't copy string");
		return -1;
	}
	return 0;
}

void
gpt_show_num(const char *prompt, uintmax_t num)
{
#ifdef HN_AUTOSCALE
	char human_num[5];
	if (humanize_number(human_num, 5, (int64_t)num ,
	    "", HN_AUTOSCALE, HN_NOSPACE|HN_B) < 0)
		human_num[0] = '\0';
#endif
	printf("%s: %ju", prompt, num);
#ifdef HN_AUTOSCALE
	if (human_num[0] != '\0')
		printf(" (%s)", human_num);
#endif
	printf("\n");
}

int
gpt_add_hdr(gpt_t gpt, int type, off_t loc)
{
	void *p;
	map_t *t;
	const char *msg;

	switch (type) {
	case MAP_TYPE_PRI_GPT_HDR:
		t = &gpt->gpt;
		msg = "primary";
		break;
	case MAP_TYPE_SEC_GPT_HDR:
		t = &gpt->tpg;
		msg = "secondary";
		break;
	default:
		gpt_warnx(gpt, "Unknown GPT header type %d", type);
		return -1;
	}

	if ((p = calloc(1, gpt->secsz)) == NULL) {
		gpt_warn(gpt, "Error allocating %s GPT header", msg);
		return -1;
	}

	*t = map_add(gpt, loc, 1LL, type, p, 1);
	if (*t == NULL) {
		gpt_warn(gpt, "Error adding %s GPT header", msg);
		free(p);
		return -1;
	}
	return 0;
}
