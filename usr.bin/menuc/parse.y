/*	$NetBSD: parse.y,v 1.16 2012/03/06 16:55:18 mbalmer Exp $	*/

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

#include <stdio.h>
#include "defs.h"

static id_rec *cur_menu;
static optn_info *cur_optn;

%}

%union {
	char *s_value;
	int   i_value;
	optn_info *optn_value;
	action a_value;
}


%token <i_value> X Y W H NO BOX SUB HELP MENU NEXT EXIT ACTION ENDWIN OPTION 
%token <i_value> TITLE DEFAULT DISPLAY ERROR EXITSTRING ALLOW DYNAMIC MENUS
		 SCROLLABLE SHORTCUT CLEAR MESSAGES ALWAYS SCROLL
%token <s_value> STRING NAME CODE INT_CONST CHAR_CONST

%type <s_value> init_code system helpstr text
%type <optn_value> option option_list
%type <i_value> act_opt
%type <a_value> action exitact

%start system

%%

system : init_code menu_list
	 { check_defined();
	   if (!had_errors)
		   write_menu_file($1);
	 }
       ;

init_code : /* empty */ { $$ = ""; }
	  | CODE
	  ;

menu_list :  /* empty */
	  |  menu_list menu_def
	  |  menu_list default_def
	  |  menu_list initerror_def
	  |  menu_list dynamic_def
	  |  menu_list msgxlat_def
	  ;

dynamic_def : ALLOW DYNAMIC MENUS ';'
		{ do_dynamic = 1; }

msgxlat_def : ALLOW DYNAMIC MESSAGES ';'
		{ do_msgxlat = 1; }

initerror_def : ERROR action ';'
		{ error_act = $2; }

default_def : DEFAULT
		{ cur_menu = &default_menu; }
	      opt opt_list ";"

menu_def  :  MENU NAME
		{ cur_menu = get_menu ($2);
		  if (cur_menu->info != NULL)
			  yyerror ("Menu %s defined twice", $2);
		  else {
			  cur_menu->info =
				  (menu_info *) malloc (sizeof (menu_info));
			  *(cur_menu->info) = default_info;
		  }
		}
	     opts ";" dispact option_list exitact helpstr
		{ optn_info *t;
		  cur_menu->info->optns = NULL;
		  while ($7 != NULL) {
			  t = $7;
			  $7 = $7->next;
			  t->next = cur_menu->info->optns;
			  cur_menu->info->optns = t;
			  cur_menu->info->numopt++;
		  }
		}
	  ;

opts	  : /* empty */
	  | opt_list
	  ;

opt_list  : "," opt
	  | opt_list "," opt
	  ;

text	  : NAME | STRING

opt	  : NO EXIT		{ cur_menu->info->mopt |= MC_NOEXITOPT; }
	  | EXIT		{ cur_menu->info->mopt &= ~MC_NOEXITOPT; }
	  | NO BOX		{ cur_menu->info->mopt |= MC_NOBOX; }
	  | BOX			{ cur_menu->info->mopt &= ~MC_NOBOX; }
	  | NO SCROLLABLE	{ cur_menu->info->mopt &= ~MC_SCROLL; }
	  | SCROLLABLE		{ cur_menu->info->mopt |= MC_SCROLL; }
	  | NO SHORTCUT 	{ cur_menu->info->mopt |= MC_NOSHORTCUT; }
	  | SHORTCUT 		{ cur_menu->info->mopt &= ~MC_NOSHORTCUT; }
	  | NO CLEAR 		{ cur_menu->info->mopt |= MC_NOCLEAR; }
	  | CLEAR 		{ cur_menu->info->mopt &= ~MC_NOCLEAR; }
	  | NO DEFAULT EXIT	{ cur_menu->info->mopt &= ~MC_DFLTEXIT; }
	  | DEFAULT EXIT 	{ cur_menu->info->mopt |= MC_DFLTEXIT; }
	  | NO ALWAYS SCROLL	{ cur_menu->info->mopt &= ~MC_ALWAYS_SCROLL; }
	  | ALWAYS SCROLL 	{ cur_menu->info->mopt |= MC_ALWAYS_SCROLL; }
	  | NO SUB MENU		{ cur_menu->info->mopt &= ~MC_SUBMENU; }
	  | SUB MENU 		{ cur_menu->info->mopt |= MC_SUBMENU; }
	  | X "=" INT_CONST	{ cur_menu->info->x = atoi($3); }
	  | Y "=" INT_CONST	{ cur_menu->info->y = atoi($3); }
	  | W "=" INT_CONST	{ cur_menu->info->w = atoi($3); }
	  | H "=" INT_CONST	{ cur_menu->info->h = atoi($3); }
	  | TITLE text	 	{ cur_menu->info->title = $2; }
	  | EXITSTRING text	{ cur_menu->info->exitstr = $2;
				  cur_menu->info->mopt &= ~MC_NOEXITOPT; }
	  ;

option_list : option
	  | option_list option  { $2->next = $1; $$ = $2; }
	  ;

option	  : OPTION
		{ cur_optn = (optn_info *) malloc (sizeof(optn_info));
		  cur_optn->menu = -1;
		  cur_optn->name = NULL;
		  cur_optn->name_is_code = FALSE;
		  cur_optn->issub = FALSE;
		  cur_optn->doexit = FALSE;
		  cur_optn->optact.code = "";
		  cur_optn->optact.endwin = FALSE;
		  cur_optn->next = NULL;
		}
	    option_legend ","
	    elem_list ";"
		{ $$ = cur_optn; }
	  ;

option_legend : text	{ cur_optn->name = $1; }
	  | CODE	{ cur_optn->name = $1; cur_optn->name_is_code = TRUE;}

elem_list : elem
	  | elem_list "," elem
	  ;

elem	  : NEXT MENU NAME
		{ id_rec *t = get_menu ($3);
		  if (cur_optn->menu != -1)
			  yyerror ("Double sub/next menu definition");
		  else {
			  cur_optn->menu = t->menu_no;
		  }
		}
	  | SUB MENU NAME
		{ id_rec *t = get_menu ($3);
		  if (cur_optn->menu != -1)
			  yyerror ("Double sub/next menu definition");
		  else {
			  cur_optn->menu = t->menu_no;
			  cur_optn->issub = TRUE;
		  }
		}
	  | action	{ cur_optn->optact = $1; }
	  | EXIT	{ cur_optn->doexit = TRUE; }
	  ;

action	  : ACTION act_opt CODE
		{ $$.code = $3;
		  $$.endwin = $2;
		}
	  ;

act_opt	  : /* empty */		{ $$ = 0; }
	  | "(" ENDWIN ")"	{ $$ = 1; }
	  ;

dispact	  : /* empty */ 	{ cur_menu->info->postact.code = ""; }
	  | DISPLAY action ";"	{ cur_menu->info->postact = $2; }
	  ;


exitact	  : /* empty */ 	{ cur_menu->info->exitact.code = ""; }
	  | EXIT action	";"	{ cur_menu->info->exitact = $2; }
	  ;

helpstr	  : /* empty */ 	{ cur_menu->info->helpstr = NULL; }
	  | HELP CODE ";" 	{ asprintf(&cur_menu->info->helpstr, "\"%s\"", $2); } 
	  | HELP text ";" 	{ cur_menu->info->helpstr = $2; } 
	  ;
