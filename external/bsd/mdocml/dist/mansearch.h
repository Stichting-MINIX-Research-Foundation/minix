/*	Id: mansearch.h,v 1.7 2014/01/05 00:29:54 schwarze Exp  */
/*
 * Copyright (c) 2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013 Ingo Schwarze <schwarze@openbsd.org>
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
#ifndef MANSEARCH_H
#define MANSEARCH_H

#define	MANDOC_DB	 "mandoc.db"

#define	TYPE_An		 0x0000000000000001ULL
#define	TYPE_Ar		 0x0000000000000002ULL
#define	TYPE_At		 0x0000000000000004ULL
#define	TYPE_Bsx	 0x0000000000000008ULL
#define	TYPE_Bx          0x0000000000000010ULL
#define	TYPE_Cd		 0x0000000000000020ULL
#define	TYPE_Cm		 0x0000000000000040ULL
#define	TYPE_Dv		 0x0000000000000080ULL
#define	TYPE_Dx		 0x0000000000000100ULL
#define	TYPE_Em		 0x0000000000000200ULL
#define	TYPE_Er		 0x0000000000000400ULL
#define	TYPE_Ev		 0x0000000000000800ULL
#define	TYPE_Fa		 0x0000000000001000ULL
#define	TYPE_Fl		 0x0000000000002000ULL
#define	TYPE_Fn		 0x0000000000004000ULL
#define	TYPE_Ft		 0x0000000000008000ULL
#define	TYPE_Fx		 0x0000000000010000ULL
#define	TYPE_Ic		 0x0000000000020000ULL
#define	TYPE_In		 0x0000000000040000ULL
#define	TYPE_Lb		 0x0000000000080000ULL
#define	TYPE_Li		 0x0000000000100000ULL
#define	TYPE_Lk		 0x0000000000200000ULL
#define	TYPE_Ms		 0x0000000000400000ULL
#define	TYPE_Mt		 0x0000000000800000ULL
#define	TYPE_Nd		 0x0000000001000000ULL
#define	TYPE_Nm		 0x0000000002000000ULL
#define	TYPE_Nx		 0x0000000004000000ULL
#define	TYPE_Ox		 0x0000000008000000ULL
#define	TYPE_Pa		 0x0000000010000000ULL
#define	TYPE_Rs		 0x0000000020000000ULL
#define	TYPE_Sh		 0x0000000040000000ULL
#define	TYPE_Ss		 0x0000000080000000ULL
#define	TYPE_St		 0x0000000100000000ULL
#define	TYPE_Sy		 0x0000000200000000ULL
#define	TYPE_Tn		 0x0000000400000000ULL
#define	TYPE_Va		 0x0000000800000000ULL
#define	TYPE_Vt		 0x0000001000000000ULL
#define	TYPE_Xr		 0x0000002000000000ULL
#define	TYPE_sec	 0x0000004000000000ULL
#define	TYPE_arch	 0x0000008000000000ULL

__BEGIN_DECLS

struct	manpage {
	char		*file; /* to be prefixed by manpath */
	char		*names; /* a list of names with sections */
	char		*desc; /* description of manpage */
	char		*output; /* user-defined additional output */
	int		 form; /* 0 == catpage */
};

struct	mansearch {
	const char	*arch; /* architecture/NULL */
	const char	*sec; /* mansection/NULL */
	uint64_t	 deftype; /* type if no key  */
	int		 flags;
#define	MANSEARCH_WHATIS 0x01 /* whatis mode: equality, no key */
};

int	mansearch(const struct mansearch *cfg, /* options */
		const struct manpaths *paths, /* manpaths */
		int argc, /* size of argv */
		char *argv[],  /* search terms */
		const char *outkey, /* name of additional output key */
		struct manpage **res, /* results */
		size_t *ressz); /* results returned */

__END_DECLS

#endif /*!MANSEARCH_H*/
