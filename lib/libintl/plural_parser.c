/*	$NetBSD: plural_parser.c,v 1.2 2007/01/17 23:24:22 hubertf Exp $	*/

/*-
 * Copyright (c) 2005 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: plural_parser.c,v 1.2 2007/01/17 23:24:22 hubertf Exp $");

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <citrus/citrus_namespace.h>
#include <citrus/citrus_region.h>
#include <citrus/citrus_memstream.h>
#include <citrus/citrus_bcs.h>
#include "plural_parser.h"

#if defined(TEST_TOKENIZER) || defined(TEST_PARSER)
#define ALLOW_EMPTY
#define ALLOW_ARBITRARY_IDENTIFIER
#endif

#define MAX_LEN_ATOM		10
#define MAX_NUM_OPERANDS	3

#define T_EOF			EOF
#define T_NONE			0x100
#define T_LAND			0x101	/* && */
#define T_LOR			0x102	/* || */
#define T_EQUALITY		0x103	/* == or != */
#define T_RELATIONAL		0x104	/* <, >, <= or >= */
#define T_ADDITIVE		0x105	/* + or - */
#define T_MULTIPLICATIVE	0x106	/* *, / or % */
#define T_IDENTIFIER		0x200
#define T_CONSTANT		0x201
#define T_ILCHAR		0x300
#define T_TOOLONG		0x301
#define T_ILTOKEN		0x302
#define T_ILEND			0x303
#define T_NOMEM			0x304
#define T_NOTFOUND		0x305
#define T_ILPLURAL		0x306
#define T_IS_OPERATOR(t)	((t) < 0x200)
#define T_IS_ERROR(t)		((t) >= 0x300)

#define OP_EQ			('='+'=')
#define OP_NEQ			('!'+'=')
#define OP_LTEQ			('<'+'=')
#define OP_GTEQ			('>'+'=')

#define PLURAL_NUMBER_SYMBOL	"n"
#define NPLURALS_SYMBOL		"nplurals"
#define LEN_NPLURAL_SYMBOL	(sizeof (NPLURALS_SYMBOL) -1)
#define PLURAL_SYMBOL		"plural"
#define LEN_PLURAL_SYMBOL	(sizeof (PLURAL_SYMBOL) -1)
#define PLURAL_FORMS		"Plural-Forms:"
#define LEN_PLURAL_FORMS	(sizeof (PLURAL_FORMS) -1)

/* ----------------------------------------------------------------------
 * tokenizer part
 */

union token_data
{
	unsigned long constant;
#ifdef ALLOW_ARBITRARY_IDENTIFIER
	char identifier[MAX_LEN_ATOM+1];
#endif
	char op;
};

struct tokenizer_context
{
	struct _memstream memstream;
	struct {
		int token;
		union token_data token_data;
	} token0;
};

/* initialize a tokenizer context */
static void
init_tokenizer_context(struct tokenizer_context *tcx)
{
	tcx->token0.token = T_NONE;
}

/* get an atom (identifier or constant) */
static int
tokenize_atom(struct tokenizer_context *tcx, union token_data *token_data)
{
	int ch, len;
	char buf[MAX_LEN_ATOM+1];

	len = 0;
	while (/*CONSTCOND*/1) {
		ch = _memstream_getc(&tcx->memstream);
		if (!(_bcs_isalnum(ch) || ch == '_')) {
			_memstream_ungetc(&tcx->memstream, ch);
			break;
		}
		if (len == MAX_LEN_ATOM)
			return T_TOOLONG;
		buf[len++] = ch;
	}
	buf[len] = '\0';
	if (len == 0)
		return T_ILCHAR;

	if (_bcs_isdigit((int)(unsigned char)buf[0])) {
		unsigned long ul;
		char *post;
		ul = strtoul(buf, &post, 0);
		if (buf+len != post)
			return T_ILCHAR;
		token_data->constant = ul;
		return T_CONSTANT;
	}

#ifdef ALLOW_ARBITRARY_IDENTIFIER
	strcpy(token_data->identifier, buf);
	return T_IDENTIFIER;
#else
	if (!strcmp(buf, PLURAL_NUMBER_SYMBOL))
		return T_IDENTIFIER;
	return T_ILCHAR;
#endif
}

