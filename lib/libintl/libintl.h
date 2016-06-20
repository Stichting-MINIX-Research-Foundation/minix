/*	$NetBSD: libintl.h,v 1.8 2015/06/08 15:04:20 christos Exp $	*/

/*-
 * Copyright (c) 2000 Citrus Project,
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

#ifndef _LIBINTL_H_
#define _LIBINTL_H_

#include <sys/cdefs.h>

#ifndef _LIBGETTEXT_H
/*
 * Avoid defining these if the GNU gettext compatibility header includes
 * us, since it re-defines those unconditionally and creates inline functions
 * for some of them. This is horrible.
 */
#define pgettext_expr(msgctxt, msgid) pgettext((msgctxt), (msgid))
#define dpgettext_expr(domainname, msgctxt, msgid) \
    dpgettext((domainname), (msgctxt), (msgid))
#define dcpgettext_expr(domainname, msgctxt, msgid, category) \
    dcpgettext((domainname), (msgctxt), (msgid), (category))
#define npgettext_expr(msgctxt, msgid1, msgid2, n) \
    npgettext((msgctxt), (msgid1), (msgid2), (n))
#define dnpgettext_expr(domainname, msgctxt, msgid1, n) \
    dnpgettext((domainname), (msgctxt), (msgid1), (msgid2), (n))
#define dcnpgettext_expr(domainname, msgctxt, msgid1, msgid2, n, category) \
    dcnpgettext((domainname), (msgctxt), (msgid1), (msgid2), (n), (category))
#endif

__BEGIN_DECLS
char *gettext(const char *) __format_arg(1);
char *dgettext(const char *, const char *) __format_arg(2);
char *dcgettext(const char *, const char *, int) __format_arg(2);
char *ngettext(const char *, const char *, unsigned long int)
	       __format_arg(1) __format_arg(2);
char *dngettext(const char *, const char *, const char *, unsigned long int)
		__format_arg(2) __format_arg(3);
char *dcngettext(const char *, const char *, const char *, unsigned long int,
		 int) __format_arg(2) __format_arg(3);
const char *pgettext(const char *, const char *) __format_arg(2);
const char *dpgettext(const char *, const char *, const char *)
		      __format_arg(3);
const char *dcpgettext(const char *, const char *, const char *, int)
		       __format_arg(3);
const char *npgettext(const char *, const char *, const char *,
		      unsigned long int) __format_arg(2) __format_arg(3);
const char *dnpgettext(const char *, const char *, const char *,
		       const char *, unsigned long int) __format_arg(3)
		       __format_arg(4);
const char *dcnpgettext(const char *, const char *, const char *,
			const char *, unsigned long int, int) __format_arg(3)
			__format_arg(4);

char *textdomain(const char *);
char *bindtextdomain(const char *, const char *);
char *bind_textdomain_codeset(const char *, const char *);

__END_DECLS

#endif /* _LIBINTL_H_ */
