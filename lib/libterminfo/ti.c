/* $NetBSD: ti.c,v 1.2 2010/02/04 09:46:26 roy Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 *
 * This id is derived from software contributed to The NetBSD Foundation
 * by Roy Marples.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source id must retain the above copyright
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: ti.c,v 1.2 2010/02/04 09:46:26 roy Exp $");

#include <assert.h>
#include <string.h>
#include <term_private.h>
#include <term.h>

int
ti_getflag(const TERMINAL *term, const char *id)
{
	ssize_t ind;
	size_t i;
	TERMUSERDEF *ud;

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(id != NULL);

	ind = _ti_flagindex(id);
	if (ind != -1)
		return term->flags[ind];
	for (i = 0; i < term->_nuserdefs; i++) {
		ud = &term->_userdefs[i];
		if (ud->type == 'f' && strcmp(ud->id, id) == 0)
			return ud->flag;
	}
	return ABSENT_BOOLEAN;
}

int
tigetflag(const char *id)
{

	_DIAGASSERT(id != NULL);	
	if (cur_term != NULL)
		return ti_getflag(cur_term, id);
	return ABSENT_BOOLEAN;
}

int
ti_getnum(const TERMINAL *term, const char *id)
{
	ssize_t ind;
	size_t i;
	TERMUSERDEF *ud;

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(id != NULL);

	ind = _ti_numindex(id);
	if (ind != -1) {
		if (!VALID_NUMERIC(term->nums[ind]))
			return ABSENT_NUMERIC;
		return term->nums[ind];
	}
	for (i = 0; i < term->_nuserdefs; i++) {
		ud = &term->_userdefs[i];
		if (ud->type == 'n' && strcmp(ud->id, id) == 0) {
			if (!VALID_NUMERIC(ud->num))
			    return ABSENT_NUMERIC;
			return ud->num;
		}
	}
	return CANCELLED_NUMERIC;
}

int
tigetnum(const char *id)
{
	
	_DIAGASSERT(id != NULL);	
	if (cur_term != NULL)
		return ti_getnum(cur_term, id);
	return CANCELLED_NUMERIC;
}

const char *
ti_getstr(const TERMINAL *term, const char *id)
{
	ssize_t ind;
	size_t i;
	TERMUSERDEF *ud;

	_DIAGASSERT(term != NULL);
	_DIAGASSERT(id != NULL);

	ind = _ti_strindex(id);
	if (ind != -1)
		return term->strs[ind];
	for (i = 0; i < term->_nuserdefs; i++) {
		ud = &term->_userdefs[i];
		if (ud->type == 's' && strcmp(ud->id, id) == 0)
			return ud->str;
	}
	return (const char *)CANCELLED_STRING;
}

char *
tigetstr(const char *id)
{
	
	_DIAGASSERT(id != NULL);
	if (cur_term != NULL)
		return __UNCONST(ti_getstr(cur_term, id));
	return (char *)CANCELLED_STRING;
}