/* tokenizer main routine */
static int
tokenize(struct tokenizer_context *tcx, union token_data *token_data)
{
	int ch, prevch;

retry:
	ch = _memstream_getc(&tcx->memstream);
	if (_bcs_isspace(ch))
		goto retry;

	switch (ch) {
	case T_EOF:
		return ch;
	case '+': case '-':
		token_data->op = ch;
		return T_ADDITIVE;
	case '*': case '/': case '%':
		token_data->op = ch;
		return T_MULTIPLICATIVE;
	case '?': case ':': case '(': case ')':
		token_data->op = ch;
		return ch;
	case '&': case '|':
		prevch = ch;
		ch = _memstream_getc(&tcx->memstream);
		if (ch != prevch) {
			_memstream_ungetc(&tcx->memstream, ch);
			return T_ILCHAR;
		}
		token_data->op = ch;
		switch (ch) {
		case '&':
			return T_LAND;
		case '|':
			return T_LOR;
		}
		/*NOTREACHED*/
	case '=': case '!': case '<': case '>':
		prevch = ch;
		ch = _memstream_getc(&tcx->memstream);
		if (ch != '=') {
			_memstream_ungetc(&tcx->memstream, ch);
			switch (prevch) {
			case '=':
				return T_ILCHAR;
			case '!':
				return '!';
			case '<':
			case '>':
				token_data->op = prevch; /* OP_LT or OP_GT */
				return T_RELATIONAL;
			}
		}
		/* '==', '!=', '<=' or '>=' */
		token_data->op = ch+prevch;
		switch (prevch) {
		case '=':
		case '!':
			return T_EQUALITY;
		case '<':
		case '>':
			return T_RELATIONAL;
		}
		/*NOTREACHED*/
	}

	_memstream_ungetc(&tcx->memstream, ch);
	return tokenize_atom(tcx, token_data);
}

/* get the next token */
static int
get_token(struct tokenizer_context *tcx, union token_data *token_data)
{
	if (tcx->token0.token != T_NONE) {
		int token = tcx->token0.token;
		tcx->token0.token = T_NONE;
		*token_data = tcx->token0.token_data;
		return token;
	}
	return tokenize(tcx, token_data);
}

/* push back the last token */
static void
unget_token(struct tokenizer_context *tcx,
	    int token, union token_data *token_data)
{
	tcx->token0.token = token;
	tcx->token0.token_data = *token_data;
}

#ifdef TEST_TOKENIZER

int
main(int argc, char **argv)
{
	struct tokenizer_context tcx;
	union token_data token_data;
	int token;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <expression>\n", argv[0]);
		return EXIT_FAILURE;
	}

	init_tokenizer_context(&tcx);
	_memstream_bind_ptr(&tcx.memstream, argv[1], strlen(argv[1]));

	while (1) {
		token = get_token(&tcx, &token_data);
		switch (token) {
		case T_EOF:
			goto quit;
		case T_ILCHAR:
			printf("illegal character.\n");
			goto quit;
		case T_TOOLONG:
			printf("too long atom.\n");
			goto quit;
		case T_CONSTANT:
			printf("constant: %lu\n", token_data.constant);
			break;
		case T_IDENTIFIER:
			printf("symbol: %s\n", token_data.identifier);
			break;
		default:
			printf("operator: ");
			switch (token) {
			case T_LAND:
				printf("&&\n");
				break;
			case T_LOR:
				printf("||\n");
				break;
			case T_EQUALITY:
				printf("%c=\n", token_data.op-'=');
				break;
			case T_RELATIONAL:
				switch(token_data.op) {
				case OP_LTEQ:
				case OP_GTEQ:
					printf("%c=\n", token_data.op-'=');
					break;
				default:
					printf("%c\n", token_data.op);
					break;
				}
				break;
			case T_ADDITIVE:
			case T_MULTIPLICATIVE:
				printf("%c\n", token_data.op);
				break;
			default:
				printf("operator: %c\n", token);
			}
		}
	}
quit:
	return 0;
}
#endif /* TEST_TOKENIZER */


