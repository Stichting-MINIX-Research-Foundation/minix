/*-
 * Copyright (c) 2012 Alistair Crooks <agc@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ARRAY_H_
#define ARRAY_H_	20120921

#ifndef PGPV_ARRAY
/* creates 2 unsigned vars called "name"c and "name"size in current scope */
/* also creates an array called "name"s in current scope */
#define PGPV_ARRAY(type, name)						\
	unsigned name##c; unsigned name##vsize; type *name##s
#endif

/* if this isn't part of a struct, need to specifically initialise things */
#define ARRAY_INIT(name) do {						\
	name##c = name##vsize = 0;					\
	name##s = NULL;							\
} while(/*CONSTCOND*/0)

/* check the array is big enough - if not, expand it by explicit amount */
/* this is clunky, but there are bugs a-lurking */
#define ARRAY_EXPAND_SIZED(name, mult, add) do {			\
	if (name##c == name##vsize) {					\
		void		*_v;					\
		char		*_cv = NULL;				\
		unsigned	 _ents;					\
		_ents = (name##vsize * (mult)) + (add); 		\
		_cv = _v = realloc(name##s, _ents * sizeof(*name##s));	\
		if (_v == NULL) {					\
			fprintf(stderr, "ARRAY_EXPAND - bad realloc\n"); \
		} else {						\
			memset(&_cv[name##vsize * sizeof(*name##s)],	\
				0x0, (_ents - name##vsize) * sizeof(*name##s)); \
			name##s = _v;					\
			name##vsize = _ents;				\
		}							\
	}								\
} while(/*CONSTCOND*/0)

/* check the array is big enough - if not, expand it (size * 2) + 10 */
#define ARRAY_EXPAND(name)	ARRAY_EXPAND_SIZED(name, 2, 10)

#define ARRAY_ELEMENT(name, num)	name##s[num]
#define ARRAY_LAST(name)		name##s[name##c - 1]
#define ARRAY_COUNT(name)		name##c
#define ARRAY_SIZE(name)		name##vsize
#define ARRAY_ARRAY(name)		name##s

#define ARRAY_APPEND(name, newel) do {					\
	ARRAY_EXPAND(name);						\
	ARRAY_COUNT(name) += 1;						\
	ARRAY_LAST(name) = newel;					\
} while(/*CONSTCOND*/0)

#define ARRAY_DELETE(name, num)	do {					\
	ARRAY_COUNT(name) -= 1;						\
	memmove(&ARRAY_ELEMENT(name, num), &ARRAY_ELEMENT(name, num + 1), \
		(ARRAY_COUNT(name) - (num)) * sizeof(ARRAY_ELEMENT(name, 0))); \
} while(/*CONSTCOND*/0)

#endif
