/* $NetBSD: efiblock.h,v 1.7 2022/04/24 06:49:38 mlelstv Exp $ */

/*-
 * Copyright (c) 2018 Jared McNeill <jmcneill@invisible.ca>
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/bootblock.h>
#include <sys/disklabel.h>
#include <sys/disklabel_gpt.h>

enum efi_block_part_type {
	EFI_BLOCK_PART_DISKLABEL,
	EFI_BLOCK_PART_GPT,
	EFI_BLOCK_PART_CD9660
};

struct efi_block_part;

struct efi_block_dev {
	uint16_t index;
	EFI_DEVICE_PATH *path;
	EFI_BLOCK_IO *bio;
	EFI_DISK_IO *dio;
	UINT32 media_id;
	TAILQ_HEAD(, efi_block_part) partitions;

	TAILQ_ENTRY(efi_block_dev) entries;
};

struct efi_block_part_disklabel {
	uint32_t secsize;
	struct partition part;
	daddr_t labelsector;
	struct {
		uint16_t type;
		uint16_t checksum;
		char packname[16];
	} label;
};

struct efi_block_part_gpt {
	uint8_t fstype;
	struct gpt_ent ent;
};

struct efi_block_part {
	uint32_t index;
	struct efi_block_dev *bdev;
	enum efi_block_part_type type;
	union {
		struct efi_block_part_disklabel disklabel;
		struct efi_block_part_gpt gpt;
	};
	uint8_t hash[16];

	TAILQ_ENTRY(efi_block_part) entries;
};

void efi_block_probe(void);
void efi_block_show(void);
struct efi_block_part *efi_block_boot_part(void);
EFI_STATUS efi_block_read(struct efi_block_dev *, UINT64, void *, UINTN);

int efi_block_open(struct open_file *, ...);
int efi_block_close(struct open_file *);
int efi_block_ioctl(struct open_file *, u_long, void *);
int efi_block_strategy(void *, int, daddr_t, size_t, void *, size_t *);

void efi_block_set_readahead(bool);
