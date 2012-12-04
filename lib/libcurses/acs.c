/*	$NetBSD: acs.c,v 1.20 2012/04/21 12:27:27 roy Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julian Coleman.
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
#ifndef lint
__RCSID("$NetBSD: acs.c,v 1.20 2012/04/21 12:27:27 roy Exp $");
#endif				/* not lint */

#include "curses.h"
#include "curses_private.h"

chtype _acs_char[NUM_ACS];
#ifdef HAVE_WCHAR
#include <assert.h>
#include <locale.h>
#include <langinfo.h>
#include <strings.h>

cchar_t _wacs_char[ NUM_ACS ];
#endif /* HAVE_WCHAR */

/*
 * __init_acs --
 *	Fill in the ACS characters.  The 'acs_chars' terminfo entry is a list of
 *	character pairs - ACS definition then terminal representation.
 */
void
__init_acs(SCREEN *screen)
{
	int		count;
	const char	*aofac;	/* Address of 'ac' */
	unsigned char	acs, term;

	/* Default value '+' for all ACS characters */
	for (count=0; count < NUM_ACS; count++)
		_acs_char[count]= '+';

	/* Add the SUSv2 defaults (those that are not '+') */
	ACS_RARROW = '>';
	ACS_LARROW = '<';
	ACS_UARROW = '^';
	ACS_DARROW = 'v';
	ACS_BLOCK = '#';
/*	ACS_DIAMOND = '+';	*/
	ACS_CKBOARD = ':';
	ACS_DEGREE = 39;	/* ' */
	ACS_PLMINUS = '#';
	ACS_BOARD = '#';
	ACS_LANTERN = '#';
/*	ACS_LRCORNER = '+';	*/
/*	ACS_URCORNER = '+';	*/
/*	ACS_ULCORNER = '+';	*/
/*	ACS_LLCORNER = '+';	*/
/*	ACS_PLUS = '+';		*/
	ACS_HLINE = '-';
	ACS_S1 = '-';
	ACS_S9 = '_';
/*	ACS_LTEE = '+';		*/
/*	ACS_RTEE = '+';		*/
/*	ACS_BTEE = '+';		*/
/*	ACS_TTEE = '+';		*/
	ACS_VLINE = '|';
	ACS_BULLET = 'o';
	/* Add the extensions defaults */
	ACS_S3 = '-';
	ACS_S7 = '-';
	ACS_LEQUAL = '<';
	ACS_GEQUAL = '>';
	ACS_PI = '*';
	ACS_NEQUAL = '!';
	ACS_STERLING = 'f';

	if (t_acs_chars(screen->term) == NULL)
		goto out;

	aofac = t_acs_chars(screen->term);

	while (*aofac != '\0') {
		if ((acs = *aofac) == '\0')
			return;
		if ((term = *++aofac) == '\0')
			return;
	 	/* Only add characters 1 to 127 */
		if (acs < NUM_ACS)
			_acs_char[acs] = term | __ALTCHARSET;
		aofac++;
#ifdef DEBUG
		__CTRACE(__CTRACE_INIT, "__init_acs: %c = %c\n", acs, term);
#endif
	}

	if (t_ena_acs(screen->term) != NULL)
		ti_puts(screen->term, t_ena_acs(screen->term), 0,
		    __cputchar_args, screen->outfd);

out:
	for (count=0; count < NUM_ACS; count++)
		screen->acs_char[count]= _acs_char[count];
}

void
_cursesi_reset_acs(SCREEN *screen)
{
	int count;

	for (count=0; count < NUM_ACS; count++)
		_acs_char[count]= screen->acs_char[count];
}

#ifdef HAVE_WCHAR
/*
 * __init_wacs --
 *	Fill in the ACS characters.  The 'acs_chars' terminfo entry is a list of
 *	character pairs - ACS definition then terminal representation.
 */
