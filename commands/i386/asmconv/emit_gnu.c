/*	emit_gnu.c - emit GNU assembly			Author: Kees J. Bot
 *								28 Dec 1993
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

typedef struct mnemonic {	/* GNU as386 mnemonics translation table. */
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
	{ BOUND,	"bound%"	},
	{ BSF,		"bsf%"		},
	{ BSR,		"bsr%"		},
	{ BSWAP,	"bswap"		},
	{ BT,		"bt%"		},
	{ BTC,		"btc%"		},
	{ BTR,		"btr%"		},
	{ BTS,		"bts%"		},
	{ CALL,		"call"		},
	{ CALLF,	"lcall"		},
	{ CBW,		"cbtw"		},
	{ CLC,		"clc"		},
	{ CLD,		"cld"		},
	{ CLI,		"cli"		},
	{ CLTS,		"clts"		},
	{ CMC,		"cmc"		},
	{ CMP,		"cmp%"		},
	{ CMPS,		"cmps%"		},
	{ CMPXCHG,	"cmpxchg"	},
	{ CWD,		"cwtd"		},
	{ DAA,		"daa"		},
	{ DAS,		"das"		},
	{ DEC,		"dec%"		},
	{ DIV,		"div%"		},
	{ DOT_ALIGN,	".align"	},
	{ DOT_ASCII,	".ascii"	},
	{ DOT_ASCIZ,	".asciz"	},
	{ DOT_ASSERT,	".assert"	},
	{ DOT_BASE,	".base"		},
	{ DOT_BSS,	".bss"		},
	{ DOT_COMM,	".comm"		},
	{ DOT_DATA,	".data"		},
	{ DOT_DATA1,	".byte"		},
	{ DOT_DATA2,	".short"	},
	{ DOT_DATA4,	".long"		},
	{ DOT_DEFINE,	".globl"	},
	{ DOT_EXTERN,	".globl"	},
	{ DOT_FILE,	".file"		},
	{ DOT_LCOMM,	".lcomm"	},
	{ DOT_LINE,	".line"		},
	{ DOT_LIST,	".list"		},
	{ DOT_NOLIST,	".nolist"	},
	{ DOT_ROM,	".data"		},	/* Minix -- separate I&D. */
	{ DOT_SPACE,	".space"	},
	{ DOT_SYMB,	".symb"		},
	{ DOT_TEXT,	".text"		},
	{ DOT_USE16,	".use16"	},
	{ DOT_USE32,	".use32"	},
	{ ENTER,	"enter"		},
	{ F2XM1,	"f2xm1"		},
	{ FABS,		"fabs"		},
	{ FADD,		"fadd"		},
	{ FADDD,	"faddl"		},
	{ FADDP,	"faddp"		},
	{ FADDS,	"fadds"		},
	{ FBLD,		"fbld"		},
	{ FBSTP,	"fbstp"		},
	{ FCHS,		"fchs"		},
	{ FCLEX,	"fnclex"	},
	{ FCOMD,	"fcoml"		},
	{ FCOMPD,	"fcompl"	},
	{ FCOMPP,	"fcompp"	},
	{ FCOMPS,	"fcomps"	},
	{ FCOMS,	"fcoms"		},
	{ FCOS,		"fcos"		},
	{ FDECSTP,	"fdecstp"	},
	{ FDIVD,	"fdivl"		},
	{ FDIVP,	"fdivp"		},
	{ FDIVRD,	"fdivrl"	},
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
	{ FINIT,	"fninit"	},
	{ FISTL,	"fistl"		},
	{ FISTP,	"fistp"		},
	{ FISTS,	"fists"		},
	{ FISUBL,	"fisubl"	},
	{ FISUBRL,	"fisubrl"	},
	{ FISUBRS,	"fisubrs"	},
	{ FISUBS,	"fisubs"	},
	{ FLD1,		"fld1"		},
	{ FLDCW,	"fldcw"		},
	{ FLDD,		"fldl"		},
	{ FLDENV,	"fldenv"	},
	{ FLDL2E,	"fldl2e"	},
	{ FLDL2T,	"fldl2t"	},
	{ FLDLG2,	"fldlg2"	},
	{ FLDLN2,	"fldln2"	},
	{ FLDPI,	"fldpi"		},
	{ FLDS,		"flds"		},
	{ FLDX,		"fldt"		},
	{ FLDZ,		"fldz"		},
	{ FMULD,	"fmull"		},
	{ FMULP,	"fmulp"		},
	{ FMULS,	"fmuls"		},
	{ FNOP,		"fnop"		},
	{ FPATAN,	"fpatan"	},
	{ FPREM,	"fprem"		},
	{ FPREM1,	"fprem1"	},
	{ FPTAN,	"fptan"		},
	{ FRNDINT,	"frndint"	},
	{ FRSTOR,	"frstor"	},
	{ FSAVE,	"fnsave"	},
	{ FSCALE,	"fscale"	},
	{ FSIN,		"fsin"		},
	{ FSINCOS,	"fsincos"	},
	{ FSQRT,	"fsqrt"		},
	{ FSTCW,	"fnstcw"	},
	{ FSTD,		"fstl"		},
	{ FSTENV,	"fnstenv"	},
	{ FSTPD,	"fstpl"		},
	{ FSTPS,	"fstps"		},
	{ FSTPX,	"fstpt"		},
	{ FSTS,		"fsts"		},
	{ FSTSW,	"fstsw"		},
	{ FSUBD,	"fsubl"		},
	{ FSUBP,	"fsubp"		},
	{ FSUBPR,	"fsubpr"	},
	{ FSUBRD,	"fsubrl"	},
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
	{ IRETD,	"iret"		},
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
	{ JMPF,		"ljmp"		},
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
	{ LEA,		"lea%"		},
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
	{ MOVSX,	"movswl"	},
	{ MOVSXB,	"movsb%"	},
	{ MOVZX,	"movzwl"	},
	{ MOVZXB,	"movzb%"	},
	{ MUL,		"mul%"		},
	{ NEG,		"neg%"		},
	{ NOP,		"nop"		},
	{ NOT,		"not%"		},
	{ OR,		"or%"		},
	{ OUT,		"out%"		},
	{ OUTS,		"outs%"		},
	{ POP,		"pop%"		},
	{ POPA,		"popa%"		},
	{ POPF,		"popf%"		},
	{ PUSH,		"push%"		},
	{ PUSHA,	"pusha%"	},
	{ PUSHF,	"pushf%"	},
	{ RCL,		"rcl%"		},
	{ RCR,		"rcr%"		},
	{ RET,		"ret"		},
	{ RETF,		"lret"		},
	{ ROL,		"rol%"		},
	{ ROR,		"ror%"		},
	{ SAHF,		"sahf"		},
	{ SAL,		"sal%"		},
	{ SAR,		"sar%"		},
	{ SBB,		"sbb%"		},
	{ SCAS,		"scas%"		},
	{ SETA,		"setab"		},
	{ SETAE,	"setaeb"	},
	{ SETB,		"setbb"		},
	{ SETBE,	"setbeb"	},
	{ SETE,		"seteb"		},
	{ SETG,		"setgb"		},
	{ SETGE,	"setgeb"	},
	{ SETL,		"setlb"		},
	{ SETLE,	"setleb"	},
	{ SETNE,	"setneb"	},
	{ SETNO,	"setnob"	},
	{ SETNP,	"setnpb"	},
	{ SETNS,	"setnsb"	},
	{ SETO,		"setob"		},
	{ SETP,		"setpb"		},
	{ SETS,		"setsb"		},
	{ SGDT,		"sgdt"		},
	{ SHL,		"shl%"		},
	{ SHLD,		"shld%"		},
	{ SHR,		"shr%"		},
	{ SHRD,		"shrd%"		},
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

