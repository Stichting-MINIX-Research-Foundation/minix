/*	$NetBSD: type_regex.c,v 1.7 2004/11/24 11:57:09 blymn Exp $	*/

/*-
 * Copyright (c) 1998-1999 Brett Lymn
 *                         (blymn@baea.com.au, brett_lymn@yahoo.com.au)
 * All rights reserved.
 *
 * This code has been donated to The NetBSD Foundation by the Author.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: type_regex.c,v 1.7 2004/11/24 11:57:09 blymn Exp $");

#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include "form.h"
#include "internals.h"

/*
 * The regex type handling.
 */

typedef struct 
{
	regex_t compiled;
	unsigned references;
} regex_args;

/*
 * Create the regex arguments structure from the given args.  Return NULL
 * if the call fails, otherwise return a pointer to the structure allocated.
 */
static char *
create_regex_args(va_list *args)
{
	regex_args *new;
	char *expression;

	new = (regex_args *) malloc(sizeof(regex_args));

	if (new != NULL) {
		new->references = 1;
		expression = va_arg(*args, char *);
		if ((regcomp(&new->compiled, expression,
			     (REG_EXTENDED | REG_NOSUB | REG_NEWLINE))) != 0) {
			free(new);
			return NULL;
		}
	}

	return (void *) new;
}

/*
 * Copy the regex argument structure.
 */
static char *
copy_regex_args(char *args)
{
	((regex_args *) (void *) args)->references++;
	
	return (void *) args;
}

/*
 * Free the allocated storage associated with the type arguments.
 */
static void
free_regex_args(char *args)
{
	if (args != NULL) {
		((regex_args *) (void *) args)->references--;
		if (((regex_args *) (void *) args)->references == 0)
			free(args);
	}
}

/*
 * Check the contents of the field buffer match the regex.
 */
static int
regex_check_field(FIELD *field, char *args)
{
	if ((args != NULL) &&
	    (regexec(&((regex_args *) (void *) field->args)->compiled,
		   args, (size_t) 0, NULL, 0) == 0))
		return TRUE;

	return FALSE;
}

static FIELDTYPE builtin_regex = {
	_TYPE_HAS_ARGS | _TYPE_IS_BUILTIN,  /* flags */
	0,                                  /* refcount */
	NULL,                               /* link */
	create_regex_args,                  /* make_args */
	copy_regex_args,                    /* copy_args */
	free_regex_args,                    /* free_args */
	regex_check_field,                  /* field_check */
	NULL,                               /* char_check */
	NULL,                               /* next_choice */
	NULL                                /* prev_choice */
};

FIELDTYPE *TYPE_REGEXP = &builtin_regex;