void
__init_wacs(SCREEN *screen)
{
	int		count;
	const char	*aofac;	/* Address of 'ac' */
	unsigned char	acs, term;
	char	*lstr;

	/* Default value '+' for all ACS characters */
	for (count=0; count < NUM_ACS; count++) {
		_wacs_char[ count ].vals[ 0 ] = ( wchar_t )btowc( '+' );
		_wacs_char[ count ].attributes = 0;
		_wacs_char[ count ].elements = 1;
	}

	/* Add the SUSv2 defaults (those that are not '+') */
	if (!strcmp(setlocale(LC_CTYPE, NULL), "C"))
		setlocale(LC_CTYPE, "");
	lstr = nl_langinfo(CODESET);
	_DIAGASSERT(lstr);
	if (strcasecmp(lstr, "UTF-8")) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INIT, "__init_wacs: setting defaults\n" );
#endif /* DEBUG */
		WACS_RARROW->vals[0]  = ( wchar_t )btowc( '>' );
		WACS_LARROW->vals[0]  = ( wchar_t )btowc( '<' );
		WACS_UARROW->vals[0]  = ( wchar_t )btowc( '^' );
		WACS_DARROW->vals[0]  = ( wchar_t )btowc( 'v' );
		WACS_BLOCK->vals[0]   = ( wchar_t )btowc( '#' );
		WACS_CKBOARD->vals[0] = ( wchar_t )btowc( ':' );
		WACS_DEGREE->vals[0]  = ( wchar_t )btowc( 39 );	/* ' */
		WACS_PLMINUS->vals[0] = ( wchar_t )btowc( '#' );
		WACS_BOARD->vals[0]   = ( wchar_t )btowc( '#' );
		WACS_LANTERN->vals[0] = ( wchar_t )btowc( '#' );
		WACS_HLINE->vals[0]   = ( wchar_t )btowc( '-' );
		WACS_S1->vals[0]      = ( wchar_t )btowc( '-' );
		WACS_S9->vals[0]      = ( wchar_t )btowc( '_' );
		WACS_VLINE->vals[0]   = ( wchar_t )btowc( '|' );
		WACS_BULLET->vals[0]  = ( wchar_t )btowc( 'o' );
		WACS_S3->vals[0]      = ( wchar_t )btowc( 'p' );
		WACS_S7->vals[0]      = ( wchar_t )btowc( 'r' );
		WACS_LEQUAL->vals[0]  = ( wchar_t )btowc( 'y' );
		WACS_GEQUAL->vals[0]  = ( wchar_t )btowc( 'z' );
		WACS_PI->vals[0]      = ( wchar_t )btowc( '{' );
		WACS_NEQUAL->vals[0]  = ( wchar_t )btowc( '|' );
		WACS_STERLING->vals[0]= ( wchar_t )btowc( '}' );
	} else {
		/* Unicode defaults */
#ifdef DEBUG
		__CTRACE(__CTRACE_INIT,
		    "__init_wacs: setting Unicode defaults\n" );
#endif /* DEBUG */
		WACS_RARROW->vals[0]  = 0x2192;
		ACS_RARROW = '+' | __ACS_IS_WACS;
		WACS_LARROW->vals[0]  = 0x2190;
		ACS_LARROW = ',' | __ACS_IS_WACS;
		WACS_UARROW->vals[0]  = 0x2191;
		ACS_UARROW = '-' | __ACS_IS_WACS;
		WACS_DARROW->vals[0]  = 0x2193;
		ACS_DARROW = '.' | __ACS_IS_WACS;
		WACS_BLOCK->vals[0]   = 0x25ae;
		ACS_BLOCK = '0' | __ACS_IS_WACS;
  		WACS_DIAMOND->vals[0] = 0x25c6;
		ACS_DIAMOND = '`' | __ACS_IS_WACS;
		WACS_CKBOARD->vals[0] = 0x2592;
		ACS_CKBOARD = 'a' | __ACS_IS_WACS;
		WACS_DEGREE->vals[0]  = 0x00b0;
		ACS_DEGREE = 'f' | __ACS_IS_WACS;
		WACS_PLMINUS->vals[0] = 0x00b1;
		ACS_PLMINUS = 'g' | __ACS_IS_WACS;
		WACS_BOARD->vals[0]   = 0x2592;
		ACS_BOARD = 'h' | __ACS_IS_WACS;
		WACS_LANTERN->vals[0] = 0x2603;
		ACS_LANTERN = 'i' | __ACS_IS_WACS;
  		WACS_LRCORNER->vals[0]= 0x2518;
		ACS_LRCORNER = 'j' | __ACS_IS_WACS;
  		WACS_URCORNER->vals[0]= 0x2510;
		ACS_URCORNER = 'k' | __ACS_IS_WACS;
  		WACS_ULCORNER->vals[0]= 0x250c;
		ACS_ULCORNER = 'l' | __ACS_IS_WACS;
  		WACS_LLCORNER->vals[0]= 0x2514;
		ACS_LLCORNER = 'm' | __ACS_IS_WACS;
  		WACS_PLUS->vals[0]    = 0x253c;
		ACS_PLUS = 'n' | __ACS_IS_WACS;
		WACS_HLINE->vals[0]   = 0x2500;
		ACS_HLINE = 'q' | __ACS_IS_WACS;
		WACS_S1->vals[0]      = 0x23ba;
		ACS_S1 = 'o' | __ACS_IS_WACS;
		WACS_S9->vals[0]      = 0x23bd;
		ACS_S9 = 's' | __ACS_IS_WACS;
  		WACS_LTEE->vals[0]    = 0x251c;
		ACS_LTEE = 't' | __ACS_IS_WACS;
  		WACS_RTEE->vals[0]    = 0x2524;
		ACS_RTEE = 'u' | __ACS_IS_WACS;
  		WACS_BTEE->vals[0]    = 0x2534;
		ACS_BTEE = 'v' | __ACS_IS_WACS;
  		WACS_TTEE->vals[0]    = 0x252c;
		ACS_TTEE = 'w' | __ACS_IS_WACS;
		WACS_VLINE->vals[0]   = 0x2502;
		ACS_VLINE = 'x' | __ACS_IS_WACS;
		WACS_BULLET->vals[0]  = 0x00b7;
		ACS_BULLET = '~' | __ACS_IS_WACS;
		WACS_S3->vals[0]      = 0x23bb;
		ACS_S3 = 'p' | __ACS_IS_WACS;
		WACS_S7->vals[0]      = 0x23bc;
		ACS_S7 = 'r' | __ACS_IS_WACS;
		WACS_LEQUAL->vals[0]  = 0x2264;
		ACS_LEQUAL = 'y' | __ACS_IS_WACS;
		WACS_GEQUAL->vals[0]  = 0x2265;
		ACS_GEQUAL = 'z' | __ACS_IS_WACS;
		WACS_PI->vals[0]      = 0x03C0;
		ACS_PI = '{' | __ACS_IS_WACS;
		WACS_NEQUAL->vals[0]  = 0x2260;
		ACS_NEQUAL = '|' | __ACS_IS_WACS;
		WACS_STERLING->vals[0]= 0x00A3;
		ACS_STERLING = '}' | __ACS_IS_WACS;
	}

	if (t_acs_chars(screen->term) == NULL) {
#ifdef DEBUG
		__CTRACE(__CTRACE_INIT,
		    "__init_wacs: no alternative characters\n" );
#endif /* DEBUG */
		goto out;
	}

	aofac = t_acs_chars(screen->term);

	while (*aofac != '\0') {
		if ((acs = *aofac) == '\0')
			return;
		if ((term = *++aofac) == '\0')
			return;
	 	/* Only add characters 1 to 127 */
		if (acs < NUM_ACS) {
			_wacs_char[acs].vals[ 0 ] = term;
			_wacs_char[acs].attributes |= WA_ALTCHARSET;
		}
		aofac++;
#ifdef DEBUG
		__CTRACE(__CTRACE_INIT, "__init_wacs: %c = %c\n", acs, term);
#endif
	}

	if (t_ena_acs(screen->term) != NULL)
		ti_puts(screen->term, t_ena_acs(screen->term), 0,
			   __cputchar_args, screen->outfd);

out:
	for (count=0; count < NUM_ACS; count++) {
		memcpy(&screen->wacs_char[count], &_wacs_char[count],
			sizeof(cchar_t));
		screen->acs_char[count]= _acs_char[count];
	}
}

void
_cursesi_reset_wacs(SCREEN *screen)
{
	int count;

	for (count=0; count < NUM_ACS; count++)
		memcpy( &_wacs_char[count], &screen->wacs_char[count],
			sizeof( cchar_t ));
}
#endif /* HAVE_WCHAR */
