/*	$NetBSD: debug.h,v 1.1 2011/11/24 15:51:32 ahoka Exp $	*/

/*-
 * Copyright (c) 2010 Department of Software Engineering,
 *		      University of Szeged, Hungary
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

/*
 * XipFFS -- Xip Flash File System
 *
 * Copyright (C) 2009  Ferenc Havasi <havasi@inf.u-szeged.hu>,
 *                     Zoltan Sogor <weth@inf.u-szeged.hu>,
 *                     ...
 *                     University of Szeged, Hungary
 *
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#ifndef __CHFS_DEBUG_H__
#define __CHFS_DEBUG_H__

#define CHFS_ERROR_PREFIX	"[CHFS ERROR]"
#define CHFS_WARNING_PREFIX	"[CHFS WARNING]"
#define CHFS_NOTICE_PREFIX	"[CHFS NOTICE]"
#define CHFS_DBG_PREFIX	"[CHFS DBG]"
#define CHFS_DBG2_PREFIX	"[CHFS DBG2]"
#define CHFS_DBG_EBH_PREFIX	"[CHFS DBG EBH]"
#define CHFS_DBG_GC_PREFIX	"[CHFS_GC DBG]"

#define unlikely(x) __builtin_expect ((x), 0)



#define debug_msg(pref, fmt, ...)                                              \
	do {                                                                   \
		printf(pref                                                    \
			" %s: " fmt, __FUNCTION__ , ##__VA_ARGS__);            \
	} while(0)

#define chfs_assert(expr) do {                                               \
	if (unlikely(!(expr))) {                                               \
		printf("CHFS assert failed in %s at %u\n", \
			__func__, __LINE__);                     \
		/*dump_stack();*/                                                  \
	}                                                                      \
} while (0)

#ifdef DBG_MSG
	#define chfs_err(fmt, ...) debug_msg(CHFS_ERROR_PREFIX, fmt, ##__VA_ARGS__)
	#define chfs_warn(fmt, ...) debug_msg(CHFS_WARNING_PREFIX, fmt, ##__VA_ARGS__)
	#define chfs_noti(fmt, ...) debug_msg(CHFS_NOTICE_PREFIX, fmt, ##__VA_ARGS__)
	#define dbg(fmt, ...) debug_msg(CHFS_DBG_PREFIX, fmt, ##__VA_ARGS__)
	#define dbg2(fmt, ...) debug_msg(CHFS_DBG2_PREFIX(fmt, ##__VA_ARGS__)
	#define dbg_ebh(fmt, ...) debug_msg(CHFS_DBG_EBH_PREFIX, fmt, ##__VA_ARGS__)
#else
	#define chfs_err(fmt, ...) debug_msg(CHFS_ERROR_PREFIX, fmt, ##__VA_ARGS__)
	#define chfs_warn(fmt, ...) debug_msg(CHFS_WARNING_PREFIX, fmt, ##__VA_ARGS__)
	#define chfs_noti(fmt, ...) debug_msg(CHFS_NOTICE_PREFIX, fmt, ##__VA_ARGS__)
	#define dbg(fmt, ...)
	#define dbg2(fmt, ...)
	#define dbg_ebh(fmt, ...)
#endif

#ifdef DBG_MSG_GC
	#define dbg_gc(fmt, ...) debug_msg(CHFS_DBG_GC_PREFIX, fmt, ##__VA_ARGS__)
#else
	#define dbg_gc(fmt, ...)
#endif

#endif /* __CHFS_DEBUG_H__ */
