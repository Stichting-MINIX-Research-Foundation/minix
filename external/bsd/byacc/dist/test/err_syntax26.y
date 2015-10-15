/*	$NetBSD: err_syntax26.y,v 1.1.1.1 2015/01/03 22:58:23 christos Exp $	*/

%{
int yylex(void);
static void yyerror(const char *);
%}

%type <tag2
