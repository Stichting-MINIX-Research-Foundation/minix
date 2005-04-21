/*
 * regcomp and regexec -- regsub and regerror are elsewhere
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 *
 *	Modified by K.Hirabayashi to accept KANJI code, memory allocation
 *	and to add following functions.
 *		isthere(), mkpat(), match(), regsub(), sjtok(), ktosj(),
 *		Strchr(), Strncmp(), Strlen()
 */

#include <stdio.h>
#include <ctype.h>
#include "regexp.h"

#define regerror(a)	error("regular expression error: %s", a)

int r_start, r_length;

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	MAGIC	0234

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	char that must begin a match; '\0' if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 * regmlen	length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
 * it anyway.
 */

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* definition	number	opnd?	meaning */
#define	END	0	/* no	End of program. */
#define	BOL	1	/* no	Match "" at beginning of line. */
#define	EOL	2	/* no	Match "" at end of line. */
#define	ANY	3	/* no	Match any one character. */
#define	ANYOF	4	/* str	Match any character in this string. */
#define	ANYBUT	5	/* str	Match any character not in this string. */
#define	BRANCH	6	/* node	Match this alternative, or the next... */
#define	BACK	7	/* no	Match "", "next" ptr points backward. */
#define	EXACTLY	8	/* str	Match this string. */
#define	NOTHING	9	/* no	Match empty string. */
#define	STAR	10	/* node	Match this (simple) thing 0 or more times. */
#define	PLUS	11	/* node	Match this (simple) thing 1 or more times. */
#define	OPEN	20	/* no	Mark this point in input as start of #n. */
		/*	OPEN+1 is number 1, etc. */
#define	CLOSE	30	/* no	Analogous to OPEN. */

/*
 * Opcode notes:
 *
 * BRANCH	The set of branches constituting a single choice are hooked
 *		together with their "next" pointers, since precedence prevents
 *		anything being concatenated to any individual branch.  The
 *		"next" pointer of the last BRANCH in a choice points to the
 *		thing following the whole choice.  This is also where the
 *		final "next" pointer of each individual branch points; each
 *		branch starts with the operand node of a BRANCH node.
 *
 * BACK		Normal "next" pointers all implicitly point forward; BACK
 *		exists to make loop structures possible.
 *
 * STAR,PLUS	'?', and complex '*' and '+', are implemented as circular
 *		BRANCH structures using BACK.  Simple cases (one character
 *		per match) are implemented with STAR and PLUS for speed
 *		and to minimize recursive plunges.
 *
 * OPEN,CLOSE	...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define	OP(p)	(*(p))
#define	NEXT(p)	(((*((p)+1)&0377)<<8) + *((p)+2)&0377)
#define	OPERAND(p)	((p) + 3)



/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define	UCHARAT(p)	((int)*(ushort *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARBITS)
#endif

#define	FAIL(m)	{ regerror(m); return(NULL); }
#define	ISMULT(c)	((c) == '*' || (c) == '+' || (c) == '?')
#define	META	"^$.[()|?+*\\"

/*
 * Flags to be passed up and down.
 */
#define	HASWIDTH	01	/* Known never to match null string. */
#define	SIMPLE		02	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		04	/* Starts with * or +. */
#define	WORST		0	/* Worst case. */

/*
 * Global work variables for regcomp().
 */
static ushort *regparse;		/* Input-scan pointer. */
static int regnpar;		/* () count. */
static ushort regdummy;
static ushort *regcode;		/* Code-emit pointer; &regdummy = don't. */
static long regsize;		/* Code size. */

/*
 * Forward declarations for regcomp()'s friends.
 */
#ifndef STATIC
#define	STATIC	static
#endif
STATIC ushort *reg();
STATIC ushort *regbranch();
STATIC ushort *regpiece();
STATIC ushort *regatom();
STATIC ushort *regnode();
STATIC ushort *regnext();
STATIC void regc();
STATIC void reginsert();
STATIC void regtail();
STATIC void regoptail();
STATIC int Strcspn();

