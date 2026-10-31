/*	$NetBSD: strlist.c,v 1.2 2021/01/23 19:41:16 thorpej Exp $	*/

/*-
 * Copyright (c) 2021 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * strlist --
 *
 *	A set of routines for interacting with IEEE 1275 (OpenFirmware)
 *	style string lists.
 *
 *	An OpenFirmware string list is simply a buffer containing
 *	multiple NUL-terminated strings concatenated together.
 *
 *	So, for example, the a string list consisting of the strings
 *	"foo", "bar", and "baz" would be represented in memory like:
 *
 *		foo\0bar\0baz\0
 */

#include <sys/types.h>

/*
 * Memory allocation wrappers to handle different environments.
 */
#if defined(_KERNEL)
#include <sys/kmem.h>
#include <sys/systm.h>

static void *
strlist_alloc(size_t const size)
{
	return kmem_zalloc(size, KM_SLEEP);
}

static void
strlist_free(void * const v, size_t const size)
{
	kmem_free(v, size);
}
#elif defined(_STANDALONE)
#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

static void *
strlist_alloc(size_t const size)
{
	char *cp = alloc(size);
	if (cp != NULL) {
		memset(cp, 0, size);
	}
	return cp;
}

static void
strlist_free(void * const v, size_t const size)
{
	dealloc(v, size);
}
#else /* user-space */
#include <stdlib.h>
#include <string.h>

extern int pmatch(const char *, const char *, const char **);

static void *
strlist_alloc(size_t const size)
{
	return calloc(1, size);
}

static void
strlist_free(void * const v, size_t const size __unused)
{
	free(v);
}
#endif

#include "strlist.h"

/*
 * strlist_next --
 *
 *	Return a pointer to the next string in the strlist,
 *	or NULL if there are no more strings.
 */
const char *
strlist_next(const char * const sl, size_t const slsize, size_t * const cursorp)
{

	if (sl == NULL || slsize == 0 || cursorp == NULL) {
		return NULL;
	}

	size_t cursor = *cursorp;

	if (cursor >= slsize) {
		/* No more strings in the list. */
		return NULL;
	}

	const char *cp = sl + cursor;
	*cursorp = cursor + strlen(cp) + 1;

	return cp;
}

/*
 * strlist_count --
 *
 *	Return the number of strings in the strlist.
 */
unsigned int
strlist_count(const char *sl, size_t slsize)
{

	if (sl == NULL || slsize == 0) {
		return 0;
	}

	size_t cursize;
	unsigned int count;

	for (count = 0; slsize != 0;
	     count++, sl += cursize, slsize -= cursize) {
		cursize = strlen(sl) + 1;
	}
	return count;
}

/*
 * strlist_string --
 *
 *	Returns the string in the strlist at the specified index.
 *	Returns NULL if the index is beyond the strlist range.
 */
const char *
strlist_string(const char * sl, size_t slsize, unsigned int const idx)
{

	if (sl == NULL || slsize == 0) {
		return NULL;
	}

	size_t cursize;
	unsigned int i;

	for (i = 0; slsize != 0; i++, slsize -= cursize, sl += cursize) {
		cursize = strlen(sl) + 1;
		if (i == idx) {
			return sl;
		}
	}

	return NULL;
}

static bool
match_strcmp(const char * const s1, const char * const s2)
{
	return strcmp(s1, s2) == 0;
}

#if !defined(_STANDALONE)
static bool
match_pmatch(const char * const s1, const char * const s2)
{
	return pmatch(s1, s2, NULL) == 2;
}
#endif /* _STANDALONE */

static bool
strlist_match_internal(const char * const sl, size_t slsize,
    const char * const str, int * const indexp, unsigned int * const countp,
    bool (*match_fn)(const char *, const char *))
{
	const char *cp;
	size_t l;
	int i;
	bool rv = false;

	if (sl == NULL || slsize == 0) {
		return false;
	}

	cp = sl;

	for (i = 0; slsize != 0;
	     l = strlen(cp) + 1, slsize -= l, cp += l, i++) {
		if (rv) {
			/*
			 * We've already matched. We must be
			 * counting to the end.
			 */
			continue;
		}
		if ((*match_fn)(cp, str)) {
			/*
			 * Matched!  Get the index.  If we don't
			 * also want the total count, then get
			 * out early.
			 */
			*indexp = i;
			rv = true;
			if (countp == NULL) {
				break;
			}
		}
	}

	if (countp != NULL) {
		*countp = i;
	}

	return rv;
}

