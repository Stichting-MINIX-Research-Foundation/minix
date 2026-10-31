/*	$NetBSD: biosdisk.h,v 1.10.6.2 2019/09/17 19:32:00 martin Exp $	*/

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
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

#define BIOSDISK_PART_NAME_LEN 36

struct biosdisk_partition {
	daddr_t offset;
	daddr_t size;
	int     fstype;
#ifndef NO_GPT
	const struct gpt_part {
		const struct uuid *guid;
		const char *name;
	} *guid;
	uint64_t attr;
	char *part_name; /* maximum BIOSDISK_PART_NAME_LEN */
#endif
};

extern struct btinfo_bootdisk bi_disk; 
extern struct btinfo_bootwedge bi_wedge;

int biosdisk_strategy(void *, int, daddr_t, size_t, void *, size_t *);
int biosdisk_open(struct open_file *, ...);
int biosdisk_open_name(struct open_file *, const char *);
int biosdisk_close(struct open_file *);
int biosdisk_ioctl(struct open_file *, u_long, void *);
int biosdisk_findpartition(int, daddr_t, int *, const char **);
int biosdisk_readpartition(int, daddr_t, daddr_t,
     struct biosdisk_partition **, int *);

struct RF_ComponentLabel_s;
int biosdisk_read_raidframe(int, daddr_t, struct RF_ComponentLabel_s *);

#if !defined(NO_GPT)
struct uuid;
bool guid_is_nil(const struct uuid *);
bool guid_is_equal(const struct uuid *, const struct uuid *);
#endif
