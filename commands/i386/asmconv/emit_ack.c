/*	emit_ack.c - emit ACK assembly			Author: Kees J. Bot
 *		     emit NCC assembly				27 Dec 1993
 */
#define nil 0
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "asmconv.h"
#include "token.h"
#include "asm86.h"
#include "languages.h"

typedef struct mnemonic {	/* ACK as86 mnemonics translation table. */
	opcode_t	opcode;
	char		*name;
} mnemonic_t;

static mnemonic_t mnemtab[] = {
	{ AAA,		"aaa"		},
	{ AAD,		"aad"		},
	{ AAM,		"aam"		},
	{ AAS,		"aas"		},
	{ ADC,		"adc%"		},
	{ ADD,		"add%"		},
	{ AND,		"and%"		},
	{ ARPL,		"arpl"		},
	{ BOUND,	"bound"		},
	{ BSF,		"bsf"		},
	{ BSR,		"bsr"		},
	{ BSWAP,	"bswap"		},
	{ BT,		"bt"		},
	{ BTC,		"btc"		},
	{ BTR,		"btr"		},
	{ BTS,		"bts"		},
	{ CALL,		"call"		},
	{ CALLF,	"callf"		},
	{ CBW,		"cbw"		},
	{ CLC,		"clc"		},
	{ CLD,		"cld"		},
	{ CLI,		"cli"		},
	{ CLTS,		"clts"		},
	{ CMC,		"cmc"		},
	{ CMP,		"cmp%"		},
	{ CMPS,		"cmps%"		},
	{ CMPXCHG,	"cmpxchg"	},
	{ CWD,		"cwd"		},
	{ DAA,		"daa"		},
	{ DAS,		"das"		},
	{ DEC,		"dec%"		},
	{ DIV,		"div%"		},
	{ DOT_ALIGN,	".align"	},
	{ DOT_ASCII,	".ascii"	},
	{ DOT_ASCIZ,	".asciz"	},
	{ DOT_ASSERT,	".assert"	},
	{ DOT_BASE,	".base"		},
	{ DOT_BSS,	".sect .bss"	},
	{ DOT_COMM,	".comm"		},
	{ DOT_DATA,	".sect .data"	},
	{ DOT_DATA1,	".data1"	},
	{ DOT_DATA2,	".data2"	},
	{ DOT_DATA4,	".data4"	},
	{ DOT_DEFINE,	".define"	},
	{ DOT_END,	".sect .end"	},
	{ DOT_EXTERN,	".extern"	},
	{ DOT_FILE,	".file"		},
	{ DOT_LCOMM,	".comm"		},
	{ DOT_LINE,	".line"		},
	{ DOT_LIST,	".list"		},
	{ DOT_NOLIST,	".nolist"	},
	{ DOT_ROM,	".sect .rom"	},
	{ DOT_SPACE,	".space"	},
	{ DOT_SYMB,	".symb"		},
	{ DOT_TEXT,	".sect .text"	},
	{ DOT_USE16,	".use16"	},
	{ DOT_USE32,	".use32"	},
	{ ENTER,	"enter"		},
	{ F2XM1,	"f2xm1"		},
	{ FABS,		"fabs"		},
	{ FADD,		"fadd"		},
	{ FADDD,	"faddd"		},
	{ FADDP,	"faddp"		},
	{ FADDS,	"fadds"		},
	{ FBLD,		"fbld"		},
	{ FBSTP,	"fbstp"		},
	{ FCHS,		"fchs"		},
	{ FCLEX,	"fclex"		},
	{ FCOMD,	"fcomd"		},
	{ FCOMPD,	"fcompd"	},
	{ FCOMPP,	"fcompp"	},
	{ FCOMPS,	"fcomps"	},
	{ FCOMS,	"fcoms"		},
	{ FCOS,		"fcos"		},
	{ FDECSTP,	"fdecstp"	},
	{ FDIVD,	"fdivd"		},
	{ FDIVP,	"fdivp"		},
	{ FDIVRD,	"fdivrd"	},
	{ FDIVRP,	"fdivrp"	},
	{ FDIVRS,	"fdivrs"	},
	{ FDIVS,	"fdivs"		},
	{ FFREE,	"ffree"		},
	{ FIADDL,	"fiaddl"	},
	{ FIADDS,	"fiadds"	},
	{ FICOM,	"ficom"		},
	{ FICOMP,	"ficomp"	},
	{ FIDIVL,	"fidivl"	},
	{ FIDIVRL,	"fidivrl"	},
	{ FIDIVRS,	"fidivrs"	},
	{ FIDIVS,	"fidivs"	},
	{ FILDL,	"fildl"		},
	{ FILDQ,	"fildq"		},
	{ FILDS,	"filds"		},
	{ FIMULL,	"fimull"	},
	{ FIMULS,	"fimuls"	},
	{ FINCSTP,	"fincstp"	},
	{ FINIT,	"finit"		},
	{ FISTL,	"fistl"		},
	{ FISTP,	"fistp"		},
	{ FISTS,	"fists"		},
	{ FISUBL,	"fisubl"	},
	{ FISUBRL,	"fisubrl"	},
	{ FISUBRS,	"fisubrs"	},
	{ FISUBS,	"fisubs"	},
	{ FLD1,		"fld1"		},
	{ FLDCW,	"fldcw"		},
	{ FLDD,		"fldd"		},
	{ FLDENV,	"fldenv"	},
	{ FLDL2E,	"fldl2e"	},
	{ FLDL2T,	"fldl2t"	},
	{ FLDLG2,	"fldlg2"	},
	{ FLDLN2,	"fldln2"	},
	{ FLDPI,	"fldpi"		},
	{ FLDS,		"flds"		},
	{ FLDX,		"fldx"		},
	{ FLDZ,		"fldz"		},
	{ FMULD,	"fmuld"		},
	{ FMULP,	"fmulp"		},
	{ FMULS,	"fmuls"		},
	{ FNOP,		"fnop"		},
	{ FPATAN,	"fpatan"	},
	{ FPREM,	"fprem"		},
	{ FPREM1,	"fprem1"	},
	{ FPTAN,	"fptan"		},
	{ FRNDINT,	"frndint"	},
	{ FRSTOR,	"frstor"	},
	{ FSAVE,	"fsave"		},
	{ FSCALE,	"fscale"	},
	{ FSIN,		"fsin"		},
	{ FSINCOS,	"fsincos"	},
	{ FSQRT,	"fsqrt"		},
	{ FSTCW,	"fstcw"		},
	{ FSTD,		"fstd"		},
	{ FSTENV,	"fstenv"	},
	{ FSTPD,	"fstpd"		},
	{ FSTPS,	"fstps"		},
	{ FSTPX,	"fstpx"		},
	{ FSTS,		"fsts"		},
	{ FSTSW,	"fstsw"		},
	{ FSUBD,	"fsubd"		},
	{ FSUBP,	"fsubp"		},
	{ FSUBPR,	"fsubpr"	},
	{ FSUBRD,	"fsubrd"	},
	{ FSUBRS,	"fsubrs"	},
	{ FSUBS,	"fsubs"		},
	{ FTST,		"ftst"		},
	{ FUCOM,	"fucom"		},
	{ FUCOMP,	"fucomp"	},
	{ FUCOMPP,	"fucompp"	},
	{ FXAM,		"fxam"		},
	{ FXCH,		"fxch"		},
	{ FXTRACT,	"fxtract"	},
	{ FYL2X,	"fyl2x"		},
	{ FYL2XP1,	"fyl2xp1"	},
	{ HLT,		"hlt"		},
	{ IDIV,		"idiv%"		},
	{ IMUL,		"imul%"		},
	{ IN,		"in%"		},
	{ INC,		"inc%"		},
	{ INS,		"ins%"		},
	{ INT,		"int"		},
	{ INTO,		"into"		},
	{ INVD,		"invd"		},
	{ INVLPG,	"invlpg"	},
	{ IRET,		"iret"		},
	{ IRETD,	"iretd"		},
	{ JA,		"ja"		},
	{ JAE,		"jae"		},
	{ JB,		"jb"		},
	{ JBE,		"jbe"		},
	{ JCXZ,		"jcxz"		},
	{ JE,		"je"		},
	{ JG,		"jg"		},
	{ JGE,		"jge"		},
	{ JL,		"jl"		},
	{ JLE,		"jle"		},
	{ JMP,		"jmp"		},
	{ JMPF,		"jmpf"		},
	{ JNE,		"jne"		},
	{ JNO,		"jno"		},
	{ JNP,		"jnp"		},
	{ JNS,		"jns"		},
	{ JO,		"jo"		},
	{ JP,		"jp"		},
	{ JS,		"js"		},
	{ LAHF,		"lahf"		},
	{ LAR,		"lar"		},
	{ LDS,		"lds"		},
	{ LEA,		"lea"		},
	{ LEAVE,	"leave"		},
	{ LES,		"les"		},
	{ LFS,		"lfs"		},
	{ LGDT,		"lgdt"		},
	{ LGS,		"lgs"		},
	{ LIDT,		"lidt"		},
	{ LLDT,		"lldt"		},
	{ LMSW,		"lmsw"		},
	{ LOCK,		"lock"		},
	{ LODS,		"lods%"		},
	{ LOOP,		"loop"		},
	{ LOOPE,	"loope"		},
	{ LOOPNE,	"loopne"	},
	{ LSL,		"lsl"		},
	{ LSS,		"lss"		},
	{ LTR,		"ltr"		},
	{ MOV,		"mov%"		},
	{ MOVS,		"movs%"		},
	{ MOVSX,	"movsx"		},
	{ MOVSXB,	"movsxb"	},
	{ MOVZX,	"movzx"		},
	{ MOVZXB,	"movzxb"	},
	{ MUL,		"mul%"		},
	{ NEG,		"neg%"		},
	{ NOP,		"nop"		},
	{ NOT,		"not%"		},
	{ OR,		"or%"		},
	{ OUT,		"out%"		},
	{ OUTS,		"outs%"		},
	{ POP,		"pop"		},
	{ POPA,		"popa"		},
	{ POPF,		"popf"		},
	{ PUSH,		"push"		},
	{ PUSHA,	"pusha"		},
	{ PUSHF,	"pushf"		},
	{ RCL,		"rcl%"		},
	{ RCR,		"rcr%"		},
	{ RET,		"ret"		},
	{ RETF,		"retf"		},
	{ ROL,		"rol%"		},
	{ ROR,		"ror%"		},
	{ SAHF,		"sahf"		},
	{ SAL,		"sal%"		},
	{ SAR,		"sar%"		},
	{ SBB,		"sbb%"		},
	{ SCAS,		"scas%"		},
	{ SETA,		"seta"		},
	{ SETAE,	"setae"		},
	{ SETB,		"setb"		},
	{ SETBE,	"setbe"		},
	{ SETE,		"sete"		},
	{ SETG,		"setg"		},
	{ SETGE,	"setge"		},
	{ SETL,		"setl"		},
	{ SETLE,	"setle"		},
	{ SETNE,	"setne"		},
	{ SETNO,	"setno"		},
	{ SETNP,	"setnp"		},
	{ SETNS,	"setns"		},
	{ SETO,		"seto"		},
	{ SETP,		"setp"		},
	{ SETS,		"sets"		},
	{ SGDT,		"sgdt"		},
	{ SHL,		"shl%"		},
	{ SHLD,		"shld"		},
	{ SHR,		"shr%"		},
	{ SHRD,		"shrd"		},
	{ SIDT,		"sidt"		},
	{ SLDT,		"sldt"		},
	{ SMSW,		"smsw"		},
	{ STC,		"stc"		},
	{ STD,		"std"		},
	{ STI,		"sti"		},
	{ STOS,		"stos%"		},
	{ STR,		"str"		},
	{ SUB,		"sub%"		},
	{ TEST,		"test%"		},
	{ VERR,		"verr"		},
	{ VERW,		"verw"		},
	{ WAIT,		"wait"		},
	{ WBINVD,	"wbinvd"	},
	{ XADD,		"xadd"		},
	{ XCHG,		"xchg%"		},
	{ XLAT,		"xlat"		},
	{ XOR,		"xor%"		},
};