/*
 * strlist_match --
 *
 *	Returns a weighted match value (1 <= match <= sl->count) if the
 *	specified string appears in the strlist.  A match at the
 *	beginning of the list carriest the greatest weight (i.e. sl->count)
 *	and a match at the end of the list carriest the least (i.e. 1).
 *	Returns 0 if there is no match.
 *
 *	This routine operates independently of the cursor used to enumerate
 *	a strlist.
 */
int
strlist_match(const char * const sl, size_t const slsize,
    const char * const str)
{
	unsigned int count;
	int idx;

	if (strlist_match_internal(sl, slsize, str, &idx, &count,
				   match_strcmp)) {
		return count - idx;
	}
	return 0;
}

#if !defined(_STANDALONE)
/*
 * strlist_pmatch --
 *
 *	Like strlist_match(), but uses pmatch(9) to match the
 *	strings.
 */
int
strlist_pmatch(const char * const sl, size_t const slsize,
    const char * const pattern)
{
	unsigned int count;
	int idx;

	if (strlist_match_internal(sl, slsize, pattern, &idx, &count,
				   match_pmatch)) {
		return count - idx;
	}
	return 0;
}
#endif /* _STANDALONE */

/*
 * strlist_index --
 *
 *	Returns the index of the specified string if it appears
 *	in the strlist.  Returns -1 if the string is not found.
 *
 *	This routine operates independently of the cursor used to enumerate
 *	a strlist.
 */
int
strlist_index(const char * const sl, size_t const slsize,
    const char * const str)
{
	int idx;

	if (strlist_match_internal(sl, slsize, str, &idx, NULL,
				   match_strcmp)) {
		return idx;
	}
	return -1;
}

/*
 * strlist_append --
 *
 *	Append the specified string to a mutable strlist.  Turns
 *	true if successful, false upon failure for any reason.
 */
bool
strlist_append(char ** const slp, size_t * const slsizep,
    const char * const str)
{
	size_t const slsize = *slsizep;
	char * const sl = *slp;

	size_t const addsize = strlen(str) + 1;
	size_t const newsize = slsize + addsize;
	char * const newbuf = strlist_alloc(newsize);

	if (newbuf == NULL) {
		return false;
	}

	if (sl != NULL) {
		memcpy(newbuf, sl, slsize);
	}

	memcpy(newbuf + slsize, str, addsize);

	if (sl != NULL) {
		strlist_free(sl, slsize);
	}

	*slp = newbuf;
	*slsizep = newsize;

	return true;
}

#ifdef STRLIST_TEST
/*
 * To build and run the tests:
 *
 * % cc -DSTRLIST_TEST -Os pmatch.c strlist.c
 * % ./a.out
 * Testing basic properties.
 * Testing enumeration.
 * Testing weighted matching.
 * Testing pattern matching.
 * Testing index return.
 * Testing string-at-index.
 * Testing gross blob count.
 * Testing gross blob indexing.
 * Testing creating a strlist.
 * Verifying new strlist.
 * All tests completed successfully.
 * %
 */

static char nice_blob[] = "zero\0one\0two\0three\0four\0five";
static char gross_blob[] = "zero\0\0two\0\0four\0\0";

