/*	parse_ack.c - parse ACK assembly		Author: Kees J. Bot
 *		      parse NCC assembly			18 Dec 1993
 */
#define nil 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "asmconv.h"
#include "token.h"
#include "asm86.h"
#include "languages.h"

typedef struct mnemonic {	/* ACK as86 mnemonics translation table. */
	char		*name;
	opcode_t	opcode;
	optype_t	optype;
} mnemonic_t;

static mnemonic_t mnemtab[] = {			/* This array is sorted. */
	{ ".align",	DOT_ALIGN,	PSEUDO },
	{ ".ascii",	DOT_ASCII,	PSEUDO },
	{ ".asciz",	DOT_ASCIZ,	PSEUDO },
	{ ".assert",	DOT_ASSERT,	PSEUDO },
	{ ".base",	DOT_BASE,	PSEUDO },
	{ ".bss",	DOT_BSS,	PSEUDO },
	{ ".comm",	DOT_LCOMM,	PSEUDO },
	{ ".data",	DOT_DATA,	PSEUDO },
	{ ".data1",	DOT_DATA1,	PSEUDO },
	{ ".data2",	DOT_DATA2,	PSEUDO },
	{ ".data4",	DOT_DATA4,	PSEUDO },
	{ ".define",	DOT_DEFINE,	PSEUDO },
	{ ".end",	DOT_END,	PSEUDO },
	{ ".extern",	DOT_EXTERN,	PSEUDO },
	{ ".file",	DOT_FILE,	PSEUDO },
	{ ".line",	DOT_LINE,	PSEUDO },
	{ ".list",	DOT_LIST,	PSEUDO },
	{ ".nolist",	DOT_NOLIST,	PSEUDO },
	{ ".rom",	DOT_ROM,	PSEUDO },
	{ ".space",	DOT_SPACE,	PSEUDO },
	{ ".symb",	DOT_SYMB,	PSEUDO },
	{ ".text",	DOT_TEXT,	PSEUDO },
	{ ".use16",	DOT_USE16,	PSEUDO },
	{ ".use32",	DOT_USE32,	PSEUDO },
	{ "aaa",	AAA,		WORD },
	{ "aad",	AAD,		WORD },
	{ "aam",	AAM,		WORD },
	{ "aas",	AAS,		WORD },
	{ "adc",	ADC,		WORD },
	{ "adcb",	ADC,		BYTE },
	{ "add",	ADD,		WORD },
	{ "addb",	ADD,		BYTE },
	{ "and",	AND,		WORD },
	{ "andb",	AND,		BYTE },
	{ "arpl",	ARPL,		WORD },
	{ "bound",	BOUND,		WORD },
	{ "bsf",	BSF,		WORD },
	{ "bsr",	BSR,		WORD },
	{ "bswap",	BSWAP,		WORD },
	{ "bt",		BT,		WORD },
	{ "btc",	BTC,		WORD },
	{ "btr",	BTR,		WORD },
	{ "bts",	BTS,		WORD },
	{ "call",	CALL,		JUMP },
	{ "callf",	CALLF,		JUMP },
	{ "cbw",	CBW,		WORD },
	{ "cdq",	CWD,		WORD },
	{ "clc",	CLC,		WORD },
	{ "cld",	CLD,		WORD },
	{ "cli",	CLI,		WORD },
	{ "clts",	CLTS,		WORD },
	{ "cmc",	CMC,		WORD },
	{ "cmp",	CMP,		WORD },
	{ "cmpb",	CMP,		BYTE },
	{ "cmps",	CMPS,		WORD },
	{ "cmpsb",	CMPS,		BYTE },
	{ "cmpxchg",	CMPXCHG,	WORD },
	{ "cwd",	CWD,		WORD },
	{ "cwde",	CBW,		WORD },
	{ "daa",	DAA,		WORD },
	{ "das",	DAS,		WORD },
	{ "dec",	DEC,		WORD },
	{ "decb",	DEC,		BYTE },
	{ "div",	DIV,		WORD },
	{ "divb",	DIV,		BYTE },
	{ "enter",	ENTER,		WORD },
	{ "f2xm1",	F2XM1,		WORD },
	{ "fabs",	FABS,		WORD },
	{ "fadd",	FADD,		WORD },
	{ "faddd",	FADDD,		WORD },
	{ "faddp",	FADDP,		WORD },
	{ "fadds",	FADDS,		WORD },
	{ "fbld",	FBLD,		WORD },
	{ "fbstp",	FBSTP,		WORD },
	{ "fchs",	FCHS,		WORD },
	{ "fclex",	FCLEX,		WORD },
	{ "fcomd",	FCOMD,		WORD },
	{ "fcompd",	FCOMPD,		WORD },
	{ "fcompp",	FCOMPP,		WORD },
	{ "fcomps",	FCOMPS,		WORD },
	{ "fcoms",	FCOMS,		WORD },
	{ "fcos",	FCOS,		WORD },
	{ "fdecstp",	FDECSTP,	WORD },
	{ "fdivd",	FDIVD,		WORD },
	{ "fdivp",	FDIVP,		WORD },
	{ "fdivrd",	FDIVRD,		WORD },
	{ "fdivrp",	FDIVRP,		WORD },
	{ "fdivrs",	FDIVRS,		WORD },
	{ "fdivs",	FDIVS,		WORD },
	{ "ffree",	FFREE,		WORD },
	{ "fiaddl",	FIADDL,		WORD },
	{ "fiadds",	FIADDS,		WORD },
	{ "ficom",	FICOM,		WORD },
	{ "ficomp",	FICOMP,		WORD },
	{ "fidivl",	FIDIVL,		WORD },
	{ "fidivrl",	FIDIVRL,	WORD },
	{ "fidivrs",	FIDIVRS,	WORD },
	{ "fidivs",	FIDIVS,		WORD },
	{ "fildl",	FILDL,		WORD },
	{ "fildq",	FILDQ,		WORD },
	{ "filds",	FILDS,		WORD },
	{ "fimull",	FIMULL,		WORD },
	{ "fimuls",	FIMULS,		WORD },
	{ "fincstp",	FINCSTP,	WORD },
	{ "finit",	FINIT,		WORD },
	{ "fistl",	FISTL,		WORD },
	{ "fistp",	FISTP,		WORD },
	{ "fists",	FISTS,		WORD },
	{ "fisubl",	FISUBL,		WORD },
	{ "fisubrl",	FISUBRL,	WORD },
	{ "fisubrs",	FISUBRS,	WORD },
	{ "fisubs",	FISUBS,		WORD },
	{ "fld1",	FLD1,		WORD },
	{ "fldcw",	FLDCW,		WORD },
	{ "fldd",	FLDD,		WORD },
	{ "fldenv",	FLDENV,		WORD },
	{ "fldl2e",	FLDL2E,		WORD },
	{ "fldl2t",	FLDL2T,		WORD },
	{ "fldlg2",	FLDLG2,		WORD },
	{ "fldln2",	FLDLN2,		WORD },
	{ "fldpi",	FLDPI,		WORD },
	{ "flds",	FLDS,		WORD },
	{ "fldx",	FLDX,		WORD },
	{ "fldz",	FLDZ,		WORD },
	{ "fmuld",	FMULD,		WORD },
	{ "fmulp",	FMULP,		WORD },
	{ "fmuls",	FMULS,		WORD },
	{ "fnop",	FNOP,		WORD },
	{ "fpatan",	FPATAN,		WORD },
	{ "fprem",	FPREM,		WORD },
	{ "fprem1",	FPREM1,		WORD },
	{ "fptan",	FPTAN,		WORD },
	{ "frndint",	FRNDINT,	WORD },
	{ "frstor",	FRSTOR,		WORD },
	{ "fsave",	FSAVE,		WORD },
	{ "fscale",	FSCALE,		WORD },
	{ "fsin",	FSIN,		WORD },
	{ "fsincos",	FSINCOS,	WORD },
	{ "fsqrt",	FSQRT,		WORD },
	{ "fstcw",	FSTCW,		WORD },
	{ "fstd",	FSTD,		WORD },
	{ "fstenv",	FSTENV,		WORD },
	{ "fstpd",	FSTPD,		WORD },
	{ "fstps",	FSTPS,		WORD },
	{ "fstpx",	FSTPX,		WORD },
	{ "fsts",	FSTS,		WORD },
	{ "fstsw",	FSTSW,		WORD },
	{ "fsubd",	FSUBD,		WORD },
	{ "fsubp",	FSUBP,		WORD },
	{ "fsubpr",	FSUBPR,		WORD },
	{ "fsubrd",	FSUBRD,		WORD },
	{ "fsubrs",	FSUBRS,		WORD },
	{ "fsubs",	FSUBS,		WORD },
	{ "ftst",	FTST,		WORD },
	{ "fucom",	FUCOM,		WORD },
	{ "fucomp",	FUCOMP,		WORD },
	{ "fucompp",	FUCOMPP,	WORD },
	{ "fxam",	FXAM,		WORD },
	{ "fxch",	FXCH,		WORD },
	{ "fxtract",	FXTRACT,	WORD },
	{ "fyl2x",	FYL2X,		WORD },
	{ "fyl2xp1",	FYL2XP1,	WORD },
	{ "hlt",	HLT,		WORD },
	{ "idiv",	IDIV,		WORD },
	{ "idivb",	IDIV,		BYTE },
	{ "imul",	IMUL,		WORD },
	{ "imulb",	IMUL,		BYTE },
	{ "in",		IN,		WORD },
	{ "inb",	IN,		BYTE },
	{ "inc",	INC,		WORD },
	{ "incb",	INC,		BYTE },
	{ "ins",	INS,		WORD },
	{ "insb",	INS,		BYTE },
	{ "int",	INT,		WORD },
	{ "into",	INTO,		JUMP },
	{ "invd",	INVD,		WORD },
	{ "invlpg",	INVLPG,		WORD },
	{ "iret",	IRET,		JUMP },
	{ "iretd",	IRETD,		JUMP },
	{ "ja",		JA,		JUMP },
	{ "jae",	JAE,		JUMP },
	{ "jb",		JB,		JUMP },
	{ "jbe",	JBE,		JUMP },
	{ "jc",		JB,		JUMP },
	{ "jcxz",	JCXZ,		JUMP },
	{ "je",		JE,		JUMP },
	{ "jecxz",	JCXZ,		JUMP },
	{ "jg",		JG,		JUMP },
	{ "jge",	JGE,		JUMP },
	{ "jl",		JL,		JUMP },
	{ "jle",	JLE,		JUMP },
	{ "jmp",	JMP,		JUMP },
	{ "jmpf",	JMPF,		JUMP },
	{ "jna",	JBE,		JUMP },
	{ "jnae",	JB,		JUMP },
	{ "jnb",	JAE,		JUMP },
	{ "jnbe",	JA,		JUMP },
	{ "jnc",	JAE,		JUMP },
	{ "jne",	JNE,		JUMP },
	{ "jng",	JLE,		JUMP },
	{ "jnge",	JL,		JUMP },
	{ "jnl",	JGE,		JUMP },
	{ "jnle",	JG,		JUMP },
	{ "jno",	JNO,		JUMP },
	{ "jnp",	JNP,		JUMP },
	{ "jns",	JNS,		JUMP },
	{ "jnz",	JNE,		JUMP },
	{ "jo",		JO,		JUMP },
	{ "jp",		JP,		JUMP },
	{ "js",		JS,		JUMP },
	{ "jz",		JE,		JUMP },
	{ "lahf",	LAHF,		WORD },
	{ "lar",	LAR,		WORD },
	{ "lds",	LDS,		WORD },
	{ "lea",	LEA,		WORD },
	{ "leave",	LEAVE,		WORD },
	{ "les",	LES,		WORD },
	{ "lfs",	LFS,		WORD },
	{ "lgdt",	LGDT,		WORD },
	{ "lgs",	LGS,		WORD },
	{ "lidt",	LIDT,		WORD },
	{ "lldt",	LLDT,		WORD },
	{ "lmsw",	LMSW,		WORD },
	{ "lock",	LOCK,		WORD },
	{ "lods",	LODS,		WORD },
	{ "lodsb",	LODS,		BYTE },
	{ "loop",	LOOP,		JUMP },
	{ "loope",	LOOPE,		JUMP },
	{ "loopne",	LOOPNE,		JUMP },
	{ "loopnz",	LOOPNE,		JUMP },
	{ "loopz",	LOOPE,		JUMP },
	{ "lsl",	LSL,		WORD },
	{ "lss",	LSS,		WORD },
	{ "ltr",	LTR,		WORD },
	{ "mov",	MOV,		WORD },
	{ "movb",	MOV,		BYTE },
	{ "movs",	MOVS,		WORD },
	{ "movsb",	MOVS,		BYTE },
	{ "movsx",	MOVSX,		WORD },
	{ "movsxb",	MOVSXB,		WORD },
	{ "movzx",	MOVZX,		WORD },
	{ "movzxb",	MOVZXB,		WORD },
	{ "mul",	MUL,		WORD },
	{ "mulb",	MUL,		BYTE },
	{ "neg",	NEG,		WORD },
	{ "negb",	NEG,		BYTE },
	{ "nop",	NOP,		WORD },
	{ "not",	NOT,		WORD },
	{ "notb",	NOT,		BYTE },
	{ "or",		OR,		WORD },
	{ "orb",	OR,		BYTE },
	{ "out",	OUT,		WORD },
	{ "outb",	OUT,		BYTE },
	{ "outs",	OUTS,		WORD },
	{ "outsb",	OUTS,		BYTE },
	{ "pop",	POP,		WORD },
	{ "popa",	POPA,		WORD },
	{ "popad",	POPA,		WORD },
	{ "popf",	POPF,		WORD },
	{ "push",	PUSH,		WORD },
	{ "pusha",	PUSHA,		WORD },
	{ "pushad",	PUSHA,		WORD },
	{ "pushf",	PUSHF,		WORD },
	{ "rcl",	RCL,		WORD },
	{ "rclb",	RCL,		BYTE },
	{ "rcr",	RCR,		WORD },
	{ "rcrb",	RCR,		BYTE },
	{ "ret",	RET,		JUMP },
	{ "retf",	RETF,		JUMP },
	{ "rol",	ROL,		WORD },
	{ "rolb",	ROL,		BYTE },
	{ "ror",	ROR,		WORD },
	{ "rorb",	ROR,		BYTE },
	{ "sahf",	SAHF,		WORD },
	{ "sal",	SAL,		WORD },
	{ "salb",	SAL,		BYTE },
	{ "sar",	SAR,		WORD },
	{ "sarb",	SAR,		BYTE },
	{ "sbb",	SBB,		WORD },
	{ "sbbb",	SBB,		BYTE },
	{ "scas",	SCAS,		WORD },
	{ "scasb",	SCAS,		BYTE },
	{ "seta",	SETA,		BYTE },
	{ "setae",	SETAE,		BYTE },
	{ "setb",	SETB,		BYTE },
	{ "setbe",	SETBE,		BYTE },
	{ "sete",	SETE,		BYTE },
	{ "setg",	SETG,		BYTE },
	{ "setge",	SETGE,		BYTE },
	{ "setl",	SETL,		BYTE },
	{ "setna",	SETBE,		BYTE },
	{ "setnae",	SETB,		BYTE },
	{ "setnb",	SETAE,		BYTE },
	{ "setnbe",	SETA,		BYTE },
	{ "setne",	SETNE,		BYTE },
	{ "setng",	SETLE,		BYTE },
	{ "setnge",	SETL,		BYTE },
	{ "setnl",	SETGE,		BYTE },
	{ "setnle",	SETG,		BYTE },
	{ "setno",	SETNO,		BYTE },
	{ "setnp",	SETNP,		BYTE },
	{ "setns",	SETNS,		BYTE },
	{ "seto",	SETO,		BYTE },
	{ "setp",	SETP,		BYTE },
	{ "sets",	SETS,		BYTE },
	{ "setz",	SETE,		BYTE },
	{ "sgdt",	SGDT,		WORD },
	{ "shl",	SHL,		WORD },
	{ "shlb",	SHL,		BYTE },
	{ "shld",	SHLD,		WORD },
	{ "shr",	SHR,		WORD },
	{ "shrb",	SHR,		BYTE },
	{ "shrd",	SHRD,		WORD },
	{ "sidt",	SIDT,		WORD },
	{ "sldt",	SLDT,		WORD },
	{ "smsw",	SMSW,		WORD },
	{ "stc",	STC,		WORD },
	{ "std",	STD,		WORD },
	{ "sti",	STI,		WORD },
	{ "stos",	STOS,		WORD },
	{ "stosb",	STOS,		BYTE },
	{ "str",	STR,		WORD },
	{ "sub",	SUB,		WORD },
	{ "subb",	SUB,		BYTE },
	{ "test",	TEST,		WORD },
	{ "testb",	TEST,		BYTE },
	{ "verr",	VERR,		WORD },
	{ "verw",	VERW,		WORD },
	{ "wait",	WAIT,		WORD },
	{ "wbinvd",	WBINVD,		WORD },
	{ "xadd",	XADD,		WORD },
	{ "xchg",	XCHG,		WORD },
	{ "xchgb",	XCHG,		BYTE },
	{ "xlat",	XLAT,		WORD },
	{ "xor",	XOR,		WORD },
	{ "xorb",	XOR,		BYTE },
};

