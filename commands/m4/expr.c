/*
 *      expression evaluator: performs a standard recursive
 *      descent parse to evaluate any expression permissible
 *      within the following grammar:
 *
 *      expr    :       query EOS
 *      query   :       lor
 *              |       lor "?" query ":" query
 *      lor     :       land { "||" land }
 *      land    :       bor { "&&" bor }
 *      bor     :       bxor { "|" bxor }
 *      bxor    :       band { "^" band }
 *      band    :       eql { "&" eql }
 *      eql     :       relat { eqrel relat }
 *      relat   :       shift { rel shift }
 *      shift   :       primary { shop primary }
 *      primary :       term { addop term }
 *      term    :       unary { mulop unary }
 *      unary   :       factor
 *              |       unop unary
 *      factor  :       constant
 *              |       "(" query ")"
 *      constant:       num
 *              |       "'" CHAR "'"
 *      num     :       decnum
 *              |       "0" octnum
 *		|	"0x" hexnum
 *	octnum	:	OCTDIGIT
 *		|	OCTDIGIT octnum
 *	decnum	:	DECDIGIT
 *		|	DECDIGIT decnum
 *	hexnum	:	HEXDIGIT
 *		|	HEXDIGIT hexnum
 *      shop    :       "<<"
 *              |       ">>"
 *      eqlrel  :       "="
 *              |       "=="
 *              |       "!="
 *      rel     :       "<"
 *              |       ">"
 *              |       "<="
 *              |       ">="
 *
 *
 *      This expression evaluator is lifted from a public-domain
 *      C Pre-Processor included with the DECUS C Compiler distribution.
 *      It is hacked somewhat to be suitable for m4.
 *
 *      Originally by:  Mike Lutz
 *                      Bob Harper
 */
 
#include "mdef.h"

#define TRUE    1
#define FALSE   0
#define EOS     (char) 0
#define EQL     0
#define NEQ     1
#define LSS     2
#define LEQ     3
#define GTR     4
#define GEQ     5
 
static char *nxtch;     /* Parser scan pointer */
 
/*
 * For longjmp
 */
#include <setjmp.h>
static jmp_buf  expjump;
 
/*
 * macros:
 *
 *      ungetch - Put back the last character examined.
 *      getch   - return the next character from expr string.
 */
#define ungetch()       nxtch--
#define getch()         *nxtch++
 
int expr(expbuf)
char *expbuf;
{
        register int rval;
 
        nxtch = expbuf;
        if (setjmp(expjump) != 0)
                return (FALSE);
        rval = query();
        if (skipws() == EOS)
                return(rval);
        experr("Ill-formed expression");
	/* NOTREACHED */
	return(0);
}
 
/*
 * query : lor | lor '?' query ':' query
 *
 */
int query()
{
        register int bool, true_val, false_val;
 
        bool = lor();
        if (skipws() != '?') {
                ungetch();
                return(bool);
        }
 
        true_val = query();
        if (skipws() != ':')
                experr("Bad query");
 
        false_val = query();
        return(bool ? true_val : false_val);
}
 
/*
 * lor : land { '||' land }
 *
 */
int lor()
{
        register int c, vl, vr;
 
        vl = land();
        while ((c = skipws()) == '|' && getch() == '|') {
                vr = land();
                vl = vl || vr;
        }
 
        if (c == '|')
                ungetch();
        ungetch();
        return(vl);
}
 
/*
 * land : bor { '&&' bor }
 *
 */
int land()
{
        register int c, vl, vr;
 
        vl = bor();
        while ((c = skipws()) == '&' && getch() == '&') {
                vr = bor();
                vl = vl && vr;
        }
 
        if (c == '&')
                ungetch();
        ungetch();
        return(vl);
}
 
/*
 * bor : bxor { '|' bxor }
 *
 */
int bor()
{
        register int vl, vr, c;
 
        vl = bxor();
        while ((c = skipws()) == '|' && getch() != '|') {
                ungetch();
                vr = bxor();
                vl |= vr;
        }
 
        if (c == '|')
                ungetch();
        ungetch();
        return(vl);
}
 
/*
 * bxor : band { '^' band }
 *
 */
int bxor()
{
        register int vl, vr;
 
        vl = band();
        while (skipws() == '^') {
                vr = band();
                vl ^= vr;
        }
 
        ungetch();
        return(vl);
}
 
/*
 * band : eql { '&' eql }
 *
 */
int band()
{
        register int vl, vr, c;
 
        vl = eql();
        while ((c = skipws()) == '&' && getch() != '&') {
                ungetch();
                vr = eql();
                vl &= vr;
        }
 
        if (c == '&')
                ungetch();
        ungetch();
        return(vl);
}
 
/*
 * eql : relat { eqrel relat }
 *
 */
int eql()
{
        register int vl, vr, rel;
 
        vl = relat();
        while ((rel = geteql()) != -1) {
                vr = relat();
 
                switch (rel) {
 
                case EQL:
                        vl = (vl == vr);
                        break;
                case NEQ:
                        vl = (vl != vr);
                        break;
                }
        }
        return(vl);
}
 
/*
 * relat : shift { rel shift }
 *
 */
int relat()
{
        register int vl, vr, rel;
 
        vl = shift();
        while ((rel = getrel()) != -1) {
 
                vr = shift();
                switch (rel) {
 
                case LEQ:
                        vl = (vl <= vr);
                        break;
                case LSS:
                        vl = (vl < vr);
                        break;
                case GTR:
                        vl = (vl > vr);
                        break;
                case GEQ:
                        vl = (vl >= vr);
                        break;
                }
        }
        return(vl);
}
 
