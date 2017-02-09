/*	$NetBSD: dkwedge_gpt.c,v 1.15 2015/08/23 18:40:15 jakllsch Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * EFI GUID Partition Table support for disk wedges
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: dkwedge_gpt.c,v 1.15 2015/08/23 18:40:15 jakllsch Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/disk.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

#include <sys/disklabel_gpt.h>
#include <sys/uuid.h>

/* UTF-8 encoding stuff */
#include <fs/unicode.h>

/*
 * GUID to dkw_ptype mapping information.
 *
 * GPT_ENT_TYPE_MS_BASIC_DATA is not suited to mapping.  Aside from being
 * used for multiple Microsoft file systems, Linux uses it for its own
 * set of native file systems.  Treating this GUID as unknown seems best.
 */

static const struct {
	struct uuid ptype_guid;
	const char *ptype_str;
} gpt_ptype_guid_to_str_tab[] = {
	{ GPT_ENT_TYPE_EFI,			DKW_PTYPE_FAT },
	{ GPT_ENT_TYPE_NETBSD_SWAP,		DKW_PTYPE_SWAP },
	{ GPT_ENT_TYPE_FREEBSD_SWAP,		DKW_PTYPE_SWAP },
	{ GPT_ENT_TYPE_NETBSD_FFS,		DKW_PTYPE_FFS },
	{ GPT_ENT_TYPE_FREEBSD_UFS,		DKW_PTYPE_FFS },
	{ GPT_ENT_TYPE_APPLE_UFS,		DKW_PTYPE_FFS },
	{ GPT_ENT_TYPE_NETBSD_LFS,		DKW_PTYPE_LFS },
	{ GPT_ENT_TYPE_NETBSD_RAIDFRAME,	DKW_PTYPE_RAIDFRAME },
	{ GPT_ENT_TYPE_NETBSD_CCD,		DKW_PTYPE_CCD },
	{ GPT_ENT_TYPE_NETBSD_CGD,		DKW_PTYPE_CGD },
	{ GPT_ENT_TYPE_APPLE_HFS,		DKW_PTYPE_APPLEHFS },
};

static const char *
gpt_ptype_guid_to_str(const struct uuid *guid)
{
	int i;

	for (i = 0; i < __arraycount(gpt_ptype_guid_to_str_tab); i++) {
		if (memcmp(&gpt_ptype_guid_to_str_tab[i].ptype_guid,
			   guid, sizeof(*guid)) == 0)
			return (gpt_ptype_guid_to_str_tab[i].ptype_str);
	}

	return (DKW_PTYPE_UNKNOWN);
}

static int
gpt_verify_header_crc(struct gpt_hdr *hdr)
{
	uint32_t crc;
	int rv;

	crc = hdr->hdr_crc_self;
	hdr->hdr_crc_self = 0;
	rv = le32toh(crc) == crc32(0, (void *)hdr, le32toh(hdr->hdr_size));
	hdr->hdr_crc_self = crc;

	return (rv);
}