static enum dialect { ACK, NCC } dialect= ACK;

void ack_parse_init(char *file)
/* Prepare parsing of an ACK assembly file. */
{
	tok_init(file, '!');
}

void ncc_parse_init(char *file)
/* Prepare parsing of an ACK Xenix assembly file.  See emit_ack.c for comments
 * on this fine assembly dialect.
 */
{
	dialect= NCC;
	ack_parse_init(file);
}

static void zap(void)
/* An error, zap the rest of the line. */
{
	token_t *t;

	while ((t= get_token(0))->type != T_EOF && t->symbol != ';')
		skip_token(1);
}

static mnemonic_t *search_mnem(char *name)
/* Binary search for a mnemonic.  (That's why the table is sorted.) */
{
	int low, mid, high;
	int cmp;
	mnemonic_t *m;

	low= 0;
	high= arraysize(mnemtab)-1;
	while (low <= high) {
		mid= (low + high) / 2;
		m= &mnemtab[mid];

		if ((cmp= strcmp(name, m->name)) == 0) return m;

		if (cmp < 0) high= mid-1; else low= mid+1;
	}
	return nil;
}

static expression_t *ack_get_C_expression(int *pn)
/* Read a "C-like" expression.  Note that we don't worry about precedence,
 * the expression is printed later like it is read.  If the target language
 * does not have all the operators (like ~) then this has to be repaired by
 * changing the source file.  (No problem, you still have one source file
 * to maintain, not two.)
 */
{
	expression_t *e, *a1, *a2;
	token_t *t;

	if ((t= get_token(*pn))->symbol == '[') {
		/* [ expr ]: grouping. */
		(*pn)++;
		if ((a1= ack_get_C_expression(pn)) == nil) return nil;
		if (get_token(*pn)->symbol != ']') {
			parse_err(1, t, "missing ]\n");
			del_expr(a1);
			return nil;
		}
		(*pn)++;
		e= new_expr();
		e->operator= '[';
		e->middle= a1;
	} else
	if (t->type == T_WORD || t->type == T_STRING) {
		/* Label, number, or string. */
		e= new_expr();
		e->operator= t->type == T_WORD ? 'W' : 'S';
		e->name= allocate(nil, (t->len+1) * sizeof(e->name[0]));
		memcpy(e->name, t->name, t->len+1);
		e->len= t->len;
		(*pn)++;
	} else
	if (t->symbol == '+' || t->symbol == '-' || t->symbol == '~') {
		/* Unary operator. */
		(*pn)++;
		if ((a1= ack_get_C_expression(pn)) == nil) return nil;
		e= new_expr();
		e->operator= t->symbol;
		e->middle= a1;
	} else {
		parse_err(1, t, "expression syntax error\n");
		return nil;
	}

	switch ((t= get_token(*pn))->symbol) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case S_LEFTSHIFT:
	case S_RIGHTSHIFT:
		(*pn)++;
		a1= e;
		if ((a2= ack_get_C_expression(pn)) == nil) {
			del_expr(a1);
			return nil;
		}
		e= new_expr();
		e->operator= t->symbol;
		e->left= a1;
		e->right= a2;
	}
	return e;
}

