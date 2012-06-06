/*	$NetBSD: pure_error.y,v 1.1.1.3 2011/09/10 21:22:05 christos Exp $	*/

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
yylex(YYSTYPE *value)
{
    return value ? 0 : -1;
}

static void
yyerror(const char* s)
{
    printf("%s\n", s);
}
