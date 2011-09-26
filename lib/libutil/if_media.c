/*	$NetBSD: if_media.c,v 1.2 2008/04/28 20:23:03 martin Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: if_media.c,v 1.2 2008/04/28 20:23:03 martin Exp $");
#endif 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <net/if_media.h>

struct ifmedia_description ifm_mode_descriptions[] =
    IFM_MODE_DESCRIPTIONS;

struct ifmedia_description ifm_type_descriptions[] =
    IFM_TYPE_DESCRIPTIONS;

struct ifmedia_description ifm_subtype_descriptions[] =
    IFM_SUBTYPE_DESCRIPTIONS;

struct ifmedia_description ifm_option_descriptions[] =
    IFM_OPTION_DESCRIPTIONS;

const char *
get_media_type_string(int mword)
{
	struct ifmedia_description *desc;

	for (desc = ifm_type_descriptions; desc->ifmt_string != NULL; desc++) {
		if (IFM_TYPE(mword) == desc->ifmt_word)
			return (desc->ifmt_string);
	}
	return "<unknown type>";
}

const char *
get_media_subtype_string(int mword)
{
	struct ifmedia_description *desc;

	for (desc = ifm_subtype_descriptions; desc->ifmt_string != NULL;
	     desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, mword) &&
		    IFM_SUBTYPE(desc->ifmt_word) == IFM_SUBTYPE(mword))
			return desc->ifmt_string;
	}
	return "<unknown subtype>";
}

const char *
get_media_mode_string(int mword)
{
	struct ifmedia_description *desc;

	for (desc = ifm_mode_descriptions; desc->ifmt_string != NULL; desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, mword) &&
		    IFM_MODE(mword) == IFM_MODE(desc->ifmt_word))
			return desc->ifmt_string;
	}
	return NULL;
}

const char *
get_media_option_string(int *mwordp)
{
	struct ifmedia_description *desc;
	int mword = *mwordp;

	for (desc = ifm_option_descriptions; desc->ifmt_string != NULL;
	     desc++) {
		if (!IFM_TYPE_MATCH(desc->ifmt_word, mword))
			continue;
		if (mword & IFM_OPTIONS(desc->ifmt_word)) {
			*mwordp = mword & ~IFM_OPTIONS(desc->ifmt_word);
			return desc->ifmt_string;
		}
	}

	/* Historical behaviour is to ignore unknown option bits! */
	*mwordp = mword & ~IFM_OPTIONS(~0);
	return NULL;
}

int
lookup_media_word(struct ifmedia_description *desc, int type, const char *val)
{

	for (; desc->ifmt_string != NULL; desc++) {
		if (IFM_TYPE_MATCH(desc->ifmt_word, type) &&
		    strcasecmp(desc->ifmt_string, val) == 0)
			return (desc->ifmt_word);
	}
	return -1;
}

int
get_media_mode(int type, const char *val)
{

	return lookup_media_word(ifm_mode_descriptions, type, val);
}

int
get_media_subtype(int type, const char *val)
{

	return lookup_media_word(ifm_subtype_descriptions, type, val);
}

int
get_media_options(int type, const char *val, char **invalid)
{
	char *optlist, *str;
	int option, rval = 0;

	/* We muck with the string, so copy it. */
	optlist = strdup(val);
	if (optlist == NULL) {
		if (invalid != NULL)
			*invalid = NULL;
		return -1;
	}
	str = optlist;

	/*
	 * Look up the options in the user-provided comma-separated list.
	 */
	type = IFM_TYPE(type);
	for (; (str = strtok(str, ",")) != NULL; str = NULL) {
		option = lookup_media_word(ifm_option_descriptions, type, str);
		if (option != -1) {
			rval |= IFM_OPTIONS(option);
			continue;
		}
		rval = -1;
		if (invalid == NULL)
			break;
		/* Pass invalid option at start of malloced buffer */
		if (str != optlist)
			memmove(optlist, str, strlen(str) + 1);
		/* Caller should free() or exit() */
		*invalid = optlist;
		return rval;
	}

	free(optlist);
	return (rval);
}