static int
dkwedge_discover_gpt(struct disk *pdk, struct vnode *vp)
{
	static const struct uuid ent_type_unused = GPT_ENT_TYPE_UNUSED;
	static const char gpt_hdr_sig[] = GPT_HDR_SIG;
	struct dkwedge_info dkw;
	void *buf;
	uint32_t secsize;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	uint32_t entries, entsz;
	daddr_t lba_start, lba_end, lba_table;
	uint32_t gpe_crc;
	int error;
	u_int i;
	size_t r, n;
	uint8_t *c;

	secsize = DEV_BSIZE << pdk->dk_blkshift;
	buf = malloc(secsize, M_DEVBUF, M_WAITOK);

	/*
	 * Note: We don't bother with a Legacy or Protective MBR
	 * here.  If a GPT is found, then the search stops, and
	 * the GPT is authoritative.
	 */

	/* Read in the GPT Header. */
	error = dkwedge_read(pdk, vp, GPT_HDR_BLKNO << pdk->dk_blkshift, buf, secsize);
	if (error)
		goto out;
	hdr = buf;

	/* Validate it. */
	if (memcmp(gpt_hdr_sig, hdr->hdr_sig, sizeof(hdr->hdr_sig)) != 0) {
		/* XXX Should check at end-of-disk. */
		error = ESRCH;
		goto out;
	}
	if (hdr->hdr_revision != htole32(GPT_HDR_REVISION)) {
		/* XXX Should check at end-of-disk. */
		error = ESRCH;
		goto out;
	}
	if (le32toh(hdr->hdr_size) > secsize) {
		/* XXX Should check at end-of-disk. */
		error = ESRCH;
		goto out;
	}
	if (gpt_verify_header_crc(hdr) == 0) {
		/* XXX Should check at end-of-disk. */
		error = ESRCH;
		goto out;
	}

	/* XXX Now that we found it, should we validate the backup? */

	{
		struct uuid disk_guid;
		char guid_str[UUID_STR_LEN];
		uuid_dec_le(hdr->hdr_guid, &disk_guid);
		uuid_snprintf(guid_str, sizeof(guid_str), &disk_guid);
		aprint_verbose("%s: GPT GUID: %s\n", pdk->dk_name, guid_str);
	}

	entries = le32toh(hdr->hdr_entries);
	entsz = roundup(le32toh(hdr->hdr_entsz), 8);
	if (entsz > roundup(sizeof(struct gpt_ent), 8)) {
		aprint_error("%s: bogus GPT entry size: %u\n",
		    pdk->dk_name, le32toh(hdr->hdr_entsz));
		error = EINVAL;
		goto out;
	}
	gpe_crc = le32toh(hdr->hdr_crc_table);

	/* XXX Clamp entries at 512 for now. */
	if (entries > 512) {
		aprint_error("%s: WARNING: clamping number of GPT entries to "
		    "512 (was %u)\n", pdk->dk_name, entries);
		entries = 512;
	}

	lba_start = le64toh(hdr->hdr_lba_start);
	lba_end = le64toh(hdr->hdr_lba_end);
	lba_table = le64toh(hdr->hdr_lba_table);
	if (lba_start < 0 || lba_end < 0 || lba_table < 0) {
		aprint_error("%s: GPT block numbers out of range\n",
		    pdk->dk_name);
		error = EINVAL;
		goto out;
	}

	free(buf, M_DEVBUF);
	buf = malloc(roundup(entries * entsz, secsize), M_DEVBUF, M_WAITOK);
	error = dkwedge_read(pdk, vp, lba_table << pdk->dk_blkshift, buf,
			     roundup(entries * entsz, secsize));
	if (error) {
		/* XXX Should check alternate location. */
		aprint_error("%s: unable to read GPT partition array, "
		    "error = %d\n", pdk->dk_name, error);
		goto out;
	}

	if (crc32(0, buf, entries * entsz) != gpe_crc) {
		/* XXX Should check alternate location. */
		aprint_error("%s: bad GPT partition array CRC\n",
		    pdk->dk_name);
		error = EINVAL;
		goto out;
	}

	/*
	 * Walk the partitions, adding a wedge for each type we know about.
	 */
	for (i = 0; i < entries; i++) {
		struct uuid ptype_guid, ent_guid;
		const char *ptype;
		int j;
		char ptype_guid_str[UUID_STR_LEN], ent_guid_str[UUID_STR_LEN];

		ent = (struct gpt_ent *)((char *)buf + (i * entsz));

		uuid_dec_le(ent->ent_type, &ptype_guid);
		if (memcmp(&ptype_guid, &ent_type_unused,
			   sizeof(ptype_guid)) == 0)
			continue;

		uuid_dec_le(ent->ent_guid, &ent_guid);

		uuid_snprintf(ptype_guid_str, sizeof(ptype_guid_str),
		    &ptype_guid);
		uuid_snprintf(ent_guid_str, sizeof(ent_guid_str),
		    &ent_guid);

		/* figure out the type */
		ptype = gpt_ptype_guid_to_str(&ptype_guid);
		strcpy(dkw.dkw_ptype, ptype);

		strcpy(dkw.dkw_parent, pdk->dk_name);
		dkw.dkw_offset = le64toh(ent->ent_lba_start);
		dkw.dkw_size = le64toh(ent->ent_lba_end) - dkw.dkw_offset + 1;

		/* XXX Make sure it falls within the disk's data area. */

		if (ent->ent_name[0] == 0x0000)
			strcpy(dkw.dkw_wname, ent_guid_str);
		else {
			c = dkw.dkw_wname;
			r = sizeof(dkw.dkw_wname) - 1;
			for (j = 0; ent->ent_name[j] != 0x0000; j++) {
				n = wput_utf8(c, r, le16toh(ent->ent_name[j]));
				if (n == 0)
					break;
				c += n; r -= n;
			}
			*c = '\0';
		}

		/*
		 * Try with the partition name first.  If that fails,
		 * use the GUID string.  If that fails, punt.
		 */
		if ((error = dkwedge_add(&dkw)) == EEXIST &&
		    strcmp(dkw.dkw_wname, ent_guid_str) != 0) {
			strcpy(dkw.dkw_wname, ent_guid_str);
			error = dkwedge_add(&dkw);
			if (!error)
				aprint_error("%s: wedge named '%s' already "
				    "existed, using '%s'\n", pdk->dk_name,
				    dkw.dkw_wname, /* XXX Unicode */
				    ent_guid_str);
		}
		if (error == EEXIST)
			aprint_error("%s: wedge named '%s' already exists, "
			    "manual intervention required\n", pdk->dk_name,
			    dkw.dkw_wname);
		else if (error)
			aprint_error("%s: error %d adding entry %u (%s), "
			    "type %s\n", pdk->dk_name, error, i, ent_guid_str,
			    ptype_guid_str);
	}
	error = 0;

 out:
	free(buf, M_DEVBUF);
	return (error);
}

DKWEDGE_DISCOVERY_METHOD_DECL(GPT, 0, dkwedge_discover_gpt);