/* ----------------------------------------------------------------------
 * parser part
 *
 * exp := cond
 *
 * cond := lor | lor '?' cond ':' cond
 *
 * lor := land ( '||' land )*
 *
 * land := equality ( '&&' equality )*
 *
 * equality := relational ( equalityops relational )*
 * equalityops := '==' | '!='
 *
 * relational := additive ( relationalops additive )*
 * relationalops := '<' | '>' | '<=' | '>='
 *
 * additive := multiplicative ( additiveops multiplicative )*
 * additiveops := '+' | '-'
 *
 * multiplicative := lnot ( multiplicativeops lnot )*
 * multiplicativeops := '*' | '/' | '%'
 *
 * lnot := '!' lnot | term
 *
 * term := literal | identifier | '(' exp ')'
 *
 */

#define T_ENSURE_OK(token, label)					      \
do {									      \
	if (T_IS_ERROR(token))						      \
		goto label;						      \
} while (/*CONSTCOND*/0)
#define T_ENSURE_SOMETHING(token, label)				      \
do {									      \
	if ((token) == T_EOF) {						      \
		token = T_ILEND;					      \
		goto label;						      \
	} else if (T_IS_ERROR(token))					      \
		goto label;						      \
} while (/*CONSTCOND*/0)

#define parser_element	plural_element

struct parser_element;
struct parser_op
{
	char op;
	struct parser_element *operands[MAX_NUM_OPERANDS];
};
struct parser_element
{
	int kind;
	union
	{
		struct parser_op parser_op;
		union token_data token_data;
	} u;
};

struct parser_op2_transition
{
	int					kind;
	const struct parser_op2_transition	*next;
};

/* prototypes */
static int parse_cond(struct tokenizer_context *, struct parser_element *);


/* transition table for the 2-operand operators */
#define DEF_TR(t, k, n)							      \
static struct parser_op2_transition exp_tr_##t = {			      \
	k, &exp_tr_##n							      \
}
#define DEF_TR0(t, k)							      \
static struct parser_op2_transition exp_tr_##t = {			      \
	k, NULL /* expect lnot */					      \
}

DEF_TR0(multiplicative, T_MULTIPLICATIVE);
DEF_TR(additive, T_ADDITIVE, multiplicative);
DEF_TR(relational, T_RELATIONAL, additive);
DEF_TR(equality, T_EQUALITY, relational);
DEF_TR(land, T_LAND, equality);
DEF_TR(lor, T_LOR, land);

/* init a parser element structure */
static void
init_parser_element(struct parser_element *pe)
{
	int i;

	pe->kind = T_NONE;
	for (i=0; i<MAX_NUM_OPERANDS; i++)
		pe->u.parser_op.operands[i] = NULL;
}

/* uninitialize a parser element structure with freeing children */
static void free_parser_element(struct parser_element *);
static void
uninit_parser_element(struct parser_element *pe)
{
	int i;

	if (T_IS_OPERATOR(pe->kind))
		for (i=0; i<MAX_NUM_OPERANDS; i++)
			if (pe->u.parser_op.operands[i])
				free_parser_element(
					pe->u.parser_op.operands[i]);
}

/* free a parser element structure with freeing children */
static void
free_parser_element(struct parser_element *pe)
{
	if (pe) {
		uninit_parser_element(pe);
		free(pe);
	}
}


/* copy a parser element structure shallowly */
static void
copy_parser_element(struct parser_element *dpe,
		    const struct parser_element *spe)
{
	memcpy(dpe, spe, sizeof *dpe);
}

/* duplicate a parser element structure shallowly */
static struct parser_element *
dup_parser_element(const struct parser_element *pe)
{
	struct parser_element *dpe = malloc(sizeof *dpe);
	if (dpe)
		copy_parser_element(dpe, pe);
	return dpe;
}

/* term := identifier | constant | '(' exp ')' */
static int
parse_term(struct tokenizer_context *tcx, struct parser_element *pelem)
{
	struct parser_element pe0;
	int token;
	union token_data token_data;

	token = get_token(tcx, &token_data);
	switch (token) {
	case '(':
		/* '(' exp ')' */
		init_parser_element(&pe0);
		/* expect exp */
		token = parse_cond(tcx, &pe0);
		T_ENSURE_OK(token, err);
		/* expect ')' */
		token = get_token(tcx, &token_data);
		T_ENSURE_SOMETHING(token, err);
		if (token != ')') {
			unget_token(tcx, token, &token_data);
			token = T_ILTOKEN;
			goto err;
		}
		copy_parser_element(pelem, &pe0);
		return token;
err:
		uninit_parser_element(&pe0);
		return token;
	case T_IDENTIFIER:
	case T_CONSTANT:
		pelem->kind = token;
		pelem->u.token_data = token_data;
		return token;
	case T_EOF:
		return T_ILEND;
	default:
		return T_ILTOKEN;
	}
}

