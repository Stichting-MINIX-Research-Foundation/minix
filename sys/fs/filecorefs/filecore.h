/*	$NetBSD: filecore.h,v 1.5 2007/12/25 18:33:43 perry Exp $	*/

/*-
 * Copyright (c) 1994 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	filecore.h		1.0	1998/6/1
 */

/*-
 * Copyright (c) 1998 Andrew McMurry
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 *	filecore.h		1.0	1998/6/1
 */

/*
 * Definitions describing Acorn Filecore file system structure, as well as
 * the functions necessary to access fields of filecore file system
 * structures.
 */
#if !defined(_KERNEL)
#error not supposed to be exposed to userland.
#endif


#define FILECORE_BOOTBLOCK_BLKN	6
#define FILECORE_BOOTBLOCK_SIZE	0x200
#define FILECORE_BB_DISCREC	0x1C0
#define FILECORE_DISCREC_SIZE	60
#define FILECORE_DIR_SIZE	2048
#define FILECORE_DIRENT_SIZE	26
#define FILECORE_MAXDIRENTS	77

#define FILECORE_ROOTINO	0xFFFFFFFF
#define FILECORE_INO_MASK	0x00FFFFFF
#define FILECORE_INO_INDEX	24

#define FILECORE_ATTR_READ	1
#define FILECORE_ATTR_WRITE	2
#define FILECORE_ATTR_DIR	8
#define FILECORE_ATTR_OREAD	32
#define FILECORE_ATTR_OWRITE	64

#if 0
#define FILECORE_DEBUG
#define FILECORE_DEBUG_BR
#endif

struct filecore_disc_record {
	unsigned log2secsize:8;		/* base 2 log of the sector size */
	unsigned secspertrack:8;	/* number of sectors per track */
	unsigned heads:8;		/* number of heads */
	unsigned density:8;		/* 0: harddisc, else floppy density */
	unsigned idlen:8;		/* length of fragment id in bits */
	unsigned log2bpmb:8;		/* base 2 log of the bytes per FAU */
	unsigned skew:8;		/* track to sector skew */
	unsigned bootoption:8;		/* *OPT option */
	unsigned lowsector:8;		/* lowest sector id on a track */
	unsigned nzones:8;		/* number of zones in the map */
	unsigned zone_spare:16;		/* number of non-map bits per zone */
	unsigned root;			/* address of root directory */
	unsigned disc_size;		/* disc size in bytes (low word) */
	unsigned disc_id:16;		/* disc cycle id */
	char	 disc_name[10];		/* disc name */
	unsigned disc_type;		/* disc type */
	unsigned disc_size_2;		/* disc size in bytes (high word) */
	unsigned share_size:8;		/* base 2 log sharing granularity */
	unsigned big_flag:8;		/* 1 if disc > 512Mb */
	char	 reserved[18];
} __packed;

struct filecore_direntry {
	char	 name[10];
	unsigned load:32;
	unsigned exec:32;
	unsigned len:32;
	unsigned addr:24;
	unsigned attr:8;
} __packed;

struct filecore_dirhead {
	unsigned mas_seq:8;
	unsigned chkname:32;
} __packed;

struct filecore_dirtail {
	unsigned lastmark:8;
	unsigned reserved:16;
	unsigned parent1:16;
	unsigned parent2:8;
	char	 title[19];
	char	 name[10];
	unsigned mas_seq:8;
	unsigned chkname:32;
	unsigned checkbyte:8;
} __packed;

#define fcdirhead(dp) ((struct filecore_dirhead *)(dp))
#define fcdirentry(dp,n) (((struct filecore_direntry *)(((char *)(dp))+5))+(n))
#define fcdirtail(dp) ((struct filecore_dirtail *)(((char *)(dp))+2007))
