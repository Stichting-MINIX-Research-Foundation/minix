/*	asm86.h - 80X86 assembly intermediate		Author: Kees J. Bot
 *								27 Jun 1993
 */

typedef enum opcode {	/* 80486 opcodes, from the i486 reference manual.
			 * Synonyms left out, some new words invented.
			 */
	DOT_ALIGN,
	DOT_ASCII,	DOT_ASCIZ,
	DOT_ASSERT,			/* Pseudo's invented */
	DOT_BASE,
	DOT_COMM,	DOT_LCOMM,
	DOT_DATA1,
	DOT_DATA2,
	DOT_DATA4,
	DOT_DEFINE,	DOT_EXTERN,
	DOT_EQU,
	DOT_FILE,	DOT_LINE,
	DOT_LABEL,
	DOT_LIST,	DOT_NOLIST,
	DOT_SPACE,
	DOT_SYMB,
	DOT_TEXT,	DOT_ROM,	DOT_DATA,	DOT_BSS,	DOT_END,
	DOT_USE16,	DOT_USE32,
	AAA,
	AAD,
	AAM,
	AAS,
	ADC,
	ADD,
	AND,
	ARPL,
	BOUND,
	BSF,
	BSR,
	BSWAP,
	BT,
	BTC,
	BTR,
	BTS,
	CALL,	CALLF,			/* CALLF added */
	CBW,
	CLC,
	CLD,
	CLI,
	CLTS,
	CMC,
	CMP,
	CMPS,
	CMPXCHG,
	CWD,
	DAA,
	DAS,
	DEC,
	DIV,
	ENTER,
	F2XM1,
	FABS,
	FADD,	FADDD,	FADDS,	FADDP,	FIADDL,	FIADDS,
	FBLD,
	FBSTP,
	FCHS,
	FCLEX,
	FCOMD,	FCOMS,	FCOMPD,	FCOMPS,	FCOMPP,
	FCOS,
	FDECSTP,
	FDIVD,	FDIVS,	FDIVP,	FIDIVL,	FIDIVS,
	FDIVRD,	FDIVRS,	FDIVRP,	FIDIVRL,	FIDIVRS,
	FFREE,
	FICOM,	FICOMP,
	FILDQ,	FILDL,	FILDS,
	FINCSTP,
	FINIT,
	FISTL,	FISTS,	FISTP,
	FLDX,	FLDD,	FLDS,
	FLD1,	FLDL2T,	FLDL2E,	FLDPI,	FLDLG2,	FLDLN2,	FLDZ,
	FLDCW,
	FLDENV,
	FMULD,	FMULS,	FMULP,	FIMULL,	FIMULS,
	FNOP,
	FPATAN,
	FPREM,
	FPREM1,
	FPTAN,
	FRNDINT,
	FRSTOR,
	FSAVE,
	FSCALE,
	FSIN,
	FSINCOS,
	FSQRT,
	FSTD,	FSTS,	FSTPX,	FSTPD,	FSTPS,
	FSTCW,
	FSTENV,
	FSTSW,
	FSUBD,	FSUBS,	FSUBP,	FISUBL,	FISUBS,
	FSUBRD,	FSUBRS,	FSUBPR,	FISUBRL, FISUBRS,
	FTST,
	FUCOM,	FUCOMP,	FUCOMPP,
	FXAM,
	FXCH,
	FXTRACT,
	FYL2X,
	FYL2XP1,
	HLT,
	IDIV,
	IMUL,
	IN,
	INC,
	INS,
	INT,	INTO,
	INVD,
	INVLPG,
	IRET,	IRETD,
	JA,	JAE,	JB,	JBE,	JCXZ,	JE,	JG,	JGE,	JL,
	JLE,	JNE,	JNO,	JNP,	JNS,	JO,	JP,	JS,
	JMP,	JMPF,			/* JMPF added */
	LAHF,
	LAR,
	LEA,
	LEAVE,
	LGDT,	LIDT,
	LGS,	LSS,	LDS,	LES,	LFS,
	LLDT,
	LMSW,
	LOCK,
	LODS,
	LOOP,	LOOPE,	LOOPNE,
	LSL,
	LTR,
	MOV,
	MOVS,
	MOVSX,
	MOVSXB,
	MOVZX,
	MOVZXB,
	MUL,
	NEG,
	NOP,
	NOT,
	OR,
	OUT,
	OUTS,
	POP,
	POPA,
	POPF,
	PUSH,
	PUSHA,
	PUSHF,
	RCL,	RCR,	ROL,	ROR,
	RET,	RETF,			/* RETF added */
	SAHF,
	SAL,	SAR,	SHL,	SHR,
	SBB,
	SCAS,
	SETA,	SETAE,	SETB,	SETBE,	SETE,	SETG,	SETGE,	SETL,
	SETLE,	SETNE,	SETNO,	SETNP,	SETNS,	SETO,	SETP,	SETS,
	SGDT,	SIDT,
	SHLD,
	SHRD,
	SLDT,
	SMSW,
	STC,
	STD,
	STI,
	STOS,
	STR,
	SUB,
	TEST,
	VERR,	VERW,
	WAIT,
	WBINVD,
	XADD,
	XCHG,
	XLAT,
	XOR
} opcode_t;

