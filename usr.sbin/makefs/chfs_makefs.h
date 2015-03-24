/*-
 * Copyright (c) 2012 Department of Software Engineering,
 *		      University of Szeged, Hungary
 * Copyright (c) 2012 Tamas Toth <ttoth@inf.u-szeged.hu>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by the Department of Software Engineering, University of Szeged, Hungary
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CHFS_MAKEFS_H
#define _CHFS_MAKEFS_H

#define TYPE_NOR 0
#define TYPE_NAND 1

#define DEFAULT_PAGESIZE 2048
#define DEFAULT_ERASEBLOCK 131072
#define DEFAULT_MEDIATYPE TYPE_NAND

typedef struct {
	int pagesize;		/* page size */
	int eraseblock;		/* eraseblock size */
	int mediatype;		/* type of the media, 0 (nor) or 1 (nand) */
} chfs_opt_t;

#endif