/*
 - regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
regexp *
regcomp(exp)
ushort *exp;
{
  register regexp *r;
  register ushort *scan;
  register ushort *longest;
  register int len;
  int flags;
  extern char *emalloc();

  if (exp == NULL)
	FAIL("NULL argument");

  /* First pass: determine size, legality. */
  regparse = exp;
  regnpar = 1;
  regsize = 0L;
  regcode = &regdummy;
  regc((ushort) MAGIC);
  if (reg(0, &flags) == NULL)
	return(NULL);

  /* Small enough for pointer-storage convention? */
  if (regsize >= 32767L)		/* Probably could be 65535L. */
	FAIL("regexp too big");

  /* Allocate space. */
  r = (regexp *)emalloc(sizeof(regexp) + (unsigned)regsize * sizeof(ushort));

  /* Second pass: emit code. */
  regparse = exp;
  regnpar = 1;
  regcode = r->program;
  regc((ushort) MAGIC);
  if (reg(0, &flags) == NULL)
	return(NULL);

  /* Dig out information for optimizations. */
  r->regstart = '\0';	/* Worst-case defaults. */
  r->reganch = 0;
  r->regmust = NULL;
  r->regmlen = 0;
  scan = r->program+1;			/* First BRANCH. */
  if (OP(regnext(scan)) == END) {		/* Only one top-level choice. */
	scan = OPERAND(scan);

	/* Starting-point info. */
	if (OP(scan) == EXACTLY)
		r->regstart = *OPERAND(scan);
	else if (OP(scan) == BOL)
		r->reganch++;

	/*
	 * If there's something expensive in the r.e., find the
	 * longest literal string that must appear and make it the
	 * regmust.  Resolve ties in favor of later strings, since
	 * the regstart check works with the beginning of the r.e.
	 * and avoiding duplication strengthens checking.  Not a
	 * strong reason, but sufficient in the absence of others.
	 */
	if (flags&SPSTART) {
		longest = NULL;
		len = 0;
		for (; scan != NULL; scan = regnext(scan))
			if (OP(scan) == EXACTLY && Strlen(OPERAND(scan)) >= len) {
				longest = OPERAND(scan);
				len = Strlen(OPERAND(scan));
			}
		r->regmust = longest;
		r->regmlen = len;
	}
  }

  return(r);
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static ushort *
reg(paren, flagp)
int paren;			/* Parenthesized? */
int *flagp;
{
  register ushort *ret;
  register ushort *br;
  register ushort *ender;
  register int parno;
  int flags;

  *flagp = HASWIDTH;	/* Tentatively. */

  /* Make an OPEN node, if parenthesized. */
  if (paren) {
	if (regnpar >= NSUBEXP)
		FAIL("too many ()");
	parno = regnpar;
	regnpar++;
	ret = regnode(OPEN+parno);
  } else
	ret = NULL;

  /* Pick up the branches, linking them together. */
  br = regbranch(&flags);
  if (br == NULL)
	return(NULL);
  if (ret != NULL)
	regtail(ret, br);	/* OPEN -> first. */
  else
	ret = br;
  if (!(flags&HASWIDTH))
	*flagp &= ~HASWIDTH;
  *flagp |= flags&SPSTART;
  while (*regparse == '|') {
	regparse++;
	br = regbranch(&flags);
	if (br == NULL)
		return(NULL);
	regtail(ret, br);	/* BRANCH -> BRANCH. */
	if (!(flags&HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags&SPSTART;
  }

  /* Make a closing node, and hook it on the end. */
  ender = regnode((paren) ? CLOSE+parno : END);	
  regtail(ret, ender);

  /* Hook the tails of the branches to the closing node. */
  for (br = ret; br != NULL; br = regnext(br))
	regoptail(br, ender);

  /* Check for proper termination. */
  if (paren && *regparse++ != ')') {
	FAIL("unmatched ()");
  } else if (!paren && *regparse != '\0') {
	if (*regparse == ')') {
		FAIL("unmatched ()");
	} else
		FAIL("junk on end");	/* "Can't happen". */
	/* NOTREACHED */
  }

  return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static ushort *
regbranch(flagp)
int *flagp;
{
  register ushort *ret;
  register ushort *chain;
  register ushort *latest;
  int flags;

  *flagp = WORST;		/* Tentatively. */

  ret = regnode(BRANCH);
  chain = NULL;
  while (*regparse != '\0' && *regparse != '|' && *regparse != ')') {
	latest = regpiece(&flags);
	if (latest == NULL)
		return(NULL);
	*flagp |= flags&HASWIDTH;
	if (chain == NULL)	/* First piece. */
		*flagp |= flags&SPSTART;
	else
		regtail(chain, latest);
	chain = latest;
  }
  if (chain == NULL)	/* Loop ran zero times. */
	(void) regnode(NOTHING);

  return(ret);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static ushort *
regpiece(flagp)
int *flagp;
{
  register ushort *ret;
  register ushort op;
  register ushort *next;
  int flags;

  ret = regatom(&flags);
  if (ret == NULL)
	return(NULL);

  op = *regparse;
  if (!ISMULT(op)) {
	*flagp = flags;
	return(ret);
  }

  if (!(flags&HASWIDTH) && op != '?')
	FAIL("*+ operand could be empty");
  *flagp = (op != '+') ? (WORST|SPSTART) : (WORST|HASWIDTH);

  if (op == '*' && (flags&SIMPLE))
	reginsert(STAR, ret);
  else if (op == '*') {
	/* Emit x* as (x&|), where & means "self". */
	reginsert(BRANCH, ret);			/* Either x */
	regoptail(ret, regnode(BACK));		/* and loop */
	regoptail(ret, ret);			/* back */
	regtail(ret, regnode(BRANCH));		/* or */
	regtail(ret, regnode(NOTHING));		/* null. */
  } else if (op == '+' && (flags&SIMPLE))
	reginsert(PLUS, ret);
  else if (op == '+') {
	/* Emit x+ as x(&|), where & means "self". */
	next = regnode(BRANCH);			/* Either */
	regtail(ret, next);
	regtail(regnode(BACK), ret);		/* loop back */
	regtail(next, regnode(BRANCH));		/* or */
	regtail(ret, regnode(NOTHING));		/* null. */
  } else if (op == '?') {
	/* Emit x? as (x|) */
	reginsert(BRANCH, ret);			/* Either x */
	regtail(ret, regnode(BRANCH));		/* or */
	next = regnode(NOTHING);		/* null. */
	regtail(ret, next);
	regoptail(ret, next);
  }
  regparse++;
  if (ISMULT(*regparse))
	FAIL("nested *?+");

  return(ret);
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
static ushort *
regatom(flagp)
int *flagp;
{
  register ushort *ret;
  int flags;
  ushort c, d;

  *flagp = WORST;		/* Tentatively. */

  switch ((int) *regparse++) {
  case '^':
	ret = regnode(BOL);
	break;
  case '$':
	ret = regnode(EOL);
	break;
  case '.':
	ret = regnode(ANY);
	*flagp |= HASWIDTH|SIMPLE;
	break;
  case '[': {
		register int class;
		register int classend;
		register int c;

		if (*regparse == '^') {	/* Complement of range. */
			ret = regnode(ANYBUT);
			regparse++;
		} else
			ret = regnode(ANYOF);
		if (*regparse == ']' || *regparse == '-')
			regc(*regparse++);
		while (*regparse != '\0' && *regparse != ']') {
			if (*regparse == '-') {
				regparse++;
				if (*regparse == ']' || *regparse == '\0')
					regc((ushort) '-');
				else {
					class = UCHARAT(regparse-2)+1;
					classend = UCHARAT(regparse);
					if (class > classend+1)
						FAIL("invalid [] range");
					regc((ushort) 0xffff);
					regc((ushort) class);
					regc((ushort) classend);
					regparse++;
				}
			} else {
				if ((c = *regparse++) == '\\') {
					if ((c = *regparse++) == 'n')
						c = '\n';
					else if (c == 't')
						c = '\t';
				}
				regc(c);
			}
		}
		regc((ushort) 0);
		if (*regparse != ']')
			FAIL("unmatched []");
		regparse++;
		*flagp |= HASWIDTH|SIMPLE;
	}
	break;
  case '(':
	ret = reg(1, &flags);
	if (ret == NULL)
		return(NULL);
	*flagp |= flags&(HASWIDTH|SPSTART);
	break;
  case '\0':
  case '|':
  case ')':
	FAIL("internal urp");	/* Supposed to be caught earlier. */
	break;
  case '?':
  case '+':
  case '*':
	FAIL("?+* follows nothing");
	break;
  case '\\':
	if (*regparse == '\0')
		FAIL("trailing \\");
	ret = regnode(EXACTLY);
/*
	regc(*regparse++);
*/
	c = *regparse++;
	if (c == 'n')
		c = '\n';
	else if (c == 't')
		c = '\t';
	else if (c == 'f')
		c = '\f';
	else if (c == '\r')
		c = '\r';
	else if (c == '\b')
		c = '\b';
	else if (IsDigid(c)) {
		d = c - '0';
		if (IsDigid(*regparse)) {
			d = d * 8 + *regparse++ - '0';
			if (IsDigid(*regparse))
				d = d * 8 + *regparse++ - '0';
		}
		c = d;
	}
	regc(c);
	regc((ushort) 0);
	*flagp |= HASWIDTH|SIMPLE;
	break;
  default: {
		register int len;
		register char ender;

		regparse--;
		len = Strcspn(regparse, META);
		if (len <= 0)
			FAIL("internal disaster");
		ender = *(regparse+len);
		if (len > 1 && ISMULT(ender))
			len--;		/* Back off clear of ?+* operand. */
		*flagp |= HASWIDTH;
		if (len == 1)
			*flagp |= SIMPLE;
		ret = regnode(EXACTLY);
		while (len > 0) {
			regc(*regparse++);
			len--;
		}
		regc((ushort) 0);
	}
	break;
  }

  return(ret);
}

IsDigid(c) ushort c;
{
  return '0' <= c && c <= '9';
}

/*
 - regnode - emit a node
 */
static ushort *			/* Location. */
regnode(op)
ushort op;
{
  register ushort *ret;
  register ushort *ptr;

  ret = regcode;
  if (ret == &regdummy) {
	regsize += 3;
	return(ret);
  }

  ptr = ret;
  *ptr++ = op;
  *ptr++ = '\0';		/* Null "next" pointer. */
  *ptr++ = '\0';
  regcode = ptr;

  return(ret);
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void
regc(b)
ushort b;
{
  if (regcode != &regdummy)
	*regcode++ = b;
  else
	regsize++;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void
reginsert(op, opnd)
ushort op;
ushort *opnd;
{
  register ushort *src;
  register ushort *dst;
  register ushort *place;

  if (regcode == &regdummy) {
	regsize += 3;
	return;
  }

  src = regcode;
  regcode += 3;
  dst = regcode;
  while (src > opnd)
	*--dst = *--src;

  place = opnd;		/* Op node, where operand used to be. */
  *place++ = op;
  *place++ = '\0';
  *place++ = '\0';
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void
regtail(p, val)
ushort *p;
ushort *val;
{
  register ushort *scan;
  register ushort *temp;
  register int offset;

  if (p == &regdummy)
	return;

  /* Find last node. */
  scan = p;
  for (;;) {
	temp = regnext(scan);
	if (temp == NULL)
		break;
	scan = temp;
  }

  if (OP(scan) == BACK)
	offset = scan - val;
  else
	offset = val - scan;
  *(scan+1) = (offset>>8)&0377;
  *(scan+2) = offset&0377;
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */
static void
regoptail(p, val)
ushort *p;
ushort *val;
{
  /* "Operandless" and "op != BRANCH" are synonymous in practice. */
  if (p == NULL || p == &regdummy || OP(p) != BRANCH)
	return;
  regtail(OPERAND(p), val);
}

/*
 * regexec and friends
 */

/*
 * Global work variables for regexec().
 */
static ushort *reginput;		/* String-input pointer. */
static ushort *regbol;		/* Beginning of input, for ^ check. */
static ushort **regstartp;	/* Pointer to startp array. */
static ushort **regendp;		/* Ditto for endp. */

/*
 * Forwards.
 */
STATIC int regtry();
STATIC int regmatch();
STATIC int regrepeat();

#ifdef DEBUG
int regnarrate = 0;
void regdump();
STATIC char *regprop();
#endif

/*
 - regexec - match a regexp against a string
 */
int
regexec(prog, string, bolflag)
register regexp *prog;
register ushort *string;
int bolflag;
{
  register ushort *s;
  extern ushort *Strchr();

  /* Be paranoid... */
  if (prog == NULL || string == NULL) {
	regerror("NULL parameter");
	return(0);
  }

  /* Check validity of program. */
  if (prog->program[0] != MAGIC) {
	regerror("corrupted program");
	return(0);
  }

  /* If there is a "must appear" string, look for it. */
  if (prog->regmust != NULL) {
	s = string;
	while ((s = Strchr(s, prog->regmust[0])) != NULL) {
		if (Strncmp(s, prog->regmust, prog->regmlen) == 0)
			break;	/* Found it. */
		s++;
	}
	if (s == NULL)	/* Not present. */
		return(0);
  }

  /* Mark beginning of line for ^ . */
  if(bolflag)
	regbol = string;
  else
	regbol = NULL;

  /* Simplest case:  anchored match need be tried only once. */
  if (prog->reganch)
	return(regtry(prog, string));

  /* Messy cases:  unanchored match. */
  s = string;
  if (prog->regstart != '\0') {
	/* We know what char it must start with. */
	while ((s = Strchr(s, prog->regstart)) != NULL) {
		if (regtry(prog, s))
			return(1);
		s++;
	}
}
  else
	/* We don't -- general case. */
	do {
		if (regtry(prog, s))
			return(1);
	} while (*s++ != '\0');

  /* Failure. */
  return(0);
}

/*
 - regtry - try match at specific point
 */
static int			/* 0 failure, 1 success */
regtry(prog, string)
regexp *prog;
ushort *string;
{
  register int i;
  register ushort **sp;
  register ushort **ep;

  reginput = string;
  regstartp = prog->startp;
  regendp = prog->endp;

  sp = prog->startp;
  ep = prog->endp;
  for (i = NSUBEXP; i > 0; i--) {
	*sp++ = NULL;
	*ep++ = NULL;
  }
  if (regmatch(prog->program + 1)) {
	prog->startp[0] = string;
	prog->endp[0] = reginput;
	return(1);
  } else
	return(0);
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
static int			/* 0 failure, 1 success */
regmatch(prog)
ushort *prog;
{
  register ushort *scan;	/* Current node. */
  ushort *next;		/* Next node. */
  extern ushort *Strchr();

  scan = prog;
#ifdef DEBUG
  if (scan != NULL && regnarrate)
	fprintf(stderr, "%s(\n", regprop(scan));
#endif
  while (scan != NULL) {
#ifdef DEBUG
	if (regnarrate)
		fprintf(stderr, "%s...\n", regprop(scan));
#endif
	next = regnext(scan);

	switch ((int) OP(scan)) {
	case BOL:
		if (reginput != regbol)
			return(0);
		break;
	case EOL:
		if (*reginput != '\0')
			return(0);
		break;
	case ANY:
		if (*reginput == '\0')
			return(0);
		reginput++;
		break;
	case EXACTLY: {
			register int len;
			register ushort *opnd;

			opnd = OPERAND(scan);
			/* Inline the first character, for speed. */
			if (*opnd != *reginput)
				return(0);
			len = Strlen(opnd);
			if (len > 1 && Strncmp(opnd, reginput, len) != 0)
				return(0);
			reginput += len;
		}
		break;
	case ANYOF:
		if (*reginput == '\0' || isthere(OPERAND(scan), *reginput) == 0)
			return(0);
		reginput++;
		break;
	case ANYBUT:
		if (*reginput == '\0' || isthere(OPERAND(scan), *reginput) != 0)
			return(0);
		reginput++;
		break;
	case NOTHING:
		break;
	case BACK:
		break;
	case OPEN+1:
	case OPEN+2:
	case OPEN+3:
	case OPEN+4:
	case OPEN+5:
	case OPEN+6:
	case OPEN+7:
	case OPEN+8:
	case OPEN+9: {
			register int no;
			register ushort *save;

			no = OP(scan) - OPEN;
			save = reginput;

			if (regmatch(next)) {
				/*
				 * Don't set startp if some later
				 * invocation of the same parentheses
				 * already has.
				 */
				if (regstartp[no] == NULL)
					regstartp[no] = save;
				return(1);
			} else
				return(0);
		}
		break;
	case CLOSE+1:
	case CLOSE+2:
	case CLOSE+3:
	case CLOSE+4:
	case CLOSE+5:
	case CLOSE+6:
	case CLOSE+7:
	case CLOSE+8:
	case CLOSE+9: {
			register int no;
			register ushort *save;

			no = OP(scan) - CLOSE;
			save = reginput;

			if (regmatch(next)) {
				/*
				 * Don't set endp if some later
				 * invocation of the same parentheses
				 * already has.
				 */
				if (regendp[no] == NULL)
					regendp[no] = save;
				return(1);
			} else
				return(0);
		}
		break;
	case BRANCH: {
			register ushort *save;

			if (OP(next) != BRANCH)		/* No choice. */
				next = OPERAND(scan);	/* Avoid recursion. */
			else {
				do {
					save = reginput;
					if (regmatch(OPERAND(scan)))
						return(1);
					reginput = save;
					scan = regnext(scan);
				} while (scan != NULL && OP(scan) == BRANCH);
				return(0);
				/* NOTREACHED */
			}
		}
		break;
	case STAR:
	case PLUS: {
			register ushort nextch;
			register int no;
			register ushort *save;
			register int min;

			/*
			 * Lookahead to avoid useless match attempts
			 * when we know what character comes next.
			 */
			nextch = '\0';
			if (OP(next) == EXACTLY)
				nextch = *OPERAND(next);
			min = (OP(scan) == STAR) ? 0 : 1;
			save = reginput;
			no = regrepeat(OPERAND(scan));
			while (no >= min) {
				/* If it could work, try it. */
				if (nextch == '\0' || *reginput == nextch)
					if (regmatch(next))
						return(1);
				/* Couldn't or didn't -- back up. */
				no--;
				reginput = save + no;
			}
			return(0);
		}
		break;
	case END:
		return(1);	/* Success! */
		break;
	default:
		regerror("memory corruption");
		return(0);
		break;
	}

	scan = next;
  }

  /*
   * We get here only if there's trouble -- normally "case END" is
   * the terminating point.
   */
  regerror("corrupted pointers");
  return(0);
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int
regrepeat(p)
ushort *p;
{
  register int count = 0;
  register ushort *scan;
  register ushort *opnd;

  scan = reginput;
  opnd = OPERAND(p);
  switch (OP(p)) {
  case ANY:
	count = Strlen(scan);
	scan += count;
	break;
  case EXACTLY:
	while (*opnd == *scan) {
		count++;
		scan++;
	}
	break;
  case ANYOF:
	while (*scan != '\0' && isthere(opnd, *scan) != 0) {
		count++;
		scan++;
	}
	break;
  case ANYBUT:
	while (*scan != '\0' && isthere(opnd, *scan) == 0) {
		count++;
		scan++;
	}
	break;
  default:		/* Oh dear.  Called inappropriately. */
	regerror("internal foulup");
	count = 0;	/* Best compromise. */
	break;
  }
  reginput = scan;

  return(count);
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static ushort *
regnext(p)
register ushort *p;
{
  register int offset;

  if (p == &regdummy)
	return(NULL);

  offset = NEXT(p);
  if (offset == 0)
	return(NULL);

  if (OP(p) == BACK)
	return(p-offset);
  else
	return(p+offset);
}

#ifdef DEBUG

STATIC char *regprop();

/*
 - regdump - dump a regexp onto stdout in vaguely comprehensible form
 */
void
regdump(r)
regexp *r;
{
  register ushort *s;
  register ushort op = EXACTLY;	/* Arbitrary non-END op. */
  register ushort *next;


  s = r->program + 1;
  while (op != END) {	/* While that wasn't END last time... */
	op = OP(s);
	printf("%2d%s", s-r->program, regprop(s));	/* Where, what. */
	next = regnext(s);
	if (next == NULL)		/* Next ptr. */
		printf("(0)");
	else 
		printf("(%d)", (s-r->program)+(next-s));
	s += 3;
	if (op == ANYOF || op == ANYBUT || op == EXACTLY) {
		/* Literal string, where present. */
		while (*s != '\0') {
			if (*s == 0xffff) {	/* range */
				kputchar(*++s); putchar('-'); ++s;
			}
			kputchar(*s++);
		}
		s++;
	}
	putchar('\n');
  }

  /* Header fields of interest. */
  if (r->regstart != '\0') {
	fputs("start `", stdout); kputchar(r->regstart); fputs("' ", stdout);
  }
  if (r->reganch)
	printf("anchored ");
  if (r->regmust != NULL) {
	fputs("must have \"", stdout); kputs(r->regmust); putchar('"');
  }
  printf("\n");
}

kputchar(c) ushort c;
{
  if (c & 0xff00)
	putchar(c >> 8);
  putchar(c & 0xff);
}

kputs(s) ushort *s;
{
  while (*s)
	kputchar(*s++);
}

/*
 - regprop - printable representation of opcode
 */
static char *
regprop(op)
ushort *op;
{
  register char *p;
  static char buf[50];

  (void) strcpy(buf, ":");

  switch ((int) OP(op)) {
  case BOL:
	p = "BOL";
	break;
  case EOL:
	p = "EOL";
	break;
  case ANY:
	p = "ANY";
	break;
  case ANYOF:
	p = "ANYOF";
	break;
  case ANYBUT:
	p = "ANYBUT";
	break;
  case BRANCH:
	p = "BRANCH";
	break;
  case EXACTLY:
	p = "EXACTLY";
	break;
  case NOTHING:
	p = "NOTHING";
	break;
  case BACK:
	p = "BACK";
	break;
  case END:
	p = "END";
	break;
  case OPEN+1:
  case OPEN+2:
  case OPEN+3:
  case OPEN+4:
  case OPEN+5:
  case OPEN+6:
  case OPEN+7:
  case OPEN+8:
  case OPEN+9:
	sprintf(buf+strlen(buf), "OPEN%d", OP(op)-OPEN);
	p = NULL;
	break;
  case CLOSE+1:
  case CLOSE+2:
  case CLOSE+3:
  case CLOSE+4:
  case CLOSE+5:
  case CLOSE+6:
  case CLOSE+7:
  case CLOSE+8:
  case CLOSE+9:
	sprintf(buf+strlen(buf), "CLOSE%d", OP(op)-CLOSE);
	p = NULL;
	break;
  case STAR:
	p = "STAR";
	break;
  case PLUS:
	p = "PLUS";
	break;
  default:
	regerror("corrupted opcode");
	break;
  }
  if (p != NULL)
	(void) strcat(buf, p);
  return(buf);
}
#endif

/*
 * The following is provided for those people who do not have strcspn() in
 * their C libraries.  They should get off their butts and do something
 * about it; at least one public-domain implementation of those (highly
 * useful) string routines has been published on Usenet.
 */
/*
 * strcspn - find length of initial segment of s1 consisting entirely
 * of characters not from s2
 */

static int
Strcspn(s1, s2)
ushort *s1;
unsigned char *s2;
{
  register ushort *scan1;
  register unsigned char *scan2;
  register int count;

  count = 0;
  for (scan1 = s1; *scan1 != '\0'; scan1++) {
	for (scan2 = s2; *scan2 != '\0';)	/* ++ moved down. */
		if (*scan1 == *scan2++)
			return(count);
	count++;
  }
  return(count);
}

isthere(s, c) ushort *s, c;
{
  register unsigned int c1, c2;

  for ( ; *s; s++) {
	if (*s == 0xffff) {	/* range */
		c1 = *++s; c2 = *++s;
		if (c1 <= c && c <= c2)
			return 1;
	}
	else if (*s == c)
		return 1;
  }
  return 0;
}

ushort *
Strchr(s, c) ushort *s, c;
{
  for ( ; *s; s++)
	if (*s == c)
		return s;
  return NULL;
}

Strncmp(s, t, n) ushort *s, *t;
{
  for ( ; --n > 0 && *s == *t; s++, t++)
	;
  return *s - *t;
}

Strlen(s) ushort *s;
{
  int i;

  for (i = 0; *s; i++, s++)
	;
  return i;
}

ushort kbuf[BUFSIZ];

char *
mkpat(s) char *s;
{
  sjtok(kbuf, s);
  return (char *) regcomp(kbuf);
}

match(p, s) regexp *p; char *s;
{
  register int i;

  sjtok(kbuf, s);
  if (i = regexec(p, kbuf, 1)) {
	r_start = p->startp[0] - kbuf + 1;
	r_length = p->endp[0] - p->startp[0];
  }
  else
	r_start = r_length = 0;
  return i;
}

sjtok(s, t) ushort *s; unsigned char *t;
{
  register c;

  for ( ; *t; t++) {
	if (isKanji(c = *t))
		c = (c << 8) | (*++t & 0xff);
	*s++ = c;
  }
  *s = 0;
}

ktosj(s, t) unsigned char *s; ushort *t;
{
  register c;

  while (*t) {
	if ((c = *t++) & 0xff00)
		*s++ = c >> 8;
	*s++ = c & 0xff;
  }
  *s = '\0';
}

regsub(dst, exp, src, pat, pos) ushort *dst, *src, *pat; regexp *exp;
{	/* dst <-- s/src/pat/pos	global substitution for pos == 0 */
  register int c, i;
  register ushort *loc1, *loc2, *s, *t, *u;
  register int n = 0;

  if (exp->program[0] != MAGIC) {
	regerror("damaged regexp fed to regsub");
	return 0;
  }
  while (*src) {
next:
	if (regexec(exp, src, 1) == 0)
		break;
	loc1 = exp->startp[0]; loc2 = exp->endp[0];
	if (pos-- > 1) {
		while (src < loc2)
			*dst++ = *src++;
		goto next;
	}
	while (src < loc1)
		*dst++ = *src++;
	for (s = pat; c = *s++; ) {
		if (c == '&')
			i = 0;
		else if (c == '\\' && '0' <= *s && *s <= '9')
			i = *s++ - '0';
		else {
			if (c == '\\' && (*s == '\\' || *s == '&'))
				c = *s++;
			*dst++ = c;
			continue;
		}
		if ((t = exp->startp[i]) != NULL
			&& (u = exp->endp[i]) != NULL) {
			while (t < u)
				*dst++ = *t++;
		}
	}
	src = loc2;
	n++;
	if (pos == 0)
		break;
  }
  while (*src)
	*dst++ = *src++;
  *dst++ = 0;
  return n;
}

static ushort kbuf1[BUFSIZ], kbuf2[BUFSIZ];

Sub(u, exp, str, s, t, pos) char *exp; char *s, *t, *u;
{
  register int i;
  regexp *r;

  if (str) {
	sjtok(kbuf, exp);
	r = regcomp(kbuf);
  }
  else
	r = (regexp *) exp;
  sjtok(kbuf, s);
  sjtok(kbuf1, t);
  i = regsub(kbuf2, r, kbuf1, kbuf, pos);
  ktosj(u, kbuf2);
  if (str)
	sfree(r);

  return i;
}