/* lnot := '!' lnot | term */
static int
parse_lnot(struct tokenizer_context *tcx, struct parser_element *pelem)
{
	struct parser_element pe0;
	int token;
	union token_data token_data;

	init_parser_element(&pe0);

	/* '!' or not */
	token = get_token(tcx, &token_data);
	if (token != '!') {
		/* stop: term */
		unget_token(tcx, token, &token_data);
		return parse_term(tcx, pelem);
	}

	/* '!' term */
	token = parse_lnot(tcx, &pe0);
	T_ENSURE_OK(token, err);

	pelem->kind = '!';
	pelem->u.parser_op.operands[0] = dup_parser_element(&pe0);
	return pelem->kind;
err:
	uninit_parser_element(&pe0);
	return token;
}

/* ext_op := ext_next ( op ext_next )* */
static int
parse_op2(struct tokenizer_context *tcx, struct parser_element *pelem,
	  const struct parser_op2_transition *tr)
{
	struct parser_element pe0, pe1, peop;
	int token;
	union token_data token_data;
	char op;

	/* special case: expect lnot */
	if (tr == NULL)
		return parse_lnot(tcx, pelem);

	init_parser_element(&pe0);
	init_parser_element(&pe1);
	token = parse_op2(tcx, &pe0, tr->next);
	T_ENSURE_OK(token, err);

	while (/*CONSTCOND*/1) {
		/* expect op or empty */
		token = get_token(tcx, &token_data);
		if (token != tr->kind) {
			/* stop */
			unget_token(tcx, token, &token_data);
			copy_parser_element(pelem, &pe0);
			break;
		}
		op = token_data.op;
		/* right hand */
		token = parse_op2(tcx, &pe1, tr->next);
		T_ENSURE_OK(token, err);

		init_parser_element(&peop);
		peop.kind = tr->kind;
		peop.u.parser_op.op = op;
		peop.u.parser_op.operands[0] = dup_parser_element(&pe0);
		init_parser_element(&pe0);
		peop.u.parser_op.operands[1] = dup_parser_element(&pe1);
		init_parser_element(&pe1);
		copy_parser_element(&pe0, &peop);
	}
	return pelem->kind;
err:
	uninit_parser_element(&pe1);
	uninit_parser_element(&pe0);
	return token;
}

/* cond := lor | lor '?' cond ':' cond */
static int
parse_cond(struct tokenizer_context *tcx, struct parser_element *pelem)
{
	struct parser_element pe0, pe1, pe2;
	int token;
	union token_data token_data;

	init_parser_element(&pe0);
	init_parser_element(&pe1);
	init_parser_element(&pe2);

	/* expect lor or empty */
	token = parse_op2(tcx, &pe0, &exp_tr_lor);
	T_ENSURE_OK(token, err);

	/* '?' or not */
	token = get_token(tcx, &token_data);
	if (token != '?') {
		/* stop: lor */
		unget_token(tcx, token, &token_data);
		copy_parser_element(pelem, &pe0);
		return pe0.kind;
	}

	/* lor '?' cond ':' cond */
	/* expect cond */
	token = parse_cond(tcx, &pe1);
	T_ENSURE_OK(token, err);

	/* expect ':' */
	token = get_token(tcx, &token_data);
	T_ENSURE_OK(token, err);
	if (token != ':') {
		unget_token(tcx, token, &token_data);
		token = T_ILTOKEN;
		goto err;
	}

	/* expect cond */
	token = parse_cond(tcx, &pe2);
	T_ENSURE_OK(token, err);

	pelem->kind = '?';
	pelem->u.parser_op.operands[0] = dup_parser_element(&pe0);
	pelem->u.parser_op.operands[1] = dup_parser_element(&pe1);
	pelem->u.parser_op.operands[2] = dup_parser_element(&pe2);
	return pelem->kind;
err:
	uninit_parser_element(&pe2);
	uninit_parser_element(&pe1);
	uninit_parser_element(&pe0);
	return token;
}

