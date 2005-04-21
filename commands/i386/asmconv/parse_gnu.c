/*	parse_ack.c - parse GNU assembly		Author: R.S. Veldema
 *							 <rveldema@cs.vu.nl>
 *								26 Aug 1996
 */
#define nil 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "asmconv.h"
#include "token.h"
#include "asm86.h"
#include "languages.h"

typedef struct mnemonic {	/* GNU as86 mnemonics translation table. */
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
	{ ".byte",	DOT_DATA1,	PSEUDO },
	{ ".comm",	DOT_COMM,	PSEUDO },
	{ ".data",	DOT_DATA,	PSEUDO },
	{ ".data1",	DOT_DATA1,	PSEUDO },
	{ ".data2",	DOT_DATA2,	PSEUDO },
	{ ".data4",	DOT_DATA4,	PSEUDO },
	{ ".end",	DOT_END,	PSEUDO },
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
	{ ".word",	DOT_DATA2,	PSEUDO },
	{ "aaa",	AAA,		WORD },
	{ "aad",	AAD,		WORD },
	{ "aam",	AAM,		WORD },
	{ "aas",	AAS,		WORD },
	{ "adcb",	ADC,		BYTE },
	{ "adcl",	ADC,		WORD },
	{ "adcw",	ADC,		OWORD },
	{ "addb",	ADD,		BYTE },
	{ "addl",	ADD,		WORD },
	{ "addw",	ADD,		OWORD },
	{ "andb",	AND,		BYTE },
	{ "andl",	AND,		WORD },
	{ "andw",	AND,		OWORD },
	{ "arpl",	ARPL,		WORD },
	{ "bound",	BOUND,		WORD },
	{ "bsf",	BSF,		WORD },
	{ "bsr",	BSR,		WORD },
	{ "bswap",	BSWAP,		WORD },
	{ "btc",	BTC,		WORD },
	{ "btl",	BT,		WORD },
	{ "btr",	BTR,		WORD },
	{ "bts",	BTS,		WORD },
	{ "btw",	BT,		OWORD },
	{ "call",	CALL,		JUMP },
	{ "callf",	CALLF,		JUMP },
	{ "cbtw",	CBW,		OWORD },
	{ "cbw",	CBW,		WORD },
	{ "cdq",	CWD,		WORD },
	{ "clc",	CLC,		WORD },
	{ "cld",	CLD,		WORD },
	{ "cli",	CLI,		WORD },
	{ "cltd",	CWD,		WORD },
	{ "clts",	CLTS,		WORD },
	{ "cmc",	CMC,		WORD },
	{ "cmpb",	CMP,		BYTE },
	{ "cmpl",	CMP,		WORD },
	{ "cmps",	CMPS,		WORD },
	{ "cmpsb",	CMPS,		BYTE },
	{ "cmpw",	CMP,		OWORD },
	{ "cmpxchg",	CMPXCHG,	WORD },
	{ "cwd",	CWD,		WORD },
	{ "cwde",	CBW,		WORD },
	{ "cwtd",	CWD,		OWORD },
	{ "cwtl",	CBW,		WORD },
	{ "daa",	DAA,		WORD },
	{ "das",	DAS,		WORD },
	{ "decb",	DEC,		BYTE },
	{ "decl",	DEC,		WORD },
	{ "decw",	DEC,		OWORD },
	{ "divb",	DIV,		BYTE },
	{ "divl",	DIV,		WORD },
	{ "divw",	DIV,		OWORD },
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
	{ "fnclex",	FCLEX,		WORD },
	{ "fninit",	FINIT,		WORD },
	{ "fnop",	FNOP,		WORD },
	{ "fnsave",	FSAVE,		WORD },
	{ "fnstcw",	FSTCW,		WORD },
	{ "fnstenv",	FSTENV,		WORD },
	{ "fpatan",	FPATAN,		WORD },
	{ "fprem",	FPREM,		WORD },
	{ "fprem1",	FPREM1,		WORD },
	{ "fptan",	FPTAN,		WORD },
	{ "frndint",	FRNDINT,	WORD },
	{ "frstor",	FRSTOR,		WORD },
	{ "fscale",	FSCALE,		WORD },
	{ "fsin",	FSIN,		WORD },
	{ "fsincos",	FSINCOS,	WORD },
	{ "fsqrt",	FSQRT,		WORD },
	{ "fstd",	FSTD,		WORD },
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
	{ "idivb",	IDIV,		BYTE },
	{ "idivl",	IDIV,		WORD },
	{ "idivw",	IDIV,		OWORD },
	{ "imulb",	IMUL,		BYTE },
	{ "imull",	IMUL,		WORD },
	{ "imulw",	IMUL,		OWORD },
	{ "inb",	IN,		BYTE },
	{ "incb",	INC,		BYTE },
	{ "incl",	INC,		WORD },
	{ "incw",	INC,		OWORD },
	{ "inl",	IN,		WORD },
	{ "insb",	INS,		BYTE },
	{ "insl",	INS,		WORD },
	{ "insw",	INS,		OWORD },
	{ "int",	INT,		WORD },
	{ "into",	INTO,		JUMP },
	{ "invd",	INVD,		WORD },
	{ "invlpg",	INVLPG,		WORD },
	{ "inw",	IN,		OWORD },
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
	{ "leal",	LEA,		WORD },
	{ "leave",	LEAVE,		WORD },
	{ "leaw",	LEA,		OWORD },
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
	{ "movb",	MOV,		BYTE },
	{ "movl",	MOV,		WORD },
	{ "movsb",	MOVS,		BYTE },
	{ "movsbl",	MOVSXB,		WORD },
	{ "movsbw",	MOVSXB,		OWORD },
	{ "movsl",	MOVS,		WORD },
	{ "movsw",	MOVS,		OWORD },
	{ "movswl",	MOVSX,		WORD },
	{ "movw",	MOV,		OWORD },
	{ "movzbl",	MOVZXB,		WORD },
	{ "movzbw",	MOVZXB,		OWORD },
	{ "movzwl",	MOVZX,		WORD },
	{ "mulb",	MUL,		BYTE },
	{ "mull",	MUL,		WORD },
	{ "mulw",	MUL,		OWORD },
	{ "negb",	NEG,		BYTE },
	{ "negl",	NEG,		WORD },
	{ "negw",	NEG,		OWORD },
	{ "nop",	NOP,		WORD },
	{ "notb",	NOT,		BYTE },
	{ "notl",	NOT,		WORD },
	{ "notw",	NOT,		OWORD },
	{ "orb",	OR,		BYTE },
	{ "orl",	OR,		WORD },
	{ "orw",	OR,		OWORD },
	{ "outb",	OUT,		BYTE },
	{ "outl",	OUT,		WORD },
	{ "outsb",	OUTS,		BYTE },
	{ "outsl",	OUTS,		WORD },
	{ "outsw",	OUTS,		OWORD },
	{ "outw",	OUT,		OWORD },
	{ "pop",	POP,		WORD },
	{ "popa",	POPA,		WORD },
	{ "popad",	POPA,		WORD },
	{ "popf",	POPF,		WORD },
	{ "popl",	POP,		WORD },
	{ "push",	PUSH,		WORD },
	{ "pusha",	PUSHA,		WORD },
	{ "pushad",	PUSHA,		WORD },
	{ "pushf",	PUSHF,		WORD },
	{ "pushl",	PUSH,		WORD },
	{ "rclb",	RCL,		BYTE },
	{ "rcll",	RCL,		WORD },
	{ "rclw",	RCL,		OWORD },
	{ "rcrb",	RCR,		BYTE },
	{ "rcrl",	RCR,		WORD },
	{ "rcrw",	RCR,		OWORD },
	{ "ret",	RET,		JUMP },
	{ "retf",	RETF,		JUMP },
	{ "rolb",	ROL,		BYTE },
	{ "roll",	ROL,		WORD },
	{ "rolw",	ROL,		OWORD },
	{ "rorb",	ROR,		BYTE },
	{ "rorl",	ROR,		WORD },
	{ "rorw",	ROR,		OWORD },
	{ "sahf",	SAHF,		WORD },
	{ "salb",	SAL,		BYTE },
	{ "sall",	SAL,		WORD },
	{ "salw",	SAL,		OWORD },
	{ "sarb",	SAR,		BYTE },
	{ "sarl",	SAR,		WORD },
	{ "sarw",	SAR,		OWORD },
	{ "sbbb",	SBB,		BYTE },
	{ "sbbl",	SBB,		WORD },
	{ "sbbw",	SBB,		OWORD },
	{ "scasb",	SCAS,		BYTE },
	{ "scasl",	SCAS,		WORD },
	{ "scasw",	SCAS,		OWORD },
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
	{ "shlb",	SHL,		BYTE },
	{ "shldl",	SHLD,		WORD },
	{ "shll",	SHL,		WORD },
	{ "shlw",	SHL,		OWORD },
	{ "shrb",	SHR,		BYTE },
	{ "shrdl",	SHRD,		WORD },
	{ "shrl",	SHR,		WORD },
	{ "shrw",	SHR,		OWORD },
	{ "sidt",	SIDT,		WORD },
	{ "sldt",	SLDT,		WORD },
	{ "smsw",	SMSW,		WORD },
	{ "stc",	STC,		WORD },
	{ "std",	STD,		WORD },
	{ "sti",	STI,		WORD },
	{ "stosb",	STOS,		BYTE },
	{ "stosl",	STOS,		WORD },
	{ "stosw",	STOS,		OWORD },
	{ "str",	STR,		WORD },
	{ "subb",	SUB,		BYTE },
	{ "subl",	SUB,		WORD },
	{ "subw",	SUB,		OWORD },
	{ "testb",	TEST,		BYTE },
	{ "testl",	TEST,		WORD },
	{ "testw",	TEST,		OWORD },
	{ "verr",	VERR,		WORD },
	{ "verw",	VERW,		WORD },
	{ "wait",	WAIT,		WORD },
	{ "wbinvd",	WBINVD,		WORD },
	{ "xadd",	XADD,		WORD },
	{ "xchgb",	XCHG,		BYTE },
	{ "xchgl",	XCHG,		WORD },
	{ "xchgw",	XCHG,		OWORD },
	{ "xlat",	XLAT,		WORD },
	{ "xorb",	XOR,		BYTE },
	{ "xorl",	XOR,		WORD },
	{ "xorw",	XOR,		OWORD },
};