#define farjmp(o)	((o) == JMPF || (o) == CALLF)

static FILE *ef;
static long eline= 1;
static char *efile;
static char *orig_efile;
static char *opcode2name_tab[N_OPCODES];
static enum dialect { ACK, NCC } dialect= ACK;

static void ack_putchar(int c)
/* LOOK, this programmer checks the return code of putc!  What an idiot, noone
 * does that!
 */
{
	if (putc(c, ef) == EOF) fatal(orig_efile);
}

static void ack_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vfprintf(ef, fmt, ap) == EOF) fatal(orig_efile);
	va_end(ap);
}

void ack_emit_init(char *file, const char *banner)
/* Prepare producing an ACK assembly file. */
{
	mnemonic_t *mp;

	if (file == nil) {
		file= "stdout";
		ef= stdout;
	} else {
		if ((ef= fopen(file, "w")) == nil) fatal(file);
	}
	orig_efile= file;
	efile= file;
	ack_printf("! %s", banner);
	if (dialect == ACK) {
		/* Declare the four sections used under Minix. */
		ack_printf(
	"\n.sect .text; .sect .rom; .sect .data; .sect .bss\n.sect .text");
	}

	/* Initialize the opcode to mnemonic translation table. */
	for (mp= mnemtab; mp < arraylimit(mnemtab); mp++) {
		assert(opcode2name_tab[mp->opcode] == nil);
		opcode2name_tab[mp->opcode]= mp->name;
	}
}