#define is_pseudo(o)	((o) <= DOT_USE32)
#define N_OPCODES	((int) XOR + 1)

#define OPZ	0x01		/* Operand size prefix. */
#define ADZ	0x02		/* Address size prefix. */

typedef enum optype {
	PSEUDO,	JUMP,	BYTE,	WORD,	OWORD		/* Ordered list! */
} optype_t;

typedef enum repeat {
	ONCE,	REP,	REPE,	REPNE
} repeat_t;

typedef enum segment {
	DEFSEG,	CSEG,	DSEG,	ESEG,	FSEG,	GSEG,	SSEG
} segment_t;

typedef struct expression {
	int		operator;
	struct expression *left, *middle, *right;
	char		*name;
	size_t		len;
	unsigned	magic;
} expression_t;

typedef struct asm86 {
	opcode_t	opcode;		/* DOT_TEXT, MOV, ... */
	char		*file;		/* Name of the file it is found in. */
	long		line;		/* Line number. */
	optype_t	optype;		/* Type of operands: byte, word... */
	int		oaz;		/* Operand/address size prefix? */
	repeat_t	rep;		/* Repeat prefix used on this instr. */
	segment_t	seg;		/* Segment override. */
	expression_t	*args;		/* Arguments in ACK order. */
	unsigned	magic;
} asm86_t;

expression_t *new_expr(void);
void del_expr(expression_t *a);
asm86_t *new_asm86(void);
void del_asm86(asm86_t *a);
int isregister(const char *name);

/*
 * Format of the arguments of the asm86_t structure:
 *
 *
 * ACK assembly operands	expression_t cell:
 * or part of operand:		{operator, left, middle, right, name, len}
 *
 * [expr]			{'[', nil, expr, nil}
 * word				{'W', nil, nil, nil, word}
 * "string"			{'S', nil, nil, nil, "string", strlen("string")}
 * label = expr			{'=', nil, expr, nil, label}
 * expr * expr			{'*', expr, nil, expr}
 * - expr			{'-', nil, expr, nil}
 * (memory)			{'(', nil, memory, nil}
 * offset(base)(index*n)	{'O', offset, base, index*n}
 * base				{'B', nil, nil, nil, base}
 * index*4			{'4', nil, nil, nil, index}
 * operand, oplist		{',', operand, nil, oplist}
 * label :			{':', nil, nil, nil, label}
 *
 * The precedence of operators is ignored.  The expression is simply copied
 * as is, including parentheses.  Problems like missing operators in the
 * target language will have to be handled by rewriting the source language.
 * 16-bit or 32-bit registers must be used where they are required by the
 * target assembler even though ACK makes no difference between 'ax' and
 * 'eax'.  Asmconv is smart enough to transform compiler output.  Human made
 * assembly can be fixed up to be transformable.
 */
