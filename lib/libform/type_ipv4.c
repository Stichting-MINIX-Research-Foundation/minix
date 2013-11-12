/*	$NetBSD: type_ipv4.c,v 1.10 2007/01/17 23:24:22 hubertf Exp $	*/

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
__RCSID("$NetBSD: type_ipv4.c,v 1.10 2007/01/17 23:24:22 hubertf Exp $");

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "form.h"
#include "internals.h"

/*
 * The IP v4 address type handling.
 */

/*
 * define the styles of address we can have, they are:
 *    FORMI_DOTTED_QUAD    address of form aaa.bbb.ccc.ddd
 *    FORMI_HEX            address of form 0xaabbccdd
 *    FORMI_CLASSLESS      address of form aaa.bbb.ccc.ddd/ee
 */
#define FORMI_DOTTED_QUAD  0
#define FORMI_HEX          1
#define FORMI_CLASSLESS    2

/*
 * Check the contents of the field buffer are a valid IPv4 address only.
 */
static int
ipv4_check_field(FIELD *field, char *args)
{
	char *buf, *buf1, *keeper, *p, *slash;
	unsigned int vals[4], style, start, mask;
	unsigned long hex_val, working;
	int i;

	if (args == NULL)
		return FALSE;
	
	if (asprintf(&keeper, "%s", args) < 0)
		return FALSE;

#ifdef DEBUG
	fprintf(dbg, "ipv4_check_field: enter with args of %s\n", keeper);
#endif
	style = FORMI_DOTTED_QUAD;
	buf = keeper;
	hex_val = 0;
	mask = 0;
	
	if ((slash = index(buf, '/')) != NULL)
		style = FORMI_CLASSLESS;
	else {
		start = _formi_skip_blanks(buf, 0);
		if ((buf[start] != '\0') && (buf[start + 1] != '\0') &&
		    (buf[start] == '0') && ((buf[start + 1] == 'x') ||
					    (buf[start + 1] == 'X')))
			style = FORMI_HEX;
	}

	switch (style) {
	case FORMI_CLASSLESS:
		*slash = '\0';
		slash++;
		mask = atoi(slash);
		if (mask > 32)
			goto FAIL;
		  /* FALLTHROUGH */
		
	case FORMI_DOTTED_QUAD:
		for (i = 0; i < 4; i++) {
			p = strsep(&buf, ".");
			if ((p == NULL) || (*p == '\0'))
				goto FAIL;
			vals[i] = atoi(p);
			if (vals[i] > 255)
				goto FAIL;
		}
		break;

		
	case FORMI_HEX:
		errno = 0;
		hex_val = strtoul(buf, NULL, 16);
		if ((hex_val == ULONG_MAX) && (errno == ERANGE))
			goto FAIL;
		
		working = hex_val;
		for (i = 3; i >= 0; i--) {
			vals[i] = (unsigned int)(working & 0xffUL);
			working = working >> 8;
		}
		break;

	}
	
	free(keeper);

	buf1 = NULL;
	
	switch (style) {
	case FORMI_DOTTED_QUAD:
		if (asprintf(&buf, "%d.%d.%d.%d", vals[0], vals[1], vals[2],
			     vals[3]) < 0)
			return FALSE;
		if (asprintf(&buf1, "%d.%d.%d.%d", vals[0], vals[1],
			     vals[2], vals[3]) < 0)
			return FALSE;
		break;

	case FORMI_CLASSLESS:
		if (asprintf(&buf, "%d.%d.%d.%d/%d", vals[0], vals[1],
			     vals[2], vals[3], mask) < 0)
			return FALSE;
		if (asprintf(&buf1, "%d.%d.%d.%d", vals[0], vals[1],
			     vals[2], vals[3]) < 0)
			return FALSE;
		break;

	case FORMI_HEX:
		if (asprintf(&buf, "0x%.8lx", hex_val) < 0)
			return FALSE;
		if (asprintf(&buf1, "%d.%d.%d.%d", vals[0], vals[1],
			     vals[2], vals[3]) < 0)
			return FALSE;
		break;
	}
	
	  /* re-set the field buffer to be the reformatted IPv4 address */
	set_field_buffer(field, 0, buf);

	  /*
	   * Set the field buffer 1 to the dotted quad format regardless
	   * of the input format, only if buffer 1 exists.
	   */
	if (field->nbuf > 1)
		set_field_buffer(field, 1, buf1);

#ifdef DEBUG
	fprintf(dbg, "ipv4_check_field: buf0 set to %s\n", buf);
	fprintf(dbg, "ipv4_check_field: buf1 set to %s\n", buf1);
#endif
	free(buf);
	free(buf1);
	
	return TRUE;

	  /* bail out point if we got a bad entry */
  FAIL:
	free(keeper);
	return FALSE;
	
}

/*
 * Check the given character is numeric, return TRUE if it is.
 */
static int
ipv4_check_char(/* ARGSUSED1 */ int c, char *args)
{
	return (isxdigit(c) || (c == '.') || (tolower(c) == 'x') ||
		(c == '/'))? TRUE : FALSE;
}

static FIELDTYPE builtin_ipv4 = {
	_TYPE_IS_BUILTIN,                   /* flags */
	0,                                  /* refcount */
	NULL,                               /* link */
	NULL,                               /* make_args */
	NULL,                               /* copy_args */
	NULL,                               /* free_args */
	ipv4_check_field,                   /* field_check */
	ipv4_check_char,                    /* char_check */
	NULL,                               /* next_choice */
	NULL                                /* prev_choice */
};

FIELDTYPE *TYPE_IPV4 = &builtin_ipv4;