static expression_t *ack_get_operand(int *pn, int deref)
/* Get something like: (memory), offset(base)(index*scale), or simpler. */
{
	expression_t *e, *offset, *base, *index;
	token_t *t;
	int c;

	/* Is it (memory)? */
	if (get_token(*pn)->symbol == '('
		&& ((t= get_token(*pn + 1))->type != T_WORD
			|| !isregister(t->name))
	) {
		/* A memory dereference. */
		(*pn)++;
		if ((offset= ack_get_C_expression(pn)) == nil) return nil;
		if (get_token(*pn)->symbol != ')') {
			parse_err(1, t, "operand syntax error\n");
			del_expr(offset);
			return nil;
		}
		(*pn)++;
		e= new_expr();
		e->operator= '(';
		e->middle= offset;
		return e;
	}

	/* #constant? */
	if (dialect == NCC && deref
			&& ((c= get_token(*pn)->symbol) == '#' || c == '*')) {
		/* NCC: mov ax,#constant  ->  ACK: mov ax,constant */
		(*pn)++;
		return ack_get_C_expression(pn);
	}

	/* @address? */
	if (dialect == NCC && get_token(*pn)->symbol == '@') {
		/* NCC: jmp @address  ->  ACK: jmp (address) */
		(*pn)++;
		if ((offset= ack_get_operand(pn, deref)) == nil) return nil;
		e= new_expr();
		e->operator= '(';
		e->middle= offset;
		return e;
	}

	/* Offset? */
	if (get_token(*pn)->symbol != '(') {
		/* There is an offset. */
		if ((offset= ack_get_C_expression(pn)) == nil) return nil;
	} else {
		/* No offset. */
		offset= nil;
	}

	/* (base)? */
	if (get_token(*pn)->symbol == '('
		&& (t= get_token(*pn + 1))->type == T_WORD
		&& isregister(t->name)
		&& get_token(*pn + 2)->symbol == ')'
	) {
		/* A base register expression. */
		base= new_expr();
		base->operator= 'B';
		base->name= copystr(t->name);
		(*pn)+= 3;
	} else {
		/* No base register expression. */
		base= nil;
	}

	/* (index*scale)? */
	if (get_token(*pn)->symbol == '(') {
		/* An index most likely. */
		token_t *m= nil;

		if (!(		/* This must be true: */
			(t= get_token(*pn + 1))->type == T_WORD
			&& isregister(t->name)
			&& (get_token(*pn + 2)->symbol == ')' || (
				get_token(*pn + 2)->symbol == '*'
				&& (m= get_token(*pn + 3))->type == T_WORD
				&& strchr("1248", m->name[0]) != nil
				&& m->name[1] == 0
				&& get_token(*pn + 4)->symbol == ')'
			))
		)) {
			/* Alas it isn't */
			parse_err(1, t, "operand syntax error\n");
			del_expr(offset);
			del_expr(base);
			return nil;
		}
		/* Found an index. */
		index= new_expr();
		index->operator= m == nil ? '1' : m->name[0];
		index->name= copystr(t->name);
		(*pn)+= (m == nil ? 3 : 5);
	} else {
		/* No index. */
		index= nil;
	}

	if (dialect == NCC && deref && base == nil && index == nil
		&& !(offset != nil && offset->operator == 'W'
					&& isregister(offset->name))
	) {
		/* NCC: mov ax,thing  ->  ACK mov ax,(thing) */
		e= new_expr();
		e->operator= '(';
		e->middle= offset;
		return e;
	}

	if (base == nil && index == nil) {
		/* Return a lone offset as is. */
		e= offset;
	} else {
		e= new_expr();
		e->operator= 'O';
		e->left= offset;
		e->middle= base;
		e->right= index;
	}
	return e;
}

