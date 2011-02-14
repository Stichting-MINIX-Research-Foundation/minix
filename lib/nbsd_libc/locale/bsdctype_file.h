/* $NetBSD: bsdctype_file.h,v 1.1 2010/06/13 04:14:57 tnozaki Exp $ */

/*-
 * Copyright (c)2008 Citrus Project,
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _BSDCTYPE_FILE_H_
#define _BSDCTYPE_FILE_H_

#include "ctype_local.h"

typedef struct {
	char			fbl_id[8];
	uint32_t		fbl_rev;
	uint32_t		fbl_num_chars;
	uint8_t			fbl_ctype_tab  [_CTYPE_CACHE_SIZE];
	int16_t			fbl_tolower_tab[_CTYPE_CACHE_SIZE];
	int16_t			fbl_toupper_tab[_CTYPE_CACHE_SIZE];
} __packed _FileBSDCTypeLocale;

#define _CTYPE_ID		"BSDCTYPE"
#define _CTYPE_REV		2

#endif /*_BSDCTYPE_FILE_H_*/