#define opcode2name(op)		(opcode2name_tab[op] + 0)

static void ack_put_string(const char *s, size_t n)
/* Emit a string with weird characters quoted. */
{
	while (n > 0) {
		int c= *s;

		if (c < ' ' || c > 0177) {
			ack_printf("\\%03o", c & 0xFF);
		} else
		if (c == '"' || c == '\\') {
			ack_printf("\\%c", c);
		} else {
			ack_putchar(c);
		}
		s++;
		n--;
	}
}

static void ack_put_expression(asm86_t *a, expression_t *e, int deref)
/* Send an expression, i.e. instruction operands, to the output file.  Deref
 * is true when the rewrite for the ncc dialect may be made.
 */
{
	assert(e != nil);

	switch (e->operator) {
	case ',':
		if (dialect == NCC && farjmp(a->opcode)) {
			/* ACK jmpf seg:off  ->  NCC jmpf off,seg */
			ack_put_expression(a, e->right, deref);
			ack_printf(", ");
			ack_put_expression(a, e->left, deref);
		} else {
			ack_put_expression(a, e->left, deref);
			ack_printf(farjmp(a->opcode) ? ":" : ", ");
			ack_put_expression(a, e->right, deref);
		}
		break;
	case 'O':
		if (deref && a->optype == JUMP) ack_putchar('@');
		if (e->left != nil) ack_put_expression(a, e->left, 0);
		if (e->middle != nil) ack_put_expression(a, e->middle, 0);
		if (e->right != nil) ack_put_expression(a, e->right, 0);
		break;
	case '(':
		if (deref && a->optype == JUMP) ack_putchar('@');
		if (!deref) ack_putchar('(');
		ack_put_expression(a, e->middle, 0);
		if (!deref) ack_putchar(')');
		break;
	case 'B':
		ack_printf("(%s)", e->name);
		break;
	case '1':
	case '2':
	case '4':
	case '8':
		ack_printf((use16() && e->operator == '1')
				? "(%s)" : "(%s*%c)", e->name, e->operator);
		break;
	case '+':
	case '-':
	case '~':
		if (e->middle != nil) {
			if (deref && a->optype != JUMP) ack_putchar('#');
			ack_putchar(e->operator);
			ack_put_expression(a, e->middle, 0);
			break;
		}
		/*FALL THROUGH*/
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case S_LEFTSHIFT:
	case S_RIGHTSHIFT:
		if (deref && a->optype != JUMP) ack_putchar('#');
		ack_put_expression(a, e->left, 0);
		if (e->operator == S_LEFTSHIFT) {
			ack_printf("<<");
		} else
		if (e->operator == S_RIGHTSHIFT) {
			ack_printf(">>");
		} else {
			ack_putchar(e->operator);
		}
		ack_put_expression(a, e->right, 0);
		break;
	case '[':
		if (deref && a->optype != JUMP) ack_putchar('#');
		ack_putchar('[');
		ack_put_expression(a, e->middle, 0);
		ack_putchar(']');
		break;
	case 'W':
		if (deref && a->optype == JUMP && isregister(e->name))
		{
			ack_printf("(%s)", e->name);
			break;
		}
		if (deref && a->optype != JUMP && !isregister(e->name)) {
			ack_putchar('#');
		}
		ack_printf("%s", e->name);
		break;
	case 'S':
		ack_putchar('"');
		ack_put_string(e->name, e->len);
		ack_putchar('"');
		break;
	default:
		fprintf(stderr,
		"asmconv: internal error, unknown expression operator '%d'\n",
			e->operator);
		exit(EXIT_FAILURE);
	}
}

