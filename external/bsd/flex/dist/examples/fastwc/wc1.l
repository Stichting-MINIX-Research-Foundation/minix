/*	$NetBSD: wc1.l,v 1.1.1.1 2009/10/26 00:28:34 christos Exp $	*/

/* First cut at a flex-based "wc" tool. */

ws    [ \t]
nonws [^ \t\n]

%option main noyywrap
%%
	int cc = 0, wc = 0, lc = 0;

{nonws}+	cc += yyleng; ++wc;

{ws}+		cc += yyleng;

\n		++lc; ++cc;

<<EOF>>		{
		printf( "%8d %8d %8d\n", lc, wc, cc );
		yyterminate();
		}
