/*	$NetBSD: dlz_list.h,v 1.3 2014/12/10 04:37:55 christos Exp $	*/

/*
 * Copyright (C) 2004, 2006, 2007, 2011-2013  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1997-2002  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DLZ_LIST_H
#define DLZ_LIST_H 1

#define DLZ_LIST(type) struct { type *head, *tail; }
#define DLZ_LIST_INIT(list) \
	do { (list).head = NULL; (list).tail = NULL; } while (/*CONSTCOND*/0)

#define DLZ_LINK(type) struct { type *prev, *next; }
#define DLZ_LINK_INIT(elt, link) \
	do { \
		(elt)->link.prev = (void *)(-1); \
		(elt)->link.next = (void *)(-1); \
	} while (/*CONSTCOND*/0)

#define DLZ_LIST_HEAD(list) ((list).head)
#define DLZ_LIST_TAIL(list) ((list).tail)

#define DLZ_LIST_APPEND(list, elt, link) \
	do { \
		if ((list).tail != NULL) \
			(list).tail->link.next = (elt); \
		else \
			(list).head = (elt); \
		(elt)->link.prev = (list).tail; \
		(elt)->link.next = NULL; \
		(list).tail = (elt); \
	} while (/*CONSTCOND*/0)

#define DLZ_LIST_PREV(elt, link) ((elt)->link.prev)
#define DLZ_LIST_NEXT(elt, link) ((elt)->link.next)

#endif /* DLZ_LIST_H */