static int
parse_exp(struct tokenizer_context *tcx, struct parser_element *pelem)
{
	int token, token1;
	union token_data token_data;

#ifdef ALLOW_EMPTY
	/* empty check */
	token = get_token(tcx, &token_data);
	if (token == T_EOF)
		return token;
	unget_token(tcx, token, &token_data);
#endif

	token = parse_cond(tcx, pelem);
	if (!T_IS_ERROR(token)) {
		/* termination check */
		token1 = get_token(tcx, &token_data);
		if (token1 == T_EOF)
			return token;
		else if (!T_IS_ERROR(token))
			 unget_token(tcx, token1, &token_data);
		return T_ILTOKEN;
	}
	return token;
}


#if defined(TEST_PARSER) || defined(TEST_PARSE_PLURAL)
#include <stdio.h>

static void dump_elem(struct parser_element *);

static void
dump_op2(struct parser_element *pelem)
{
	dump_elem(pelem->u.parser_op.operands[0]);
	printf(" ");
	dump_elem(pelem->u.parser_op.operands[1]);
	printf(")");
}

static void
dump_op3(struct parser_element *pelem)
{
	dump_elem(pelem->u.parser_op.operands[0]);
	printf(" ");
	dump_elem(pelem->u.parser_op.operands[1]);
	printf(" ");
	dump_elem(pelem->u.parser_op.operands[2]);
	printf(")");
}

static void
dump_elem(struct parser_element *pelem)
{
	switch (pelem->kind) {
	case T_LAND:
		printf("(&& ");
		dump_op2(pelem);
		break;
	case T_LOR:
		printf("(|| ");
		dump_op2(pelem);
		break;
	case T_EQUALITY:
		switch (pelem->u.parser_op.op) {
		case OP_EQ:
			printf("(== ");
			break;
		case OP_NEQ:
			printf("(!= ");
			break;
		}
		dump_op2(pelem);
		break;
	case T_RELATIONAL:
		switch (pelem->u.parser_op.op) {
		case '<':
		case '>':
			printf("(%c ", pelem->u.parser_op.op);
			break;
		case OP_LTEQ:
		case OP_GTEQ:
			printf("(%c= ", pelem->u.parser_op.op-'=');
			break;
		}
		dump_op2(pelem);
		break;
	case T_ADDITIVE:
	case T_MULTIPLICATIVE:
		printf("(%c ", pelem->u.parser_op.op);
		dump_op2(pelem);
		break;
	case '!':
		printf("(! ");
		dump_elem(pelem->u.parser_op.operands[0]);
		printf(")");
		break;
	case '?':
		printf("(? ");
		dump_op3(pelem);
		break;
	case T_CONSTANT:
		printf("%d", pelem->u.token_data.constant);
		break;
	case T_IDENTIFIER:
#ifdef ALLOW_ARBITRARY_IDENTIFIER
		printf("%s", pelem->u.token_data.identifier);
#else
		printf(PLURAL_NUMBER_SYMBOL);
#endif
		break;
	}
}
#endif
#ifdef TEST_PARSER
int
main(int argc, char **argv)
{
	struct tokenizer_context tcx;
	struct parser_element pelem;
	int token;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <expression>\n", argv[0]);
		return EXIT_FAILURE;
	}

	init_tokenizer_context(&tcx);
	_memstream_bind_ptr(&tcx.memstream, argv[1], strlen(argv[1]));

	init_parser_element(&pelem);
	token = parse_exp(&tcx, &pelem);

	if (token == T_EOF)
		printf("none");
	else if (T_IS_ERROR(token))
		printf("error: 0x%X", token);
	else
		dump_elem(&pelem);
	printf("\n");

	uninit_parser_element(&pelem);

	return EXIT_SUCCESS;
}
#endif /* TEST_PARSER */

/* ----------------------------------------------------------------------
 * calcurate plural number
 */
