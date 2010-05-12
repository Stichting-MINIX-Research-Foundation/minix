/*	parse_bas.c - parse BCC AS assembly		Author: Kees J. Bot
 *								13 Nov 1994
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

typedef struct mnemonic {	/* BAS mnemonics translation table. */
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
	{ ".blkb",	DOT_SPACE,	PSEUDO },
	{ ".bss",	DOT_BSS,	PSEUDO },
	{ ".byte",	DOT_DATA1,	PSEUDO },
	{ ".comm",	DOT_COMM,	PSEUDO },
	{ ".data",	DOT_DATA,	PSEUDO },
	{ ".define",	DOT_DEFINE,	PSEUDO },
	{ ".end",	DOT_END,	PSEUDO },
	{ ".even",	DOT_ALIGN,	PSEUDO },
	{ ".extern",	DOT_EXTERN,	PSEUDO },
	{ ".file",	DOT_FILE,	PSEUDO },
	{ ".globl",	DOT_DEFINE,	PSEUDO },
	{ ".lcomm",	DOT_LCOMM,	PSEUDO },
	{ ".line",	DOT_LINE,	PSEUDO },
	{ ".list",	DOT_LIST,	PSEUDO },
	{ ".long",	DOT_DATA4,	PSEUDO },
	{ ".nolist",	DOT_NOLIST,	PSEUDO },
	{ ".rom",	DOT_ROM,	PSEUDO },
	{ ".space",	DOT_SPACE,	PSEUDO },
	{ ".symb",	DOT_SYMB,	PSEUDO },
	{ ".text",	DOT_TEXT,	PSEUDO },
	{ ".use16",	DOT_USE16,	PSEUDO },
	{ ".use32",	DOT_USE32,	PSEUDO },
	{ ".word",	DOT_DATA2,	PSEUDO },
	{ ".zerob",	DOT_SPACE,	PSEUDO },
	{ ".zerow",	DOT_SPACE,	PSEUDO },
	{ "aaa",	AAA,		WORD },
	{ "aad",	AAD,		WORD },
	{ "aam",	AAM,		WORD },
	{ "aas",	AAS,		WORD },
	{ "adc",	ADC,		WORD },
	{ "add",	ADD,		WORD },
	{ "and",	AND,		WORD },
	{ "arpl",	ARPL,		WORD },
	{ "bc",		JB,		JUMP },
	{ "beq",	JE,		JUMP },
	{ "bge",	JGE,		JUMP },
	{ "bgt",	JG,		JUMP },
	{ "bhi",	JA,		JUMP },
	{ "bhis",	JAE,		JUMP },
	{ "ble",	JLE,		JUMP },
	{ "blo",	JB,		JUMP },
	{ "blos",	JBE,		JUMP },
	{ "blt",	JL,		JUMP },
	{ "bnc",	JAE,		JUMP },
	{ "bne",	JNE,		JUMP },
	{ "bound",	BOUND,		WORD },
	{ "br",		JMP,		JUMP },
	{ "bsf",	BSF,		WORD },
	{ "bsr",	BSR,		WORD },
	{ "bswap",	BSWAP,		WORD },
	{ "bt",		BT,		WORD },
	{ "btc",	BTC,		WORD },
	{ "btr",	BTR,		WORD },
	{ "bts",	BTS,		WORD },
	{ "bz",		JE,		JUMP },
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
	{ "cmps",	CMPS,		WORD },
	{ "cmpsb",	CMPS,		BYTE },
	{ "cmpxchg",	CMPXCHG,	WORD },
	{ "cwd",	CWD,		WORD },
	{ "cwde",	CBW,		WORD },
	{ "daa",	DAA,		WORD },
	{ "das",	DAS,		WORD },
	{ "dd",		DOT_DATA4,	PSEUDO },
	{ "dec",	DEC,		WORD },
	{ "div",	DIV,		WORD },
	{ "enter",	ENTER,		WORD },
	{ "export",	DOT_DEFINE,	PSEUDO },
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
	{ "imul",	IMUL,		WORD },
	{ "in",		IN,		WORD },
	{ "inb",	IN,		BYTE },
	{ "inc",	INC,		WORD },
	{ "ins",	INS,		WORD },
	{ "insb",	INS,		BYTE },
	{ "int",	INT,		WORD },
	{ "into",	INTO,		JUMP },
	{ "invd",	INVD,		WORD },
	{ "invlpg",	INVLPG,		WORD },
	{ "iret",	IRET,		JUMP },
	{ "iretd",	IRETD,		JUMP },
	{ "j",		JMP,		JUMP },
	{ "ja",		JA,		JUMP },
	{ "jae",	JAE,		JUMP },
	{ "jb",		JB,		JUMP },
	{ "jbe",	JBE,		JUMP },
	{ "jc",		JB,		JUMP },
	{ "jcxz",	JCXZ,		JUMP },
	{ "je",		JE,		JUMP },
	{ "jecxz",	JCXZ,		JUMP },
	{ "jeq",	JE,		JUMP },
	{ "jg",		JG,		JUMP },
	{ "jge",	JGE,		JUMP },
	{ "jgt",	JG,		JUMP },
	{ "jhi",	JA,		JUMP },
	{ "jhis",	JAE,		JUMP },
	{ "jl",		JL,		JUMP },
	{ "jle",	JLE,		JUMP },
	{ "jlo",	JB,		JUMP },
	{ "jlos",	JBE,		JUMP },
	{ "jlt",	JL,		JUMP },
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
	{ "movs",	MOVS,		WORD },
	{ "movsb",	MOVS,		BYTE },
	{ "movsx",	MOVSX,		WORD },
	{ "movzx",	MOVZX,		WORD },
	{ "mul",	MUL,		WORD },
	{ "neg",	NEG,		WORD },
	{ "nop",	NOP,		WORD },
	{ "not",	NOT,		WORD },
	{ "or",		OR,		WORD },
	{ "out",	OUT,		WORD },
	{ "outb",	OUT,		BYTE },
	{ "outs",	OUTS,		WORD },
	{ "outsb",	OUTS,		BYTE },
	{ "pop",	POP,		WORD },
	{ "popa",	POPA,		WORD },
	{ "popad",	POPA,		WORD },
	{ "popf",	POPF,		WORD },
	{ "popfd",	POPF,		WORD },
	{ "push",	PUSH,		WORD },
	{ "pusha",	PUSHA,		WORD },
	{ "pushad",	PUSHA,		WORD },
	{ "pushf",	PUSHF,		WORD },
	{ "pushfd",	PUSHF,		WORD },
	{ "rcl",	RCL,		WORD },
	{ "rcr",	RCR,		WORD },
	{ "ret",	RET,		JUMP },
	{ "retf",	RETF,		JUMP },
	{ "rol",	ROL,		WORD },
	{ "ror",	ROR,		WORD },
	{ "sahf",	SAHF,		WORD },
	{ "sal",	SAL,		WORD },
	{ "sar",	SAR,		WORD },
	{ "sbb",	SBB,		WORD },
	{ "scas",	SCAS,		WORD },
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
	{ "shld",	SHLD,		WORD },
	{ "shr",	SHR,		WORD },
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
	{ "test",	TEST,		WORD },
	{ "verr",	VERR,		WORD },
	{ "verw",	VERW,		WORD },
	{ "wait",	WAIT,		WORD },
	{ "wbinvd",	WBINVD,		WORD },
	{ "xadd",	XADD,		WORD },
	{ "xchg",	XCHG,		WORD },
	{ "xlat",	XLAT,		WORD },
	{ "xor",	XOR,		WORD },
};