void gnu_parse_init(char *file)
/* Prepare parsing of an GNU assembly file. */
{
	tok_init(file, '#');
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

static expression_t *gnu_get_C_expression(int *pn)
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
		if ((a1= gnu_get_C_expression(pn)) == nil) return nil;
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
		memcpy(e->name, t->name , t->len+1);
		e->len= t->len;
		(*pn)++;
	} else
	if (t->symbol == '+' || t->symbol == '-' || t->symbol == '~') {
		/* Unary operator. */
		(*pn)++;
		if ((a1= gnu_get_C_expression(pn)) == nil) return nil;
		e= new_expr();
		e->operator= t->symbol;
		e->middle= a1;
	} else {
		parse_err(1, t, "expression syntax error\n");
		return nil;
	}

	switch ((t= get_token(*pn))->symbol) {
	case '%': 
	case '+':
	case '-':
	case '*':
	case '/':
	case '&':
	case '|':
	case '^':
	case S_LEFTSHIFT:
	case S_RIGHTSHIFT:
		(*pn)++;
		a1= e;
		if ((a2= gnu_get_C_expression(pn)) == nil) {
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

static expression_t *gnu_get_operand(int *pn, int deref)
/* Get something like: $immed, memory, offset(%base,%index,scale), or simpler. */
{
	expression_t *e, *offset, *base, *index;
	token_t *t;
	int c;

	if (get_token(*pn)->symbol == '$') {
		/* An immediate value. */
		(*pn)++;
		return gnu_get_C_expression(pn);
	}

	if (get_token(*pn)->symbol == '*') {
		/* Indirection. */
		(*pn)++;
		if ((offset= gnu_get_operand(pn, deref)) == nil) return nil;
		e= new_expr();
		e->operator= '(';
		e->middle= offset;
		return e;
	}

	if ((get_token(*pn)->symbol == '%')
		&& (t= get_token(*pn + 1))->type == T_WORD
		&& isregister(t->name)
	) {
		/* A register operand. */
		(*pn)+= 2;
		e= new_expr();
		e->operator= 'W';
		e->name= copystr(t->name);
		return e;
	}

	/* Offset? */
	if (get_token(*pn)->symbol != '('
				|| get_token(*pn + 1)->symbol != '%') {
		/* There is an offset. */
		if ((offset= gnu_get_C_expression(pn)) == nil) return nil;
	} else {
		/* No offset. */
		offset= nil;
	}

	/* (%base,%index,scale) ? */
	base= index= nil;
	if (get_token(*pn)->symbol == '(') {
		(*pn)++;

		/* %base ? */
		if (get_token(*pn)->symbol == '%'
			&& (t= get_token(*pn + 1))->type == T_WORD
			&& isregister(t->name)
		) {
			/* A base register expression. */
			base= new_expr();
			base->operator= 'B';
			base->name= copystr(t->name);
			(*pn)+= 2;
		}

		if (get_token(*pn)->symbol == ',') (*pn)++;

		/* %index ? */
		if (get_token(*pn)->symbol == '%'
			&& (t= get_token(*pn + 1))->type == T_WORD
			&& isregister(t->name)
		) {
			/* A index register expression. */
			index= new_expr();
			index->operator= '1';		/* for now */
			index->name= copystr(t->name);
			(*pn)+= 2;
		}

		if (get_token(*pn)->symbol == ',') (*pn)++;

		/* scale ? */
		if ((base != nil || index != nil)
			&& (t= get_token(*pn))->type == T_WORD
			&& strchr("1248", t->name[0]) != nil
			&& t->name[1] == 0
		) {		
			if (index == nil) {
				/* Base is really an index register. */
				index= base;
				base= nil;
			}
			index->operator= t->name[0];
			(*pn)++;
		}

		if (get_token(*pn)->symbol == ')') {
			/* Ending paren. */
			(*pn)++;
		} else {
			/* Alas. */
			parse_err(1, t, "operand syntax error\n");
			del_expr(offset);
			del_expr(base);
			del_expr(index);
			return nil;
		}
	}

	if (base == nil && index == nil) {
		if (deref) {
			/* Return a lone offset as (offset). */
			e= new_expr();
			e->operator= '(';
			e->middle= offset;
		} else {
			/* Return a lone offset as is. */
			e= offset;
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

static expression_t *gnu_get_oplist(int *pn, int deref)
/* Get a comma (or colon for jmpf and callf) separated list of instruction
 * operands.
 */
{
	expression_t *e, *o1, *o2;
	token_t *t;

	if ((e= gnu_get_operand(pn, deref)) == nil) return nil;

	if ((t= get_token(*pn))->symbol == ',' || t->symbol == ':') {
		o1= e;
		(*pn)++;
		if ((o2= gnu_get_oplist(pn, deref)) == nil) {
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


static asm86_t *gnu_get_statement(void)
/* Get a pseudo op or machine instruction with arguments. */
{
	token_t *t= get_token(0);
	asm86_t *a;
	mnemonic_t *m;
	int n;
	int prefix_seen;
	int deref;

	assert(t->type == T_WORD);

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
	a->oaz= 0;
	if (a->optype == OWORD) {
		a->oaz|= OPZ;
		a->optype= WORD;
	}

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
			&& (a->args= gnu_get_oplist(&n, deref)) == nil) {
		del_asm86(a);
		return nil;
	}
	if (get_token(n)->symbol != ';') {
		parse_err(1, t, "garbage at end of instruction\n");
		del_asm86(a);
		return nil;
	}
	if (!is_pseudo(a->opcode)) {
		/* GNU operand order is the other way around. */
		expression_t *e, *t;

		e= a->args;
		while (e != nil && e->operator == ',') {
			t= e->right; e->right= e->left; e->left= t;
			e= e->left;
		}
	}
	switch (a->opcode) {
	case DOT_ALIGN:
		/* Delete two argument .align, because ACK can't do it.
		 * Raise 2 to the power of .align's argument.
		 */
		if (a->args == nil || a->args->operator != 'W') {	
			del_asm86(a);
			return nil;
		}
		if (a->args != nil && a->args->operator == 'W'
			&& isanumber(a->args->name)
		) {	
			unsigned n;
			char num[sizeof(int) * CHAR_BIT / 3 + 1];

			n= 1 << strtoul(a->args->name, nil, 0);
			sprintf(num, "%u", n);
			deallocate(a->args->name);
			a->args->name= copystr(num);
		}
		break;
	case JMPF:
	case CALLF:
		/*FALL THROUGH*/
	case JMP:
	case CALL:
		break;
	default:;
	}
	skip_token(n+1);
	return a;
}


asm86_t *gnu_get_instruction(void)
{
	asm86_t *a= nil;
	expression_t *e;
	token_t *t;

	while ((t= get_token(0))->symbol == ';' || t->symbol == '/') {
		zap();		/* if a comment started by a '/' */
		skip_token(1);
	}

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
		a= gnu_get_instruction();
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

		if ((e= gnu_get_C_expression(&n)) == nil) {
			zap();
			a= gnu_get_instruction();
		} else
		if (get_token(n)->symbol != ';') {
			parse_err(1, t, "garbage after assignment\n");
			zap();
			a= gnu_get_instruction();
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
		if ((a= gnu_get_statement()) == nil) {
			zap();
			a= gnu_get_instruction();
		}
	} else {
		parse_err(1, t, "syntax error\n");
		zap();
		a= gnu_get_instruction();
	}
	return a;
}