static unsigned long
calculate_plural(const struct parser_element *pe, unsigned long n)
{
	unsigned long val0, val1;
	switch (pe->kind) {
	case T_IDENTIFIER:
		return n;
	case T_CONSTANT:
		return pe->u.token_data.constant;
	case '?':
		val0 = calculate_plural(pe->u.parser_op.operands[0], n);
		if (val0)
			val1=calculate_plural(pe->u.parser_op.operands[1], n);
		else
			val1=calculate_plural(pe->u.parser_op.operands[2], n);
		return val1;
	case '!':
		return !calculate_plural(pe->u.parser_op.operands[0], n);
	case T_MULTIPLICATIVE:
	case T_ADDITIVE:
	case T_RELATIONAL:
	case T_EQUALITY:
	case T_LOR:
	case T_LAND:
		val0 = calculate_plural(pe->u.parser_op.operands[0], n);
		val1 = calculate_plural(pe->u.parser_op.operands[1], n);
		switch (pe->u.parser_op.op) {
		case '*':
			return val0*val1;
		case '/':
			return val0/val1;
		case '%':
			return val0%val1;
		case '+':
			return val0+val1;
		case '-':
			return val0-val1;
		case '<':
			return val0<val1;
		case '>':
			return val0>val1;
		case OP_LTEQ:
			return val0<=val1;
		case OP_GTEQ:
			return val0>=val1;
		case OP_EQ:
			return val0==val1;
		case OP_NEQ:
			return val0!=val1;
		case '|':
			return val0||val1;
		case '&':
			return val0&&val1;
		}
	}
	return 0;
}

#ifdef TEST_CALC_PLURAL
#include <stdio.h>

int
main(int argc, char **argv)
{
	struct tokenizer_context tcx;
	struct parser_element pelem;
	int token;

	if (argc != 3) {
		fprintf(stderr, "usage: %s <expression> <n>\n", argv[0]);
		return EXIT_FAILURE;
	}

	init_tokenizer_context(&tcx);
	_memstream_bind_ptr(&tcx.memstream, argv[1], strlen(argv[1]));

	init_parser_element(&pelem);
	token = parse_exp(&tcx, &pelem);

	if (token == T_EOF)
		printf("none");
	else if (T_IS_ERROR(token))
		printf("error: 0x%X", token);
	else {
		printf("plural = %lu",
		       calculate_plural(&pelem, atoi(argv[2])));
	}
	printf("\n");

	uninit_parser_element(&pelem);

	return EXIT_SUCCESS;
}
#endif /* TEST_CALC_PLURAL */


/* ----------------------------------------------------------------------
 * parse plural forms
 */

static void
region_skip_ws(struct _region *r)
{
	const char *str = _region_head(r);
	size_t len = _region_size(r);

	str = _bcs_skip_ws_len(str, &len);
	_region_init(r, __UNCONST(str), len);
}

static void
region_trunc_rws(struct _region *r)
{
	const char *str = _region_head(r);
	size_t len = _region_size(r);

	_bcs_trunc_rws_len(str, &len);
	_region_init(r, __UNCONST(str), len);
}

static int
region_check_prefix(struct _region *r, const char *pre, size_t prelen,
		    int ignorecase)
{
	if (_region_size(r) < prelen)
		return -1;

	if (ignorecase) {
		if (_bcs_strncasecmp(_region_head(r), pre, prelen))
			return -1;
	} else {
		if (memcmp(_region_head(r), pre, prelen))
			return -1;
	}
	return 0;
}

static int
cut_trailing_semicolon(struct _region *r)
{

	region_trunc_rws(r);
	if (_region_size(r) == 0 || _region_peek8(r, _region_size(r)-1) != ';')
		return -1;
	_region_get_subregion(r, r, 0, _region_size(r)-1);
	return 0;
}

static int
find_plural_forms(struct _region *r)
{
	struct _memstream ms;
	struct _region rr;

	_memstream_bind(&ms, r);

	while (!_memstream_getln_region(&ms, &rr)) {
		if (!region_check_prefix(&rr,
					 PLURAL_FORMS, LEN_PLURAL_FORMS, 1)) {
			_region_get_subregion(
				r, &rr, LEN_PLURAL_FORMS,
				_region_size(&rr)-LEN_PLURAL_FORMS);
			region_skip_ws(r);
			region_trunc_rws(r);
			return 0;
		}
	}
	return -1;
}