#include <assert.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
	const char *sl;
	size_t slsize;
	size_t cursor;
	const char *cp;
	size_t size;

	sl = nice_blob;
	slsize = sizeof(nice_blob);

	printf("Testing basic properties.\n");
	assert(strlist_count(sl, slsize) == 6);

	printf("Testing enumeration.\n");
	cursor = 0;
	assert((cp = strlist_next(sl, slsize, &cursor)) != NULL);
	assert(strcmp(cp, "zero") == 0);

	assert((cp = strlist_next(sl, slsize, &cursor)) != NULL);
	assert(strcmp(cp, "one") == 0);

	assert((cp = strlist_next(sl, slsize, &cursor)) != NULL);
	assert(strcmp(cp, "two") == 0);

	assert((cp = strlist_next(sl, slsize, &cursor)) != NULL);
	assert(strcmp(cp, "three") == 0);

	assert((cp = strlist_next(sl, slsize, &cursor)) != NULL);
	assert(strcmp(cp, "four") == 0);

	assert((cp = strlist_next(sl, slsize, &cursor)) != NULL);
	assert(strcmp(cp, "five") == 0);

	assert((cp = strlist_next(sl, slsize, &cursor)) == NULL);

	printf("Testing weighted matching.\n");
	assert(strlist_match(sl, slsize, "non-existent") == 0);
	assert(strlist_match(sl, slsize, "zero") == 6);
	assert(strlist_match(sl, slsize, "one") == 5);
	assert(strlist_match(sl, slsize, "two") == 4);
	assert(strlist_match(sl, slsize, "three") == 3);
	assert(strlist_match(sl, slsize, "four") == 2);
	assert(strlist_match(sl, slsize, "five") == 1);

	printf("Testing pattern matching.\n");
	assert(strlist_pmatch(sl, slsize, "t?o") == 4);
	assert(strlist_pmatch(sl, slsize, "f[a-o][o-u][a-z]") == 2);

	printf("Testing index return.\n");
	assert(strlist_index(sl, slsize, "non-existent") == -1);
	assert(strlist_index(sl, slsize, "zero") == 0);
	assert(strlist_index(sl, slsize, "one") == 1);
	assert(strlist_index(sl, slsize, "two") == 2);
	assert(strlist_index(sl, slsize, "three") == 3);
	assert(strlist_index(sl, slsize, "four") == 4);
	assert(strlist_index(sl, slsize, "five") == 5);

	printf("Testing string-at-index.\n");
	assert(strcmp(strlist_string(sl, slsize, 0), "zero") == 0);
	assert(strcmp(strlist_string(sl, slsize, 1), "one") == 0);
	assert(strcmp(strlist_string(sl, slsize, 2), "two") == 0);
	assert(strcmp(strlist_string(sl, slsize, 3), "three") == 0);
	assert(strcmp(strlist_string(sl, slsize, 4), "four") == 0);
	assert(strcmp(strlist_string(sl, slsize, 5), "five") == 0);
	assert(strlist_string(sl, slsize, 6) == NULL);

	sl = gross_blob;
	slsize = sizeof(gross_blob);

	printf("Testing gross blob count.\n");
	assert(strlist_count(sl, slsize) == 7);

	printf("Testing gross blob indexing.\n");
	assert(strcmp(strlist_string(sl, slsize, 0), "zero") == 0);
	assert(strcmp(strlist_string(sl, slsize, 1), "") == 0);
	assert(strcmp(strlist_string(sl, slsize, 2), "two") == 0);
	assert(strcmp(strlist_string(sl, slsize, 3), "") == 0);
	assert(strcmp(strlist_string(sl, slsize, 4), "four") == 0);
	assert(strcmp(strlist_string(sl, slsize, 5), "") == 0);
	assert(strcmp(strlist_string(sl, slsize, 6), "") == 0);
	assert(strlist_string(sl, slsize, 7) == NULL);


	printf("Testing creating a strlist.\n");
	char *newsl = NULL;
	size_t newslsize = 0;
	assert(strlist_append(&newsl, &newslsize, "zero"));
	assert(strlist_append(&newsl, &newslsize, "one"));
	assert(strlist_append(&newsl, &newslsize, "two"));
	assert(strlist_append(&newsl, &newslsize, "three"));
	assert(strlist_append(&newsl, &newslsize, "four"));
	assert(strlist_append(&newsl, &newslsize, "five"));

	printf("Verifying new strlist.\n");
	assert(strlist_count(newsl, newslsize) == 6);
	assert(strcmp(strlist_string(newsl, newslsize, 0), "zero") == 0);
	assert(strcmp(strlist_string(newsl, newslsize, 1), "one") == 0);
	assert(strcmp(strlist_string(newsl, newslsize, 2), "two") == 0);
	assert(strcmp(strlist_string(newsl, newslsize, 3), "three") == 0);
	assert(strcmp(strlist_string(newsl, newslsize, 4), "four") == 0);
	assert(strcmp(strlist_string(newsl, newslsize, 5), "five") == 0);
	assert(strlist_string(newsl, newslsize, 6) == NULL);

	/* This should be equivalent to nice_blob. */
	assert(newslsize == sizeof(nice_blob));
	assert(memcmp(newsl, nice_blob, newslsize) == 0);


	printf("All tests completed successfully.\n");
	return 0;
}

#endif /* STRLIST_TEST */
