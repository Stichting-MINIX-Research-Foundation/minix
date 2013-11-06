/*	$NetBSD: msgscan.l,v 1.7 2012/03/06 16:26:01 mbalmer Exp $	*/

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

%{
/* scan.l: scanner description for message compiler. */

#include <sys/cdefs.h>

#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: msgscan.l,v 1.7 2012/03/06 16:26:01 mbalmer Exp $");
#endif


#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "msgparse.h"

static int level; 	/* For nested comments. */

%}

%x COMMENT
%x BRACE

%option noinput

%%

[ \t]+	{ /* ignore spaces and tabs */ }

[\n]	{ line_no++; }

";"	{ return (int)yytext[0]; }

message { return MESSAGE; }

[a-zA-Z][a-zA-Z0-9_]* {
	  yylval.s_value = strdup(yytext);
	  return(NAME);
	}

"/*"  {	level = 1; BEGIN COMMENT;  }

<COMMENT>"/*" { level++; }

<COMMENT>"*/" { if (level-- == 1) BEGIN 0; }

<COMMENT>"\n" { line_no++; }

<COMMENT><<EOF>> { yyerror ("EOF inside a comment."); exit (1); }

<COMMENT>.  {/* eat character */}

"{"	{ level = 1; BEGIN BRACE; }

<BRACE>"\\{"	{ buff_add_ch('{'); }

<BRACE>"\\}"	{ buff_add_ch('}'); }

<BRACE>"{"	{ buff_add_ch(yytext[0]); level++; }

<BRACE>"}"	{ if (level-- == 1)  {
			BEGIN 0;
			yylval.s_value = buff_copy();
			return VALUE;
		  } else
			buff_add_ch (yytext[0]);
		}

<BRACE>"\n"	{ buff_add_ch (yytext[0]); line_no++; }

<BRACE>.	{ buff_add_ch (yytext[0]); }

.       {
	  if (yytext[0] < ' ')
	    yyerror ("illegal character: ^%c",yytext[0] + '@');
	  else
	    if (yytext[0] > '~')
	      yyerror ("illegal character: \\%3d", (int) yytext[0]);
	    else
	      yyerror ("illegal character: %s",yytext);

	  /* To quiet the compiler */
	  if (0) unput(0);
	}
%%

#ifdef SCAN
YYSTYPE yylval;

main()
{
  int val;

  line_no = 1;
  while ( (val = yylex()) != 0 )
    printf ("val = %d\n yytext = %s\n", val, yytext);
}
#endif
