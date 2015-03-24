/*	$NetBSD: v7fs_file.h,v 1.2 2011/07/16 12:35:40 uch Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#ifndef _V7FS_FILE_H_
#define	_V7FS_FILE_H_

struct v7fs_lookup_arg {
	const char *name;
	char *buf;
	v7fs_ino_t inode_number;
	struct v7fs_dirent *replace;
};

__BEGIN_DECLS
/* core */
int v7fs_file_lookup_by_name(struct v7fs_self *, struct v7fs_inode *,
    const char*, v7fs_ino_t *);
int v7fs_file_allocate(struct v7fs_self *, struct v7fs_inode *, const char *,
    struct v7fs_fileattr *, v7fs_ino_t *);
int v7fs_file_deallocate(struct v7fs_self *, struct v7fs_inode *, const char *);
int v7fs_directory_add_entry(struct v7fs_self *,struct v7fs_inode *, v7fs_ino_t,
    const char *);
int v7fs_directory_remove_entry(struct v7fs_self *,struct v7fs_inode *,
    const char *);

/* util */
int v7fs_file_rename(struct v7fs_self *, struct v7fs_inode *, const char *,
    struct v7fs_inode *, const char *);
int v7fs_directory_replace_entry(struct v7fs_self *, struct v7fs_inode *,
    const char *, v7fs_ino_t);
int v7fs_file_link(struct v7fs_self *, struct v7fs_inode *, struct v7fs_inode *,
    const char *);
bool v7fs_file_lookup_by_number(struct v7fs_self *, struct v7fs_inode *,
    v7fs_ino_t, char *);
int v7fs_file_symlink(struct v7fs_self *, struct v7fs_inode *, const char *);
__END_DECLS
#endif /*!_V7FS_INODE_H_ */
