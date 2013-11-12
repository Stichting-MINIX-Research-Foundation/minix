/*	$NetBSD: type_integer.c,v 1.8 2004/10/28 21:14:52 dsl Exp $	*/

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
__RCSID("$NetBSD: type_integer.c,v 1.8 2004/10/28 21:14:52 dsl Exp $");

#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include "form.h"
#include "internals.h"

/*
 * The integer type handling.
 */

typedef struct 
{
	unsigned precision;
	long min;
	long max;
} integer_args;

/*
 * Create the integer arguments structure from the given args.  Return NULL
 * if the call fails, otherwise return a pointer to the structure allocated.
 */
static char *
create_integer_args(va_list *args)
{
	integer_args *new;

	new = (integer_args *) malloc(sizeof(integer_args));

	if (new != NULL) {
		new->precision = va_arg(*args, unsigned);
		new->min = va_arg(*args, long);
		new->max = va_arg(*args, long);
	}

	return (void *) new;
}

/*
 * Copy the integer argument structure.
 */
static char *
copy_integer_args(char *args)
{
	integer_args *new;

	new = (integer_args *) malloc(sizeof(integer_args));

	if (new != NULL)
		bcopy(args, new, sizeof(integer_args));

	return (void *) new;
}

/*
 * Free the allocated storage associated with the type arguments.
 */
static void
free_integer_args(char *args)
{
	if (args != NULL)
		free(args);
}

/*
 * Check the contents of the field buffer are digits only.
 */
static int
integer_check_field(FIELD *field, char *args)
{
	int cur;
	long number, max, min;
	int precision;
	char *buf, *new_buf;

	if (args == NULL)
		return FALSE;
	
	precision = ((integer_args *) (void *) field->args)->precision;
	min = ((integer_args *) (void *) field->args)->min;
	max = ((integer_args *) (void *) field->args)->max;
	
	buf = args;
	cur = 0;

	  /* skip leading white space */
	while ((buf[cur] != '\0')
	       && ((buf[cur] == ' ') || (buf[cur] == '\t')))
		cur++;

	  /* no good if we have hit the end */
	if (buf[cur] == '\0')
		return FALSE;

	  /* find the end of the digits but allow a leading + or - sign */
	if ((buf[cur] == '-') || (buf[cur] == '+'))
		cur++;
	
	while(isdigit((unsigned char)buf[cur]))
		cur++;

	  /* check there is only trailing whitespace */
	while ((buf[cur] != '\0')
	       && ((buf[cur] == ' ') || (buf[cur] == '\t')))
		cur++;

	  /* no good if we are not at the end of the string */
	if (buf[cur] != '\0')
		return FALSE;

	  /* convert and range check the number...*/
	number = atol(buf);
	if ((min > max) || ((number < min) || (number > max)))
		return FALSE;

	if (asprintf(&new_buf, "%.*ld", precision, number) < 0)
		return FALSE;

	  /* re-set the field buffer to be the reformatted numeric */
	set_field_buffer(field, 0, new_buf);

	free(new_buf);
	
	  /* otherwise all was ok */
	return TRUE;
}

/*
 * Check the given character is numeric, return TRUE if it is.
 */
static int
integer_check_char(/* ARGSUSED1 */ int c, char *args)
{
	return ((isdigit(c) || (c == '-') || (c == '+')) ? TRUE : FALSE);
}

static FIELDTYPE builtin_integer = {
	_TYPE_HAS_ARGS | _TYPE_IS_BUILTIN,  /* flags */
	0,                                  /* refcount */
	NULL,                               /* link */
	create_integer_args,                  /* make_args */
	copy_integer_args,                    /* copy_args */
	free_integer_args,                    /* free_args */
	integer_check_field,                  /* field_check */
	integer_check_char,                   /* char_check */
	NULL,                               /* next_choice */
	NULL                                /* prev_choice */
};

FIELDTYPE *TYPE_INTEGER = &builtin_integer;