/*
 * shift : primary { shop primary }
 *
 */
int shift()
{
        register int vl, vr, c;
 
        vl = primary();
        while (((c = skipws()) == '<' || c == '>') && c == getch()) {
                vr = primary();
 
                if (c == '<')
                        vl <<= vr;
                else
                        vl >>= vr;
        }
 
        if (c == '<' || c == '>')
                ungetch();
        ungetch();
        return(vl);
}
 
/*
 * primary : term { addop term }
 *
 */
int primary()
{
        register int c, vl, vr;
 
        vl = term();
        while ((c = skipws()) == '+' || c == '-') {
                vr = term();
                if (c == '+')
                        vl += vr;
                else
                        vl -= vr;
        }
 
        ungetch();
        return(vl);
}
 
/*
 * <term> := <unary> { <mulop> <unary> }
 *
 */
int term()
{
        register int c, vl, vr;
 
        vl = unary();
        while ((c = skipws()) == '*' || c == '/' || c == '%') {
                vr = unary();
 
                switch (c) {
                case '*':
                        vl *= vr;
                        break;
                case '/':
                        vl /= vr;
                        break;
                case '%':
                        vl %= vr;
                        break;
                }
        }
        ungetch();
        return(vl);
}
 
/*
 * unary : factor | unop unary
 *
 */
int unary()
{
        register int val, c;
 
        if ((c = skipws()) == '!' || c == '~' || c == '-') {
                val = unary();
 
                switch (c) {
                case '!':
                        return(! val);
                case '~':
                        return(~ val);
                case '-':
                        return(- val);
                }
        }
 
        ungetch();
        return(factor());
}
 
/*
 * factor : constant | '(' query ')'
 *
 */
int factor()
{
        register int val;
 
        if (skipws() == '(') {
                val = query();
                if (skipws() != ')')
                        experr("Bad factor");
                return(val);
        }
 
        ungetch();
        return(constant());
}
 
/*
 * constant: num | 'char'
 *
 */
int constant()
{
        /*
         * Note: constant() handles multi-byte constants
         */
 
        register int    i;
        register int    value;
        register char   c;
        int             v[sizeof (int)];
 
        if (skipws() != '\'') {
                ungetch();
                return(num());
        }
        for (i = 0; i < sizeof(int); i++) {
                if ((c = getch()) == '\'') {
                        ungetch();
                        break;
                }
                if (c == '\\') {
                        switch (c = getch()) {
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                                ungetch();
                                c = num();
                                break;
                        case 'n':
                                c = 012;
                                break;
                        case 'r':
                                c = 015;
                                break;
                        case 't':
                                c = 011;
                                break;
                        case 'b':
                                c = 010;
                                break;
                        case 'f':
                                c = 014;
                                break;
                        }
                }
                v[i] = c;
        }
        if (i == 0 || getch() != '\'')
                experr("Illegal character constant");
        for (value = 0; --i >= 0;) {
                value <<= 8;
                value += v[i];
        }
        return(value);
}
 
/*
 * num : digit | num digit
 *
 */
int num()
{
        register int rval, c, base;
        int ndig;
 
        ndig = 0;
        if ((c = skipws()) == '0') {
        	c = getch ();
        	if (c == 'x' || c == 'X') {
        		base = 16;
        		c = getch ();
        	} else {
        		base = 8;
        		ndig = 1;
        	}
        } else {
        	base = 10;
        }
        rval = 0;
        for (;;) {
		if (isdigit(c))         c -= '0';
		else if (isupper (c))   c -= ('A' - 10);
		else if (islower (c))   c -= ('a' - 10);
		else                    break;
		if (c < 0 || c >= base)
			break;

                rval *= base;
                rval += c;
                c = getch();
                ndig++;
        }
        ungetch();
        if (ndig)
                return(rval);
        experr("Bad constant");
	/* NOTREACHED */
	return(0);
}
 
/*
 * eqlrel : '=' | '==' | '!='
 *
 */
int geteql()
{
        register int c1, c2;
 
        c1 = skipws();
        c2 = getch();
 
        switch (c1) {
 
        case '=':
                if (c2 != '=')
                        ungetch();
                return(EQL);
 
        case '!':
                if (c2 == '=')
                        return(NEQ);
                ungetch();
                ungetch();
                return(-1);
 
        default:
                ungetch();
                ungetch();
                return(-1);
        }
}
 
/*
 * rel : '<' | '>' | '<=' | '>='
 *
 */
int getrel()
{
        register int c1, c2;
 
        c1 = skipws();
        c2 = getch();
 
        switch (c1) {
 
        case '<':
                if (c2 == '=')
                        return(LEQ);
                ungetch();
                return(LSS);
 
        case '>':
                if (c2 == '=')
                        return(GEQ);
                ungetch();
                return(GTR);
 
        default:
                ungetch();
                ungetch();
                return(-1);
        }
}
 
/*
 * Skip over any white space and return terminating char.
 */
int skipws()
{
        register char c;
 
        while ((c = getch()) <= ' ' && c > EOS)
                ;
        return(c);
}
 
/*
 * Error handler - resets environment to eval(), prints an error,
 * and returns FALSE.
 */
int experr(msg)
char *msg;
{
        printf("mp: %s\n",msg);
        longjmp(expjump, -1);          /* Force eval() to return FALSE */
}
