/*	$NetBSD: scan.l,v 1.16 2012/03/06 16:55:18 mbalmer Exp $	*/

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
/* scan.l: scanner description for menu compiler. */

#include <stdio.h>
#include <string.h>
#include "defs.h"
#include "parse.h"

static int level; 	/* For nested comments. */
static int comstart;	/* line number of comment start. */

%}

%x COMMENT
%x BRACE

%option noinput

%%

[ \t]+	{ /* ignore spaces and tabs */ }

[\n]	{ line_no++; }

"="|";"|","|"("|")"	{ return (int)yytext[0]; }

x	{ return X; }

y	{ return Y; }

w	{ return W; }

h	{ return H; }

no	{ return NO; }

box	{ return BOX; }

sub	{ return SUB; }

help	{ return HELP; }

menu	{ return MENU; }

menus	{ return MENUS; }

next	{ return NEXT; }

exit	{ return  EXIT; }

exitstring { return  EXITSTRING; }

title	{ return TITLE; }

action	{ return ACTION; }

endwin	{ return ENDWIN; }

option	{ return OPTION; }

default { return DEFAULT; }

display { return DISPLAY; }

error { return ERROR; }

allow { return ALLOW; }

dynamic { return DYNAMIC; }

messages { return MESSAGES; }

scrollable { return SCROLLABLE; }

shortcut { return SHORTCUT; }

clear { return CLEAR; }

always { return ALWAYS; }

scroll { return SCROLL; }

\"([^\"\n]*(\\\")?)*\"  {
	  yylval.s_value = strdup (yytext);
	  max_strlen = max_strlen > strlen(yytext)
		       ? max_strlen : strlen(yytext) + 1;
	  return STRING;
	}

[a-zA-Z][a-zA-Z0-9_]* {
	  yylval.s_value = strdup(yytext);
	  return(NAME);
	}

0|[-1-9][0-9]* {
	  yylval.s_value = strdup(yytext); 
	  return(INT_CONST);
	}

"'"[^'\\]|(\\[athrn])|(\\[0-9][0-9]?[0-9]?)"'" {
	  yylval.s_value = strdup(yytext);
	  return(CHAR_CONST);
	}

"/*"  {	level = 1; comstart = line_no; BEGIN COMMENT; }

<COMMENT>"/*" { level++; }

<COMMENT>"*/" { if (level-- == 1) BEGIN 0; }

<COMMENT>"\n" { line_no++; }

<COMMENT><<EOF>> {
		yyerror ("EOF inside a comment that started at line %d",
			 comstart);
		exit (1);
	}

<COMMENT>.  {/* eat character */}

"{"	{ level = 1; BEGIN BRACE; }

<BRACE>"{"	{ buff_add_ch(yytext[0]); level++; }

<BRACE>"}"	{ if (level-- == 1)  {
			BEGIN 0;
			yylval.s_value = buff_copy();
			return CODE;
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
