/*	$NetBSD: calc2.y,v 1.1.1.3 2011/09/10 21:22:06 christos Exp $	*/

%parse-param { int regs[26] }
%parse-param { int *base }

%lex-param { int *base }

%{
# include <stdio.h>
# include <ctype.h>

%}

%start list

%token DIGIT LETTER

%left '|'
%left '&'
%left '+' '-'
%left '*' '/' '%'
%left UMINUS   /* supplies precedence for unary minus */

%% /* beginning of rules section */

list  :  /* empty */
      |  list stat '\n'
      |  list error '\n'
            {  yyerrok ; }
      ;

stat  :  expr
            {  printf("%d\n",$1);}
      |  LETTER '=' expr
            {  regs[$1] = $3; }
      ;

expr  :  '(' expr ')'
            {  $$ = $2; }
      |  expr '+' expr
            {  $$ = $1 + $3; }
      |  expr '-' expr
            {  $$ = $1 - $3; }
      |  expr '*' expr
            {  $$ = $1 * $3; }
      |  expr '/' expr
            {  $$ = $1 / $3; }
      |  expr '%' expr
            {  $$ = $1 % $3; }
      |  expr '&' expr
            {  $$ = $1 & $3; }
      |  expr '|' expr
            {  $$ = $1 | $3; }
      |  '-' expr %prec UMINUS
            {  $$ = - $2; }
      |  LETTER
            {  $$ = regs[$1]; }
      |  number
      ;

number:  DIGIT
         {  $$ = $1; (*base) = ($1==0) ? 8 : 10; }
      |  number DIGIT
         {  $$ = (*base) * $1 + $2; }
      ;

%% /* start of programs */

#ifdef YYBYACC
extern int YYLEX_DECL();
static void YYERROR_DECL();
#endif

int
main (void)
{
    int regs[26];
    int base = 10;

    while(!feof(stdin)) {
	yyparse(regs, &base);
    }
    return 0;
}

static void
YYERROR_DECL()
{
    fprintf(stderr, "%s\n", s);
}

int
yylex(int *base)
{
	/* lexical analysis routine */
	/* returns LETTER for a lower case letter, yylval = 0 through 25 */
	/* return DIGIT for a digit, yylval = 0 through 9 */
	/* all other characters are returned immediately */

    int c;

    while( (c=getchar()) == ' ' )   { /* skip blanks */ }

    /* c is now nonblank */

    if( islower( c )) {
	yylval = c - 'a';
	return ( LETTER );
    }
    if( isdigit( c )) {
	yylval = (c - '0') % (*base);
	return ( DIGIT );
    }
    return( c );
}
