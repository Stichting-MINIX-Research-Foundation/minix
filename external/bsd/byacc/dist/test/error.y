/*	$NetBSD: error.y,v 1.1.1.4 2011/09/10 21:22:03 christos Exp $	*/

%%
S: error
%%

#include <stdio.h>

#ifdef YYBYACC
extern int YYLEX_DECL();
static void YYERROR_DECL();
#endif

int
main(void)
{
    printf("yyparse() = %d\n", yyparse());
    return 0;
}

int
yylex(void)
{
    return -1;
}

static void
yyerror(const char* s)
{
    printf("%s\n", s);
}
