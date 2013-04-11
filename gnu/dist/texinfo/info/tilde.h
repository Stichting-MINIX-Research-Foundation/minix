/*	$NetBSD: tilde.h,v 1.1.1.4 2008/09/02 07:50:10 christos Exp $	*/

/* tilde.h: Externally available variables and function in libtilde.a.
   Id: tilde.h,v 1.3 2004/04/11 17:56:46 karl Exp

   This file has appeared in prior works by the Free Software Foundation;
   thus it carries copyright dates from 1988 through 1993.

   Copyright (C) 1988, 1989, 1990, 1991, 1992, 1993, 2004 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#ifndef TILDE_H
#define TILDE_H

#include "info.h"

/* If non-null, this contains the address of a function to call if the
   standard meaning for expanding a tilde fails.  The function is called
   with the text (sans tilde, as in "foo"), and returns a malloc()'ed string
   which is the expansion, or a NULL pointer if there is no expansion. */
extern CFunction *tilde_expansion_failure_hook;

/* When non-null, this is a NULL terminated array of strings which
   are duplicates for a tilde prefix.  Bash uses this to expand
   `=~' and `:~'. */
extern char **tilde_additional_prefixes;

/* When non-null, this is a NULL terminated array of strings which match
   the end of a username, instead of just "/".  Bash sets this to
   `:' and `=~'. */
extern char **tilde_additional_suffixes;

/* Return a new string which is the result of tilde expanding STRING. */
extern char *tilde_expand (char *string);

/* Do the work of tilde expansion on FILENAME.  FILENAME starts with a
   tilde.  If there is no expansion, call tilde_expansion_failure_hook. */
extern char *tilde_expand_word (char *filename);

#endif /* not TILDE_H */
