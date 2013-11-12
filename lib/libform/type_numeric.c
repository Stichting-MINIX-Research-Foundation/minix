/*	$NetBSD: type_numeric.c,v 1.8 2004/10/28 21:14:52 dsl Exp $	*/

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
__RCSID("$NetBSD: type_numeric.c,v 1.8 2004/10/28 21:14:52 dsl Exp $");

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "form.h"
#include "internals.h"

/*
 * The numeric type handling.
 */

typedef struct 
{
	unsigned precision;
	double min;
	double max;
} numeric_args;

/*
 * Create the numeric arguments structure from the given args.  Return NULL
 * if the call fails, otherwise return a pointer to the structure allocated.
 */
static char *
create_numeric_args(va_list *args)
{
	numeric_args *new;

	new = (numeric_args *) malloc(sizeof(numeric_args));

	if (new != NULL) {
		new->precision = va_arg(*args, unsigned);
		new->min = va_arg(*args, double);
		new->max = va_arg(*args, double);
	}

	return (void *) new;
}

/*
 * Copy the numeric argument structure.
 */
static char *
copy_numeric_args(char *args)
{
	numeric_args *new;

	new = (numeric_args *) malloc(sizeof(numeric_args));

	if (new != NULL)
		bcopy(args, new, sizeof(numeric_args));

	return (void *) new;
}

/*
 * Free the allocated storage associated with the type arguments.
 */
static void
free_numeric_args(char *args)
{
	if (args != NULL)
		free(args);
}

/*
 * Check the contents of the field buffer are numeric only.  A valid
 * number is of the form nnnn[.mmmmm][Ee[+-]ddd]
 */
static int
numeric_check_field(FIELD *field, char *args)
{
	int cur;
	double number, max, min;
	int precision;
	char *buf, *new_buf;

	if (args == NULL)
		return FALSE;
	
	precision = ((numeric_args *) (void *) field->args)->precision;
	min = ((numeric_args *) (void *) field->args)->min;
	max = ((numeric_args *) (void *) field->args)->max;
	
	buf = args;
	cur = 0;

	  /* skip leading white space */
	while ((buf[cur] != '\0')
	       && ((buf[cur] == ' ') || (buf[cur] == '\t')))
		cur++;

	  /* no good if we have hit the end */
	if (buf[cur] == '\0')
		return FALSE;

	  /* find the end of the digits but allow a leading + or - sign, and
	   * a decimal point.
	   */
	if ((buf[cur] == '-') || (buf[cur] == '+'))
		cur++;
	
	while(isdigit((unsigned char)buf[cur]))
		cur++;

	  /* if not at end of string then check for decimal... */
	if ((buf[cur] != '\0') && (buf[cur] == '.')) {
		cur++;
		  /* check for more digits now.... */
		while(isdigit((unsigned char)buf[cur]))
			cur++;
	}
	
	  /* check for an exponent */
	if ((buf[cur] != '\0') &&
	    ((buf[cur] == 'E') || (buf[cur] == 'e'))) {
		cur++;
		if (buf[cur] == '\0')
			return FALSE;
			
		  /* allow a + or a - for exponent */
		if ((buf[cur] == '+') || (buf[cur] == '-'))
			cur++;

		if (buf[cur] == '\0')
			return FALSE;
			
		  /* we expect a digit now */
		if (!isdigit((unsigned char)buf[cur]))
			return FALSE;
			
		  /* skip digits for the final time */
		while(isdigit((unsigned char)buf[cur]))
			cur++;
	}
			
	  /* check there is only trailing whitespace */
	while ((buf[cur] != '\0')
	       && ((buf[cur] == ' ') || (buf[cur] == '\t')))
		cur++;

	  /* no good if we are not at the end of the string */
	if (buf[cur] != '\0')
		return FALSE;

	  /* convert and range check the number...*/
	number = atof(buf);
	if ((min < max) && ((number < min) || (number > max)))
		return FALSE;

	if (asprintf(&new_buf, "%.*f", precision, number) < 0)
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
numeric_check_char(/* ARGSUSED1 */ int c, char *args)
{
	return ((isdigit(c) || (c == '-') || (c == '+')
		 || (c == '.') || (c == 'e') || (c == 'E')) ? TRUE : FALSE);
}

static FIELDTYPE builtin_numeric = {
	_TYPE_HAS_ARGS | _TYPE_IS_BUILTIN,  /* flags */
	0,                                  /* refcount */
	NULL,                               /* link */
	create_numeric_args,                  /* make_args */
	copy_numeric_args,                    /* copy_args */
	free_numeric_args,                    /* free_args */
	numeric_check_field,                  /* field_check */
	numeric_check_char,                   /* char_check */
	NULL,                               /* next_choice */
	NULL                                /* prev_choice */
};

FIELDTYPE *TYPE_NUMERIC = &builtin_numeric;