void bas_parse_init(char *file)
/* Prepare parsing of an BAS assembly file. */
{
	tok_init(file, '!');
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

static expression_t *bas_get_C_expression(int *pn)
/* Read a "C-like" expression.  Note that we don't worry about precedence,
 * the expression is printed later like it is read.  If the target language
 * does not have all the operators (like ~) then this has to be repaired by
 * changing the source file.  (No problem, you still have one source file
 * to maintain, not two.)
 */
{
	expression_t *e, *a1, *a2;
	token_t *t;

	if ((t= get_token(*pn))->symbol == '(') {
		/* ( expr ): grouping. */
		(*pn)++;
		if ((a1= bas_get_C_expression(pn)) == nil) return nil;
		if (get_token(*pn)->symbol != ')') {
			parse_err(1, t, "missing )\n");
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
		if ((a1= bas_get_C_expression(pn)) == nil) return nil;
		e= new_expr();
		e->operator= t->symbol;
		e->middle= a1;
	} else
	if (t->symbol == '$' && get_token(*pn + 1)->type == T_WORD) {
		/* A hexadecimal number. */
		t= get_token(*pn + 1);
		e= new_expr();
		e->operator= 'W';
		e->name= allocate(nil, (t->len+3) * sizeof(e->name[0]));
		strcpy(e->name, "0x");
		memcpy(e->name+2, t->name, t->len+1);
		e->len= t->len+2;
		(*pn)+= 2;
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
		if ((a2= bas_get_C_expression(pn)) == nil) {
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

/* We want to know the sizes of the first two operands. */
static optype_t optypes[2];
static int op_idx;

static expression_t *bas_get_operand(int *pn)
/* Get something like: [memory], offset[base+index*scale], or simpler. */
{
	expression_t *e, *offset, *base, *index;
	token_t *t;
	int c;
	optype_t optype;

	/* Prefixed by 'byte', 'word' or 'dword'? */
	if ((t= get_token(*pn))->type == T_WORD && (
		strcmp(t->name, "byte") == 0
		|| strcmp(t->name, "word") == 0
		|| strcmp(t->name, "dword") == 0)
	) {
		switch (t->name[0]) {
		case 'b':	optype= BYTE; break;
		case 'w':	optype= use16() ? WORD : OWORD; break;
		case 'd':	optype= use32() ? WORD : OWORD; break;
		}
		if (op_idx < arraysize(optypes)) optypes[op_idx++]= optype;
		(*pn)++;

		/* It may even be "byte ptr"... */
		if ((t= get_token(*pn))->type == T_WORD
					&& strcmp(t->name, "ptr") == 0) {
			(*pn)++;
		}
	}

	/* Is it [memory]? */
	if (get_token(*pn)->symbol == '['
		&& ((t= get_token(*pn + 1))->type != T_WORD
			|| !isregister(t->name))
	) {
		/* A memory dereference. */
		(*pn)++;
		if ((offset= bas_get_C_expression(pn)) == nil) return nil;
		if (get_token(*pn)->symbol != ']') {
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

	/* #something? *something? */
	if ((c= get_token(*pn)->symbol) == '#' || c == '*') {
		/* '#' and '*' are often used to introduce some constant. */
		(*pn)++;
	}

	/* Offset? */
	if (get_token(*pn)->symbol != '[') {
		/* There is an offset. */
		if ((offset= bas_get_C_expression(pn)) == nil) return nil;
	} else {
		/* No offset. */
		offset= nil;
	}

	/* [base]? [base+? base-? */
	c= 0;
	if (get_token(*pn)->symbol == '['
		&& (t= get_token(*pn + 1))->type == T_WORD
		&& isregister(t->name)
		&& ((c= get_token(*pn + 2)->symbol) == ']' || c=='+' || c=='-')
	) {
		/* A base register expression. */
		base= new_expr();
		base->operator= 'B';
		base->name= copystr(t->name);
		(*pn)+= c == ']' ? 3 : 2;
	} else {
		/* No base register expression. */
		base= nil;
	}

	/* +offset]? -offset]? */
	if (offset == nil
		&& (c == '+' || c == '-')
		&& (t= get_token(*pn + 1))->type == T_WORD
		&& !isregister(t->name)
	) {
		(*pn)++;
		if ((offset= bas_get_C_expression(pn)) == nil) return nil;
		if (get_token(*pn)->symbol != ']') {
			parse_err(1, t, "operand syntax error\n");
			del_expr(offset);
			del_expr(base);
			return nil;
		}
		(*pn)++;
		c= 0;
	}

	/* [index*scale]? +index*scale]? */
	if (c == '+' || get_token(*pn)->symbol == '[') {
		/* An index most likely. */
		token_t *m= nil;

		if (!(		/* This must be true: */
			(t= get_token(*pn + 1))->type == T_WORD
			&& isregister(t->name)
			&& (get_token(*pn + 2)->symbol == ']' || (
				get_token(*pn + 2)->symbol == '*'
				&& (m= get_token(*pn + 3))->type == T_WORD
				&& strchr("1248", m->name[0]) != nil
				&& m->name[1] == 0
				&& get_token(*pn + 4)->symbol == ']'
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

	if (base == nil && index == nil) {
		/* Return a lone offset as is. */
		e= offset;

		/* Lone registers tell operand size. */
		if (offset->operator == 'W' && isregister(offset->name)) {
			switch (isregister(offset->name)) {
			case 1:	optype= BYTE; break;
			case 2:	optype= use16() ? WORD : OWORD; break;
			case 4:	optype= use32() ? WORD : OWORD; break;
			}
			if (op_idx < arraysize(optypes))
				optypes[op_idx++]= optype;
		}
	} else {
		e= new_expr();
		e->operator= 'O';
		e->left= offset;
		e->middle= base;
		e->right= index;
	}
	return e;
}

static expression_t *bas_get_oplist(int *pn)
/* Get a comma (or colon for jmpf and callf) separated list of instruction
 * operands.
 */
{
	expression_t *e, *o1, *o2;
	token_t *t;

	if ((e= bas_get_operand(pn)) == nil) return nil;

	if ((t= get_token(*pn))->symbol == ',' || t->symbol == ':') {
		o1= e;
		(*pn)++;
		if ((o2= bas_get_oplist(pn)) == nil) {
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

static asm86_t *bas_get_statement(void)
/* Get a pseudo op or machine instruction with arguments. */
{
	token_t *t= get_token(0);
	asm86_t *a;
	mnemonic_t *m;
	int n;
	int prefix_seen;


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
	for (prefix_seen= 0;; prefix_seen= 1) {
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
		if (strcmp(t->name, "seg") == 0
					&& get_token(1)->type == T_WORD) {
			if (a->seg != DEFSEG) {
				parse_err(1, t,
				"can't have more than one segment prefix\n");
			}
			switch (get_token(1)->name[0]) {
			case 'c':	a->seg= CSEG;	break;
			case 'd':	a->seg= DSEG;	break;
			case 'e':	a->seg= ESEG;	break;
			case 'f':	a->seg= FSEG;	break;
			case 'g':	a->seg= GSEG;	break;
			case 's':	a->seg= SSEG;	break;
			}
			skip_token(1);
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
	if (a->opcode == CBW || a->opcode == CWD) {
		a->optype= (strcmp(t->name, "cbw") == 0
		    || strcmp(t->name, "cwd") == 0) == use16() ? WORD : OWORD;
	}
	for (op_idx= 0; op_idx < arraysize(optypes); op_idx++)
		optypes[op_idx]= m->optype;
	op_idx= 0;

	n= 1;
	if (get_token(1)->symbol != ';'
				&& (a->args= bas_get_oplist(&n)) == nil) {
		del_asm86(a);
		return nil;
	}

	if (m->optype == WORD) {
		/* Does one of the operands overide the optype? */
		for (op_idx= 0; op_idx < arraysize(optypes); op_idx++) {
			if (optypes[op_idx] != m->optype)
				a->optype= optypes[op_idx];
		}
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
		if (strcmp(t->name, ".even") == 0 && a->args == nil) {
			/* .even becomes .align 2. */
			expression_t *e;
			a->args= e= new_expr();
			e->operator= 'W';
			e->name= copystr("2");
			e->len= 2;
		}
		if (a->args == nil || a->args->operator != 'W'
					|| !isanumber(a->args->name)) {
			parse_err(1, t,
			  ".align is restricted to one numeric argument\n");
			del_asm86(a);
			return nil;
		}
		break;
	case MOVSX:
	case MOVZX:
		/* Types of both operands tell the instruction type. */
		a->optype= optypes[0];
		if (optypes[1] == BYTE) {
			a->opcode= a->opcode == MOVSX ? MOVSXB : MOVZXB;
		}
		break;
	case SAL:
	case SAR:
	case SHL:
	case SHR:
	case RCL:
	case RCR:
	case ROL:
	case ROR:
		/* Only the first operand tells the operand size. */
		a->optype= optypes[0];
		break;
	default:;
	}
	skip_token(n+1);
	return a;
}

asm86_t *bas_get_instruction(void)
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
		a= bas_get_instruction();
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

		if ((e= bas_get_C_expression(&n)) == nil) {
			zap();
			a= bas_get_instruction();
		} else
		if (get_token(n)->symbol != ';') {
			parse_err(1, t, "garbage after assignment\n");
			zap();
			a= bas_get_instruction();
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
	if (t->type == T_WORD && get_token(1)->type == T_WORD
				&& strcmp(get_token(1)->name, "lcomm") == 0) {
		/* Local common block definition. */
		int n= 2;

		if ((e= bas_get_C_expression(&n)) == nil) {
			zap();
			a= bas_get_instruction();
		} else
		if (get_token(n)->symbol != ';') {
			parse_err(1, t, "garbage after lcomm\n");
			zap();
			a= bas_get_instruction();
		} else {
			a= new_asm86();
			a->line= t->line;
			a->opcode= DOT_LCOMM;
			a->optype= PSEUDO;
			a->args= new_expr();
			a->args->operator= ',';
			a->args->right= e;
			a->args->left= e= new_expr();
			e->operator= 'W';
			e->name= copystr(t->name);
			e->len= strlen(e->name)+1;
			skip_token(n+1);
		}
	} else
	if (t->type == T_WORD) {
		if ((a= bas_get_statement()) == nil) {
			zap();
			a= bas_get_instruction();
		}
	} else {
		parse_err(1, t, "syntax error\n");
		zap();
		a= bas_get_instruction();
	}
	if (a->optype == OWORD) {
		a->optype= WORD;
		a->oaz|= OPZ;
	}
	return a;
}
