/* libmain - flex run-time support library "main" function */

/* $Header$ */

extern int yylex(void);

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
