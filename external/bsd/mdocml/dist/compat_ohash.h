#ifndef OHASH_H
#define OHASH_H
/* $OpenBSD: ohash.h,v 1.9 2006/01/16 15:52:25 espie Exp $ */
/* ex:ts=8 sw=4: 
 */

/* Copyright (c) 1999, 2004 Marc Espie <espie@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Open hashing support. 
 * Open hashing was chosen because it is much lighter than other hash
 * techniques, and more efficient in most cases.
 */

struct ohash_info {
	ptrdiff_t key_offset;
	void *data;	/* user data */
	void *(*halloc)(size_t, void *);
	void (*hfree)(void *, size_t, void *);
	void *(*alloc)(size_t, void *);
};

struct _ohash_record;

struct ohash {
	struct _ohash_record 	*t;
	struct ohash_info 	info;
	unsigned int 		size;
	unsigned int 		total;
	unsigned int 		deleted;
};

/* For this to be tweakable, we use small primitives, and leave part of the
 * logic to the client application.  e.g., hashing is left to the client
 * application.  We also provide a simple table entry lookup that yields
 * a hashing table index (opaque) to be used in find/insert/remove.
 * The keys are stored at a known position in the client data.
 */
__BEGIN_DECLS
void ohash_init(struct ohash *, unsigned, struct ohash_info *);
void ohash_delete(struct ohash *);

unsigned int ohash_lookup_interval(struct ohash *, const char *,
	    const char *, uint32_t);
unsigned int ohash_lookup_memory(struct ohash *, const char *,
	    size_t, uint32_t)
		__attribute__ ((__bounded__(__string__,2,3)));
void *ohash_find(struct ohash *, unsigned int);
void *ohash_remove(struct ohash *, unsigned int);
void *ohash_insert(struct ohash *, unsigned int, void *);
void *ohash_first(struct ohash *, unsigned int *);
void *ohash_next(struct ohash *, unsigned int *);
unsigned int ohash_entries(struct ohash *);

void *ohash_create_entry(struct ohash_info *, const char *, const char **);
uint32_t ohash_interval(const char *, const char **);

unsigned int ohash_qlookupi(struct ohash *, const char *, const char **);
unsigned int ohash_qlookup(struct ohash *, const char *);
__END_DECLS
#endif