static expression_t *ack_get_oplist(int *pn, int deref)
/* Get a comma (or colon for jmpf and callf) separated list of instruction
 * operands.
 */
{
	expression_t *e, *o1, *o2;
	token_t *t;

	if ((e= ack_get_operand(pn, deref)) == nil) return nil;

	if ((t= get_token(*pn))->symbol == ',' || t->symbol == ':') {
		o1= e;
		(*pn)++;
		if ((o2= ack_get_oplist(pn, deref)) == nil) {
			del_expr(o1);
			return nil;
		}
		e= new_expr();
		e->operator= ',';
		e->left= o1;
		e->right= o2;
	}
	return e;
}

static asm86_t *ack_get_statement(void)
/* Get a pseudo op or machine instruction with arguments. */
{
	token_t *t= get_token(0);
	asm86_t *a;
	mnemonic_t *m;
	int n;
	int prefix_seen;
	int oaz_prefix;
	int deref;

	assert(t->type == T_WORD);

	if (strcmp(t->name, ".sect") == 0) {
		/* .sect .text etc.  Accept only four segment names. */
		skip_token(1);
		t= get_token(0);
		if (t->type != T_WORD || (
			strcmp(t->name, ".text") != 0
			&& strcmp(t->name, ".rom") != 0
			&& strcmp(t->name, ".data") != 0
			&& strcmp(t->name, ".bss") != 0
			&& strcmp(t->name, ".end") != 0
		)) {
			parse_err(1, t, "weird section name to .sect\n");
			return nil;
		}
	}
	a= new_asm86();

	/* Process instruction prefixes. */
	oaz_prefix= 0;
	for (prefix_seen= 0;; prefix_seen= 1) {
		if (strcmp(t->name, "o16") == 0) {
			if (use16()) {
				parse_err(1, t, "o16 in an 8086 section\n");
			}
			oaz_prefix|= OPZ;
		} else
		if (strcmp(t->name, "o32") == 0) {
			if (use32()) {
				parse_err(1, t, "o32 in an 80386 section\n");
			}
			oaz_prefix|= OPZ;
		} else
		if (strcmp(t->name, "a16") == 0) {
			if (use16()) {
				parse_err(1, t, "a16 in an 8086 section\n");
			}
			oaz_prefix|= ADZ;
		} else
		if (strcmp(t->name, "a32") == 0) {
			if (use32()) {
				parse_err(1, t, "a32 in an 80386 section\n");
			}
			oaz_prefix|= ADZ;
		} else
		if (strcmp(t->name, "rep") == 0
			|| strcmp(t->name, "repe") == 0
			|| strcmp(t->name, "repne") == 0
			|| strcmp(t->name, "repz") == 0
			|| strcmp(t->name, "repnz") == 0
		) {
			if (a->rep != ONCE) {
				parse_err(1, t,
					"can't have more than one rep\n");
			}
			switch (t->name[3]) {
			case 0:		a->rep= REP;	break;
			case 'e':
			case 'z':	a->rep= REPE;	break;
			case 'n':	a->rep= REPNE;	break;
			}
		} else
		if (strchr("cdefgs", t->name[0]) != nil
					&& strcmp(t->name+1, "seg") == 0) {
			if (a->seg != DEFSEG) {
				parse_err(1, t,
				"can't have more than one segment prefix\n");
			}
			switch (t->name[0]) {
			case 'c':	a->seg= CSEG;	break;
			case 'd':	a->seg= DSEG;	break;
			case 'e':	a->seg= ESEG;	break;
			case 'f':	a->seg= FSEG;	break;
			case 'g':	a->seg= GSEG;	break;
			case 's':	a->seg= SSEG;	break;
			}
		} else
		if (!prefix_seen) {
			/* No prefix here, get out! */
			break;
		} else {
			/* No more prefixes, next must be an instruction. */
			if (t->type != T_WORD
				|| (m= search_mnem(t->name)) == nil
				|| m->optype == PSEUDO
			) {
				parse_err(1, t,
		"machine instruction expected after instruction prefix\n");
				del_asm86(a);
				return nil;
			}
			if (oaz_prefix != 0 && m->optype != JUMP
						&& m->optype != WORD) {
				parse_err(1, t,
			"'%s' can't have an operand size prefix\n", m->name);
			}
			break;
		}

		/* Skip the prefix and extra newlines. */
		do {
			skip_token(1);
		} while ((t= get_token(0))->symbol == ';');
	}

	/* All the readahead being done upsets the line counter. */
	a->line= t->line;

	/* Read a machine instruction or pseudo op. */
	if ((m= search_mnem(t->name)) == nil) {
		parse_err(1, t, "unknown instruction '%s'\n", t->name);
		del_asm86(a);
		return nil;
	}
	a->opcode= m->opcode;
	a->optype= m->optype;
	a->oaz= oaz_prefix;

	switch (a->opcode) {
	case IN:
	case OUT:
	case INT:
		deref= 0;
		break;
	default:
		deref= (a->optype >= BYTE);
	}
	n= 1;
	if (get_token(1)->symbol != ';'
			&& (a->args= ack_get_oplist(&n, deref)) == nil) {
		del_asm86(a);
		return nil;
	}
	if (get_token(n)->symbol != ';') {
		parse_err(1, t, "garbage at end of instruction\n");
		del_asm86(a);
		return nil;
	}
	switch (a->opcode) {
	case DOT_ALIGN:
		/* Restrict .align to have a single numeric argument, some
		 * assemblers think of the argument as a power of two, so
		 * we need to be able to change the value.
		 */
		if (a->args == nil || a->args->operator != 'W'
					|| !isanumber(a->args->name)) {
			parse_err(1, t,
			  ".align is restricted to one numeric argument\n");
			del_asm86(a);
			return nil;
		}
		break;
	case JMPF:
	case CALLF:
		/* NCC jmpf off,seg  ->  ACK jmpf seg:off */
		if (dialect == NCC && a->args != nil
						&& a->args->operator == ',') {
			expression_t *t;

			t= a->args->left;
			a->args->left= a->args->right;
			a->args->right= t;
			break;
		}
		/*FALL THROUGH*/
	case JMP:
	case CALL:
		/* NCC jmp @(reg)  ->  ACK jmp (reg) */
		if (dialect == NCC && a->args != nil && (
			(a->args->operator == '('
				&& a->args->middle != nil
				&& a->args->middle->operator == 'O')
			|| (a->args->operator == 'O'
				&& a->args->left == nil
				&& a->args->middle != nil
				&& a->args->right == nil)
		)) {
			expression_t *t;

			t= a->args;
			a->args= a->args->middle;
			t->middle= nil;
			del_expr(t);
			if (a->args->operator == 'B') a->args->operator= 'W';
		}
		break;
	default:;
	}
	skip_token(n+1);
	return a;
}

