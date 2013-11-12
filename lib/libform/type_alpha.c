/*	$NetBSD: type_alpha.c,v 1.11 2004/11/24 11:57:09 blymn Exp $	*/

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
__RCSID("$NetBSD: type_alpha.c,v 1.11 2004/11/24 11:57:09 blymn Exp $");

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "form.h"
#include "internals.h"

/*
 * The alpha type handling.
 */

typedef struct 
{
	unsigned width;
} alpha_args;

/*
 * Create the alpha arguments structure from the given args.  Return NULL
 * if the call fails, otherwise return a pointer to the structure allocated.
 */
static char *
create_alpha_args(va_list *args)
{
	alpha_args *new;

	new = (alpha_args *) malloc(sizeof(alpha_args));

	if (new != NULL)
		new->width = va_arg(*args, int);

	return (void *) new;
}

/*
 * Copy the alpha argument structure.
 */
static char *
copy_alpha_args(char *args)
{
	alpha_args *new;

	new = (alpha_args *) malloc(sizeof(alpha_args));

	if (new != NULL)
		new->width = ((alpha_args *) (void *) args)->width;

	return (void *) new;
}

/*
 * Free the allocated storage associated with the type arguments.
 */
static void
free_alpha_args(char *args)
{
	if (args != NULL)
		free(args);
}

/*
 * Check the contents of the field buffer are alphanumeric only.
 */
static int
alpha_check_field(FIELD *field, char *args)
{
	int width, start, cur, end;
	char *buf, *new;

	width = ((alpha_args *) (void *) field->args)->width;
	buf = args;
	start = 0;

	if (buf == NULL)
		return FALSE;
	
	  /* skip leading white space */
	while ((buf[start] != '\0')
	       && ((buf[start] == ' ') || (buf[start] == '\t')))
		start++;

	  /* no good if we have hit the end */
	if (buf[start] == '\0')
		return FALSE;

	  /* find the end of the non-whitespace stuff */
	cur = start;
	while(isalpha((unsigned char)buf[cur]))
		cur++;

	  /* no good if it exceeds the width */
	if ((cur - start) > width)
		return FALSE;

	end = cur;
	
	  /* check there is only trailing whitespace */
	while ((buf[cur] != '\0')
	       && ((buf[cur] == ' ') || (buf[cur] == '\t')))
		cur++;

	  /* no good if we are not at the end of the string */
	if (buf[cur] != '\0')
		return FALSE;

	  /* set buffer 0 to the new string */
	if ((new = (char *) malloc(sizeof(char) * (end - start))) == NULL)
		return FALSE;

	if ((end - start) >= 1) {
		strncpy(new, &buf[start], (size_t) (end - start - 1));
		new[end] = '\0';
	} else
		new[0] = '\0';
	
		
	set_field_buffer(field, 0, new);
	free(new);
	
	  /* otherwise all was ok */
	return TRUE;
}

/*
 * Check the given character is alphabetic, return TRUE if it is.
 */
static int
alpha_check_char(/* ARGSUSED1 */ int c, char *args)
{
	return (isalpha(c) ? TRUE : FALSE);
}

static FIELDTYPE builtin_alpha = {
	_TYPE_HAS_ARGS | _TYPE_IS_BUILTIN,  /* flags */
	0,                                  /* refcount */
	NULL,                               /* link */
	create_alpha_args,                  /* make_args */
	copy_alpha_args,                    /* copy_args */
	free_alpha_args,                    /* free_args */
	alpha_check_field,                  /* field_check */
	alpha_check_char,                   /* char_check */
	NULL,                               /* next_choice */
	NULL                                /* prev_choice */
};

FIELDTYPE *TYPE_ALPHA = &builtin_alpha;