static int
skip_assignment(struct _region *r, const char *sym, size_t symlen)
{
	region_skip_ws(r);
	if (region_check_prefix(r, sym, symlen, 0))
		return -1;
	_region_get_subregion(r, r, symlen, _region_size(r)-symlen);
	region_skip_ws(r);
	if (_region_size(r) == 0 || _region_peek8(r, 0) != '=')
		return -1;
	_region_get_subregion(r, r, 1, _region_size(r)-1);
	region_skip_ws(r);
	return 0;
}

static int
skip_nplurals(struct _region *r, unsigned long *rnp)
{
	unsigned long np;
	char buf[MAX_LEN_ATOM+2], *endptr;
	const char *endptrconst;
	size_t ofs;

	if (skip_assignment(r, NPLURALS_SYMBOL, LEN_NPLURAL_SYMBOL))
		return -1;
	if (_region_size(r) == 0 || !_bcs_isdigit(_region_peek8(r, 0)))
		return -1;
	strlcpy(buf, _region_head(r), sizeof (buf));
	np = strtoul(buf, &endptr, 0);
	endptrconst = _bcs_skip_ws(endptr);
	if (*endptrconst != ';')
		return -1;
	ofs = endptrconst+1-buf;
	if (_region_get_subregion(r, r, ofs, _region_size(r)-ofs))
		return -1;
	if (rnp)
		*rnp = np;
	return 0;
}

static int
parse_plural_body(struct _region *r, struct parser_element **rpe)
{
	int token;
	struct tokenizer_context tcx;
	struct parser_element pelem, *ppe;

	init_tokenizer_context(&tcx);
	_memstream_bind(&tcx.memstream, r);

	init_parser_element(&pelem);
	token = parse_exp(&tcx, &pelem);
	if (T_IS_ERROR(token))
		return token;

	ppe = dup_parser_element(&pelem);
	if (ppe == NULL) {
		uninit_parser_element(&pelem);
		return T_NOMEM;
	}

	*rpe = ppe;

	return 0;
}

static int
parse_plural(struct parser_element **rpe, unsigned long *rnp,
	     const char *str, size_t len)
{
	struct _region r;

	_region_init(&r, __UNCONST(str), len);

	if (find_plural_forms(&r))
		return T_NOTFOUND;
	if (skip_nplurals(&r, rnp))
		return T_ILPLURAL;
	if (skip_assignment(&r, PLURAL_SYMBOL, LEN_PLURAL_SYMBOL))
		return T_ILPLURAL;
	if (cut_trailing_semicolon(&r))
		return T_ILPLURAL;
	return parse_plural_body(&r, rpe);
}

#ifdef TEST_PARSE_PLURAL
int
main(int argc, char **argv)
{
	int ret;
	struct parser_element *pelem;
	unsigned long np;

	if (argc != 2 && argc != 3) {
		fprintf(stderr, "usage: %s <mime-header> [n]\n", argv[0]);
		return EXIT_FAILURE;
	}

	ret = parse_plural(&pelem, &np, argv[1], strlen(argv[1]));

	if (ret == T_EOF)
		printf("none");
	else if (T_IS_ERROR(ret))
		printf("error: 0x%X", ret);
	else {
		printf("syntax tree: ");
		dump_elem(pelem);
		printf("\nnplurals = %lu", np);
		if (argv[2])
			printf(", plural = %lu",
			       calculate_plural(pelem, atoi(argv[2])));
		free_parser_element(pelem);
	}
	printf("\n");


	return EXIT_SUCCESS;
}
#endif /* TEST_PARSE_PLURAL */

/*
 * external interface
 */

int
_gettext_parse_plural(struct gettext_plural **rpe, unsigned long *rnp,
		      const char *str, size_t len)
{
	return parse_plural((struct parser_element **)rpe, rnp, str, len);
}

unsigned long
_gettext_calculate_plural(const struct gettext_plural *pe, unsigned long n)
{
	return calculate_plural((void *)__UNCONST(pe), n);
}

void
_gettext_free_plural(struct gettext_plural *pe)
{
	free_parser_element((void *)pe);
}

#ifdef TEST_PLURAL
#include <libintl.h>
#include <locale.h>

#define PR(n)	printf("n=%d: \"%s\"\n", n, dngettext("test", "1", "2", n))

int
main(void)
{
	bindtextdomain("test", "."); /* ./LANG/LC_MESSAGES/test.mo */
	PR(1);
	PR(2);
	PR(3);
	PR(4);

	return 0;
}
#endif