static FILE *ef;
static long eline= 1;
static char *efile;
static char *orig_efile;
static char *opcode2name_tab[N_OPCODES];

static void gnu_putchar(int c)
/* LOOK, this programmer checks the return code of putc!  What an idiot, noone
 * does that!
 */
{
	if (putc(c, ef) == EOF) fatal(orig_efile);
}

static void gnu_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vfprintf(ef, fmt, ap) == EOF) fatal(orig_efile);
	va_end(ap);
}

void gnu_emit_init(char *file, const char *banner)
/* Prepare producing a GNU assembly file. */
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
	gnu_printf("/ %s", banner);

	/* Initialize the opcode to mnemonic translation table. */
	for (mp= mnemtab; mp < arraylimit(mnemtab); mp++) {
		assert(opcode2name_tab[mp->opcode] == nil);
		opcode2name_tab[mp->opcode]= mp->name;
	}
}

#define opcode2name(op)		(opcode2name_tab[op] + 0)

static void gnu_put_string(const char *s, size_t n)
/* Emit a string with weird characters quoted. */
{
	while (n > 0) {
		int c= *s;

		if (c < ' ' || c > 0177) {
			gnu_printf("\\%03o", c);
		} else
		if (c == '"' || c == '\\') {
			gnu_printf("\\%c", c & 0xFF);
		} else {
			gnu_putchar(c);
		}
		s++;
		n--;
	}
}

