/*	$NetBSD: cdefs.h,v 1.1.1.3 2014/07/12 11:57:52 spz Exp $	*/
/* cdefs.h

   Standard C definitions... */

/*
 * Copyright (c) 1995 RadioMail Corporation.  All rights reserved.
 * Copyright (c) 2011,2012 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 2004,2009 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 * This software was written for RadioMail Corporation by Ted Lemon
 * under a contract with Vixie Enterprises.   Further modifications have
 * been made for Internet Systems Consortium under a contract
 * with Vixie Laboratories.
 */

#if !defined (__ISC_DHCP_CDEFS_H__)
#define __ISC_DHCP_CDEFS_H__
/* Delete attributes if not gcc or not the right version of gcc. */
#if !defined(__GNUC__) || __GNUC__ < 2 || \
        (__GNUC__ == 2 && __GNUC_MINOR__ < 5) || defined (darwin)
#define __attribute__(x)
#endif

/* The following macro handles the case of unwanted return values.  In
 * GCC one can specify an attribute for a function to generate a warning
 * if the return value of the function is ignored and one can't dispose of
 * the warning by the use of void.  In conjunction with the use of -Werror
 * these warnings prohibit the compilation of the package.  This macro
 * allows us to assign the return value to a variable and then ignore it.
 *
 * __attribute__((unused)) is added for avoiding another warning about set,
 * but unused variable. This is produced by unused-but-set-variable switch
 * that is enabled by default in gcc 4.6.
 */
#if !defined(__GNUC__) || (__GNUC__ < 4)
#define IGNORE_RET(x) (void) x
#else
#define IGNORE_RET(x)			\
	do {				\
                int __attribute__((unused)) ignore_return ;\
                ignore_return = x;                         \
	} while (0)
#endif

/* This macro is defined to avoid unused-but-set-variable warning
 * that is enabled in gcc 4.6 
 */

#define IGNORE_UNUSED(x) { x = x; }

#endif /* __ISC_DHCP_CDEFS_H__ */