void ack_emit_instruction(asm86_t *a)
/* Output one instruction and its operands. */
{
	int same= 0;
	char *p;
	static int high_seg;
	int deref;

	if (a == nil) {
		/* Last call */
		ack_putchar('\n');
		return;
	}

	/* Make sure the line number of the line to be emitted is ok. */
	if ((a->file != efile && strcmp(a->file, efile) != 0)
				|| a->line < eline || a->line > eline+10) {
		ack_putchar('\n');
		ack_printf("# %ld \"%s\"\n", a->line, a->file);
		efile= a->file;
		eline= a->line;
	} else {
		if (a->line == eline) {
			ack_printf("; ");
			same= 1;
		}
		while (eline < a->line) {
			ack_putchar('\n');
			eline++;
		}
	}

	if (a->opcode == DOT_LABEL) {
		assert(a->args->operator == ':');
		ack_printf("%s:", a->args->name);
	} else
	if (a->opcode == DOT_EQU) {
		assert(a->args->operator == '=');
		ack_printf("\t%s = ", a->args->name);
		ack_put_expression(a, a->args->middle, 0);
	} else
	if ((p= opcode2name(a->opcode)) != nil) {
		char *sep= dialect == ACK ? "" : ";";

		if (!is_pseudo(a->opcode) && !same) ack_putchar('\t');

		switch (a->rep) {
		case ONCE:	break;
		case REP:	ack_printf("rep");	break;
		case REPE:	ack_printf("repe");	break;
		case REPNE:	ack_printf("repne");	break;
		default:	assert(0);
		}
		if (a->rep != ONCE) {
			ack_printf(dialect == ACK ? " " : "; ");
		}
		switch (a->seg) {
		case DEFSEG:	break;
		case CSEG:	ack_printf("cseg");	break;
		case DSEG:	ack_printf("dseg");	break;
		case ESEG:	ack_printf("eseg");	break;
		case FSEG:	ack_printf("fseg");	break;
		case GSEG:	ack_printf("gseg");	break;
		case SSEG:	ack_printf("sseg");	break;
		default:	assert(0);
		}
		if (a->seg != DEFSEG) {
			ack_printf(dialect == ACK ? " " : "; ");
		}
		if (a->oaz & OPZ) ack_printf(use16() ? "o32 " : "o16 ");
		if (a->oaz & ADZ) ack_printf(use16() ? "a32 " : "a16 ");

		if (a->opcode == CBW) {
			p= !(a->oaz & OPZ) == use16() ? "cbw" : "cwde";
		}

		if (a->opcode == CWD) {
			p= !(a->oaz & OPZ) == use16() ? "cwd" : "cdq";
		}

		if (a->opcode == DOT_COMM && a->args != nil
			&& a->args->operator == ','
			&& a->args->left->operator == 'W'
		) {
			ack_printf(".define\t%s; ", a->args->left->name);
		}
		while (*p != 0) {
			if (*p == '%') {
				if (a->optype == BYTE) ack_putchar('b');
			} else {
				ack_putchar(*p);
			}
			p++;
		}
		if (a->args != nil) {
			ack_putchar('\t');
			switch (a->opcode) {
			case IN:
			case OUT:
			case INT:
				deref= 0;
				break;
			default:
				deref= (dialect == NCC && a->optype != PSEUDO);
			}
			ack_put_expression(a, a->args, deref);
		}
		if (a->opcode == DOT_USE16) set_use16();
		if (a->opcode == DOT_USE32) set_use32();
	} else {
		fprintf(stderr,
			"asmconv: internal error, unknown opcode '%d'\n",
			a->opcode);
		exit(EXIT_FAILURE);
	}
}

/* A few ncc mnemonics are different. */
static mnemonic_t ncc_mnemtab[] = {
	{ DOT_BSS,	".bss"		},
	{ DOT_DATA,	".data"		},
	{ DOT_END,	".end"		},
	{ DOT_ROM,	".rom"		},
	{ DOT_TEXT,	".text"		},
};

void ncc_emit_init(char *file, const char *banner)
/* The assembly produced by the Minix ACK ANSI C compiler for the 8086 is
 * different from the normal ACK assembly, and different from the old K&R
 * assembler.  This brings us endless joy.  (It was supposed to make
 * translation of the assembly used by the old K&R assembler easier by
 * not deviating too much from that dialect.)
 */
{
	mnemonic_t *mp;

	dialect= NCC;
	ack_emit_init(file, banner);

	/* Replace a few mnemonics. */
	for (mp= ncc_mnemtab; mp < arraylimit(ncc_mnemtab); mp++) {
		opcode2name_tab[mp->opcode]= mp->name;
	}
}

void ncc_emit_instruction(asm86_t *a)
{
	ack_emit_instruction(a);
}