static void gnu_put_expression(asm86_t *a, expression_t *e, int deref)
/* Send an expression, i.e. instruction operands, to the output file.  Deref
 * is true when the rewrite of "x" -> "#x" or "(x)" -> "x" may be made.
 */
{
	assert(e != nil);

	switch (e->operator) {
	case ',':
		if (is_pseudo(a->opcode)) {
			/* Pseudo's are normal. */
			gnu_put_expression(a, e->left, deref);
			gnu_printf(", ");
			gnu_put_expression(a, e->right, deref);
		} else {
			/* He who invented GNU assembly has seen one VAX too
			 * many, operands are given in the wrong order.  This
			 * makes coding from an Intel databook a real delight.
			 * A good thing this program allows us to write the
			 * more normal ACK assembly.
			 */
			gnu_put_expression(a, e->right, deref);
			gnu_printf(", ");
			gnu_put_expression(a, e->left, deref);
		}
		break;
	case 'O':
		if (deref && a->optype == JUMP) gnu_putchar('*');
		if (e->left != nil) gnu_put_expression(a, e->left, 0);
		gnu_putchar('(');
		if (e->middle != nil) gnu_put_expression(a, e->middle, 0);
		if (e->right != nil) {
			gnu_putchar(',');
			gnu_put_expression(a, e->right, 0);
		}
		gnu_putchar(')');
		break;
	case '(':
		if (!deref) gnu_putchar('(');
		if (deref && a->optype == JUMP) gnu_putchar('*');
		gnu_put_expression(a, e->middle, 0);
		if (!deref) gnu_putchar(')');
		break;
	case 'B':
		gnu_printf("%%%s", e->name);
		break;
	case '1':
	case '2':
	case '4':
	case '8':
		gnu_printf("%%%s,%c", e->name, e->operator);
		break;
	case '+':
	case '-':
	case '~':
		if (e->middle != nil) {
			if (deref && a->optype >= BYTE) gnu_putchar('$');
			gnu_putchar(e->operator);
			gnu_put_expression(a, e->middle, 0);
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
		if (deref && a->optype >= BYTE) gnu_putchar('$');
		gnu_put_expression(a, e->left, 0);
		if (e->operator == S_LEFTSHIFT) {
			gnu_printf("<<");
		} else
		if (e->operator == S_RIGHTSHIFT) {
			gnu_printf(">>");
		} else {
			gnu_putchar(e->operator);
		}
		gnu_put_expression(a, e->right, 0);
		break;
	case '[':
		if (deref && a->optype >= BYTE) gnu_putchar('$');
		gnu_putchar('(');
		gnu_put_expression(a, e->middle, 0);
		gnu_putchar(')');
		break;
	case 'W':
		if (isregister(e->name)) {
			if (a->optype == JUMP) gnu_putchar('*');
			gnu_printf("%%%s", e->name);
		} else {
			if (deref && a->optype >= BYTE) gnu_putchar('$');
			gnu_printf("%s", e->name);
		}
		break;
	case 'S':
		gnu_putchar('"');
		gnu_put_string(e->name, e->len);
		gnu_putchar('"');
		break;
	default:
		fprintf(stderr,
		"asmconv: internal error, unknown expression operator '%d'\n",
			e->operator);
		exit(EXIT_FAILURE);
	}
}

void gnu_emit_instruction(asm86_t *a)
/* Output one instruction and its operands. */
{
	int same= 0;
	char *p;

	if (a == nil) {
		/* Last call */
		gnu_putchar('\n');
		return;
	}

	if (use16()) {
		fprintf(stderr,
		"asmconv: the GNU assembler can't translate 8086 code\n");
		exit(EXIT_FAILURE);
	}

	/* Make sure the line number of the line to be emitted is ok. */
	if ((a->file != efile && strcmp(a->file, efile) != 0)
				|| a->line < eline || a->line > eline+10) {
		gnu_putchar('\n');
		gnu_printf("# %ld \"%s\"\n", a->line, a->file);
		efile= a->file;
		eline= a->line;
	} else {
		if (a->line == eline) {
			gnu_printf("; ");
			same= 1;
		}
		while (eline < a->line) {
			gnu_putchar('\n');
			eline++;
		}
	}

	if (a->opcode == DOT_LABEL) {
		assert(a->args->operator == ':');
		gnu_printf("%s:", a->args->name);
	} else
	if (a->opcode == DOT_EQU) {
		assert(a->args->operator == '=');
		gnu_printf("\t%s = ", a->args->name);
		gnu_put_expression(a, a->args->middle, 0);
	} else
	if (a->opcode == DOT_ALIGN) {
		/* GNU .align thinks in powers of two. */
		unsigned long n;
		unsigned s;

		assert(a->args->operator == 'W' && isanumber(a->args->name));
		n= strtoul(a->args->name, nil, 0);
		for (s= 0; s <= 4 && (1 << s) < n; s++) {}
		gnu_printf(".align\t%u", s);
	} else
	if ((p= opcode2name(a->opcode)) != nil) {
		if (!is_pseudo(a->opcode) && !same) gnu_putchar('\t');

		switch (a->rep) {
		case ONCE:	break;
		case REP:	gnu_printf("rep; ");	break;
		case REPE:	gnu_printf("repe; ");	break;
		case REPNE:	gnu_printf("repne; ");	break;
		default:	assert(0);
		}
		switch (a->seg) {
		/* Kludge to avoid knowing where to put the "%es:" */
		case DEFSEG:	break;
		case CSEG:	gnu_printf(".byte 0x2e; ");	break;
		case DSEG:	gnu_printf(".byte 0x3e; ");	break;
		case ESEG:	gnu_printf(".byte 0x26; ");	break;
		case FSEG:	gnu_printf(".byte 0x64; ");	break;
		case GSEG:	gnu_printf(".byte 0x65; ");	break;
		case SSEG:	gnu_printf(".byte 0x36; ");	break;
		default:	assert(0);
		}

		/* Exceptions, exceptions... */
		if (a->opcode == CBW) {
			if (!(a->oaz & OPZ)) p= "cwtl";
			a->oaz&= ~OPZ;
		}
		if (a->opcode == CWD) {
			if (!(a->oaz & OPZ)) p= "cltd";
			a->oaz&= ~OPZ;
		}

		if (a->opcode == RET || a->opcode == RETF) {
			/* Argument of RET needs a '$'. */
			a->optype= WORD;
		}

		if (a->opcode == MUL && a->args != nil
						&& a->args->operator == ',') {
			/* Two operand MUL is an IMUL? */
			p="imul%";
		}

		/* GAS doesn't understand the interesting combinations. */
		if (a->oaz & ADZ) gnu_printf(".byte 0x67; ");
		if (a->oaz & OPZ && strchr(p, '%') == nil)
			gnu_printf(".byte 0x66; ");

		/* Unsupported instructions that Minix code needs. */
		if (a->opcode == JMPF && a->args != nil
					&& a->args->operator == ',') {
			/* JMPF seg:off. */
			gnu_printf(".byte 0xEA; .long ");
			gnu_put_expression(a, a->args->right, 0);
			gnu_printf("; .short ");
			gnu_put_expression(a, a->args->left, 0);
			return;
		}
		if (a->opcode == JMPF && a->args != nil
			&& a->args->operator == 'O'
			&& a->args->left != nil
			&& a->args->right == nil
			&& a->args->middle != nil
			&& a->args->middle->operator == 'B'
			&& strcmp(a->args->middle->name, "esp") == 0
		) {
			/* JMPF offset(ESP). */
			gnu_printf(".byte 0xFF,0x6C,0x24,");
			gnu_put_expression(a, a->args->left, 0);
			return;
		}
		if (a->opcode == MOV && a->args != nil
			&& a->args->operator == ','
			&& a->args->left != nil
			&& a->args->left->operator == 'W'
			&& (strcmp(a->args->left->name, "ds") == 0
				|| strcmp(a->args->left->name, "es") == 0)
			&& a->args->right->operator == 'O'
			&& a->args->right->left != nil
			&& a->args->right->right == nil
			&& a->args->right->middle != nil
			&& a->args->right->middle->operator == 'B'
			&& strcmp(a->args->right->middle->name, "esp") == 0
		) {
			/* MOV DS, offset(ESP); MOV ES, offset(ESP) */
			gnu_printf(".byte 0x8E,0x%02X,0x24,",
				a->args->left->name[0] == 'd' ? 0x5C : 0x44);
			gnu_put_expression(a, a->args->right->left, 0);
			return;
		}
		if (a->opcode == MOV && a->args != nil
			&& a->args->operator == ','
			&& a->args->left != nil
			&& a->args->left->operator == 'W'
			&& (strcmp(a->args->left->name, "ds") == 0
				|| strcmp(a->args->left->name, "es") == 0)
			&& a->args->right->operator == '('
			&& a->args->right->middle != nil
		) {
			/* MOV DS, (memory); MOV ES, (memory) */
			gnu_printf(".byte 0x8E,0x%02X; .long ",
				a->args->left->name[0] == 'd' ? 0x1D : 0x05);
			gnu_put_expression(a, a->args->right->middle, 0);
			return;
		}

		while (*p != 0) {
			if (*p == '%') {
				if (a->optype == BYTE) {
					gnu_putchar('b');
				} else
				if (a->optype == WORD) {
					gnu_putchar((a->oaz & OPZ) ? 'w' : 'l');
				} else {
					assert(0);
				}
			} else {
				gnu_putchar(*p);
			}
			p++;
		}

		if (a->args != nil) {
			static char *aregs[] = { "al", "ax", "eax" };

			gnu_putchar('\t');
			switch (a->opcode) {
			case IN:
				gnu_put_expression(a, a->args, 1);
				gnu_printf(", %%%s", aregs[a->optype - BYTE]);
				break;
			case OUT:
				gnu_printf("%%%s, ", aregs[a->optype - BYTE]);
				gnu_put_expression(a, a->args, 1);
				break;
			default:
				gnu_put_expression(a, a->args, 1);
			}
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
