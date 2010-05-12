/* libmain - flex run-time support library "main" function */

/* $Header$ */

#include "flexdef.h"

extern int yylex(void);

int main PROTO((int, char**));

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