asm86_t *ack_get_instruction(void)
{
	asm86_t *a= nil;
	expression_t *e;
	token_t *t;

	while ((t= get_token(0))->symbol == ';')
		skip_token(1);

	if (t->type == T_EOF) return nil;

	if (t->symbol == '#') {
		/* Preprocessor line and file change. */

		if ((t= get_token(1))->type != T_WORD || !isanumber(t->name)
			|| get_token(2)->type != T_STRING
		) {
			parse_err(1, t, "file not preprocessed?\n");
			zap();
		} else {
			set_file(get_token(2)->name,
				strtol(get_token(1)->name, nil, 0) - 1);

			/* GNU CPP adds extra cruft, simply zap the line. */
			zap();
		}
		a= ack_get_instruction();
	} else
	if (t->type == T_WORD && get_token(1)->symbol == ':') {
		/* A label definition. */
		a= new_asm86();
		a->line= t->line;
		a->opcode= DOT_LABEL;
		a->optype= PSEUDO;
		a->args= e= new_expr();
		e->operator= ':';
		e->name= copystr(t->name);
		skip_token(2);
	} else
	if (t->type == T_WORD && get_token(1)->symbol == '=') {
		int n= 2;

		if ((e= ack_get_C_expression(&n)) == nil) {
			zap();
			a= ack_get_instruction();
		} else
		if (get_token(n)->symbol != ';') {
			parse_err(1, t, "garbage after assignment\n");
			zap();
			a= ack_get_instruction();
		} else {
			a= new_asm86();
			a->line= t->line;
			a->opcode= DOT_EQU;
			a->optype= PSEUDO;
			a->args= new_expr();
			a->args->operator= '=';
			a->args->name= copystr(t->name);
			a->args->middle= e;
			skip_token(n+1);
		}
	} else
	if (t->type == T_WORD) {
		if ((a= ack_get_statement()) == nil) {
			zap();
			a= ack_get_instruction();
		}
	} else {
		parse_err(1, t, "syntax error\n");
		zap();
		a= ack_get_instruction();
	}
	return a;
}

asm86_t *ncc_get_instruction(void)
{
	return ack_get_instruction();
}
