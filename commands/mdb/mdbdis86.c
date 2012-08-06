/* 
 * mdbdis86.c for mdb.c - 8086-386 and 8087 disassembler
 * From Bruce Evans db
 */

#include "mdb.h"
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include "proto.h"

struct address_s
{
    off_t off;
    off_t base;
};

static int bits32;
static struct address_s uptr;

static u8_t get8(void);
static u16_t get16(void);
static u32_t get32(void);
static u8_t peek_byte(off_t addr);
static u16_t peek_word(off_t addr);
static int puti(void);
static int outsegaddr(struct address_s *addr);
static int outssegaddr(struct address_s *addr);
static int show1instruction(void);

/************************* UNASM ******************************/


#define LINDIRECT	'['
#define RINDIRECT	']'

#define BASE_MASK	0x07
#define INDEX_MASK	0x38
#define INDEX_SHIFT	3
#define MOD_MASK	0xC0	/* mod reg r/m  is  mmrrrRRR */
#define REG_MOD		0xC0
#define MEM0_MOD	0x00
#define MEM1_MOD	0x40
#define MEM2_MOD	0x80
#define REG_MASK	0x38
#define REG_SHIFT	3
#define RM_MASK		0x07
#define RM_SHIFT	0
#define SS_MASK		0xC0
#define SS_SHIFT	6

#define SIGNBIT		0x02	/* opcode bits xxxxxxsw for immediates */
#define WORDBIT		0x01
#define TOREGBIT	0x02	/* opcode bit for non-immediates */

#define MAX_SIGNED_CHAR	0x7F	/* will assume 2's complement */
#define MAX_UNSIGNED_CHAR	0xFF

typedef unsigned opcode_pt;	/* promote to unsigned and not int */

typedef int reg_pt;
typedef int su16_t;
typedef int su8_pt;

static su8_pt get8s(void);
static void getmodregrm(void);
static void i_00_to_3f(opcode_pt opc );
static void i_40_to_5f(opcode_pt opc );
static void i_60_to_6f(opcode_pt opc );
static void i_70_to_7f(opcode_pt opc );
static void i_80(opcode_pt opc );
static void i_88(opcode_pt opc );
static void i_90(opcode_pt opc );
static void i_98(opcode_pt opc );
static void i_a0(opcode_pt opc );
static void i_a8(opcode_pt opc );
static void i_b0(opcode_pt opc );
static void i_b8(opcode_pt opc );
static void i_c0(opcode_pt opc );
static void i_c8(opcode_pt opc );
static void i_d0(opcode_pt opc );
static void i_d8(opcode_pt opc );
static void i_e0(opcode_pt opc );
static void i_e8(opcode_pt opc );
static void i_f0(opcode_pt opc );
static void i_f8(opcode_pt opc );
static void outad(opcode_pt opc );
static void outad1(opcode_pt opc );
static void outalorx(opcode_pt opc );
static void outax(void);
static void outbptr(void);
static void outbwptr(opcode_pt opc );
static void outea(opcode_pt wordflags );
static void outf1(void);
static void out32offset(void);
static void outfishy(void);
static void outgetaddr(void);
static void outimmed(opcode_pt signwordflag );
static void outpc(off_t pc );
static void outsegpc(void);
static void oututstr(char *s );
static void outword(void);
static void outwptr(void);
static void outwsize(void);
static void pagef(void);
static void shift(opcode_pt opc );
static void checkmemory(void);
static void CL(void);
static void Eb(void);
static void Ev(void);
static void EvGv(void);
static void EvIb(void);
static void Ew(void);
static void EwRw(void);
static void Gv(void);
static void Gv1(void);
static void GvEv(void);
static void GvEw(void);
static void GvM(void);
static void GvMa(void);
static void GvMp(void);
static void Ib(void);
static void Iw(void);
static void Iv(void);
static void Jb(void);
static void Jv(void);
static void Ms(void);

typedef void(*pfv_t) (opcode_pt opc );

static pfv_t optable[] =
{
 i_00_to_3f,
 i_00_to_3f,
 i_00_to_3f,
 i_00_to_3f,
 i_00_to_3f,
 i_00_to_3f,
 i_00_to_3f,
 i_00_to_3f,
 i_40_to_5f,
 i_40_to_5f,
 i_40_to_5f,
 i_40_to_5f,
 i_60_to_6f,
 i_60_to_6f,
 i_70_to_7f,
 i_70_to_7f,
 i_80,
 i_88,
 i_90,
 i_98,
 i_a0,
 i_a8,
 i_b0,
 i_b8,
 i_c0,
 i_c8,
 i_d0,
 i_d8,
 i_e0,
 i_e8,
 i_f0,
 i_f8,
};

static char fishy[] = "???";
static char movtab[] = "mov\t";

static char *genreg[] =
{
 "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
 "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
 "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
};

static char *segreg[] =
{
 "es", "cs", "ss", "ds", "fs", "gs", "?s", "?s",
};

static char *indreg[] =
{
 "bx+si", "bx+di", "bp+si", "bp+di", "si", "di", "bp", "bx",
};

static char *str_00_to_3f[] =
{
 /* index by (opcode >> 3) & 7 */
 "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp",
};

static char *sstr_00_to_3f[] =
{
 /* index ((opc>>2) & 0x0E) + (opc & 7) - 6 */
 "push\tes", "pop\tes", "push\tcs", "pop\tcs",
 "push\tss", "pop\tss", "push\tds", "pop\tds",
 "es:", "daa", "cs:", "das", "ss:", "aaa", "ds:", "aas",
};

static char *sstr_0f[] =
{
 "push\tfs", "pop\tfs", fishy, "bt\t", "shld\t", "shld\t", fishy, fishy,
 "push\tgs", "pop\tgs", fishy, "bts\t", "shrd\t", "shrd\t", fishy, "imul\t",
 fishy, fishy, "lss\t", "btr\t", "lfs\t", "lgs\t", "movzx\t", "movzx\t",
 fishy, fishy, "", "btc\t", "bsf\t", "bsr\t", "movsx\t", "movsx\t",
};

static char *ssstr_0f[] =
{
 "sldt\t", "str\t", "lldt\t", "ltr\t", "verr\t", "verw\t", fishy, fishy,
 "sgdt\t", "sidt\t", "lgdt\t", "lidt\t", "smsw\t", fishy, "lmsw\t", fishy,
 fishy, fishy, fishy, fishy, "bt\t", "bts\t", "btr\t", "btc\t",
};

static char *str_40_to_5f[] =
{
 /* index by (opcode >> 3) & 3 */
 "inc\t", "dec\t", "push\t", "pop\t",
};

static char *str_60_to_6f[] =
{
 "pusha", "popa", "bound\t", "arpl\t", "fs:", "gs:", "os:", "as:",
 "push\t", "imul\t", "push\t", "imul\t", "insb", "ins", "outsb", "outs",
};

static char *str_flags[] =
{
 /* opcodes 0x70 to 0x7F, and 0x0F80 to 0x0F9F */
 "o", "no", "b", "nb", "z", "nz", "be", "a",
 "s", "ns", "pe", "po", "l", "ge", "le", "g",
};

static char *str_98[] =
{
 "cbw", "cwd", "call\t", "wait", "pushf", "popf", "sahf", "lahf",
 "cwde", "cdq", "call\t", "wait", "pushfd", "popfd", "sahf", "lahf",
};

static char *str_a0[] =
{
 movtab, movtab, movtab, movtab, "movsb", "movs", "cmpsb", "cmps",
};

static char *str_a8[] =
{
 "test\t", "test\t", "stosb", "stos", "lodsb", "lods", "scasb", "scas",
};

static char *str_c0[] =
{
 "", "", "ret\t", "ret", "les\t", "lds\t", movtab, movtab,
};

static char *str_c8[] =
{
 "enter\t", "leave", "retf\t", "retf", "int\t3", "int\t", "into", "iret",
};

static char *str_d0[] =
{
 "aam", "aad", "db\td6", "xlat",
};

static char *sstr_d0[] =
{
 "rol", "ror", "rcl", "rcr", "shl", "shr", fishy, "sar",
};

static char *str_d8[] =
{
 "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr",
 "fld", NULL, "fst", "fstp", "fldenv", "fldcw", "fstenv", "fstcw",
 "fiadd", "fimul", "ficom", "ficomp", "fisub", "fisubr", "fidiv", "fidivr",
 "fild", NULL, "fist", "fistp", NULL, "fld", NULL, "fstp",
 "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr",
 "fld", NULL, "fst", "fstp", "frstor", NULL, "fsave", "fstsw",
 "fiadd", "fimul", "ficom", "ficomp", "fisub", "fisubr", "fidiv", "fidivr",
 "fild", NULL, "fist", "fistp", "fbld", "fild", "fbstp", "fistp",
};

static char *str1_d8[] =
{
 "fadd", "fmul", "fcom", "fcomp", "fsub", "fsubr", "fdiv", "fdivr",
 "fld", "fxch", "\0\0", NULL, "\0\10", "\0\20", "\0\30", "\0\40",
 NULL, NULL, NULL, NULL, NULL, "\0\50", NULL, NULL,
 NULL, NULL, NULL, NULL, "\0\60", NULL, NULL, NULL,
 "fadd", "fmul", NULL, NULL, "fsubr", "fsub", "fdivr", "fdiv",
 "ffree", NULL, "fst", "fstp", "fucom", "fucomp", NULL, NULL,
 "faddp", "fmulp", NULL, "\0\70", "fsubrp", "fsubp", "fdivrp", "fdivp",
 NULL, NULL, NULL, NULL, "\0\100", NULL, NULL, NULL,
};

static unsigned char size_d8[] =
{
 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 14-28, 2, 14-28, 2,
 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 0, 10, 0, 10,
 8, 8, 8, 8, 8, 8, 8, 8, 8, 0, 8, 8, 94-108, 0, 94-108, 2,
 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 10, 8, 10, 8,
};

static char *sstr_d8[] =
{
 "fnop", NULL, NULL, NULL,			/* D9D0 */
 NULL, NULL, NULL, NULL,
 "fchs", "fabs", NULL, NULL,			/* D9E0 */
 "ftst", "fxam", NULL, NULL,
 "fld1", "fldl2t", "fldl2e", "fldpi",		/* D9E8 */
 "fldlg2", "fldln2", "fldz", NULL,
 "f2xm1", "fyl2x", "fptan", "fpatan",		/* D9F0 */
 "fxtract", "fprem1", "fdecstp", "fincstp",
 "fprem", "fyl2xp1", "fsqrt", "fsincos",	/* D9F8 */
 "frndint", "fscale", "fsin", "fcos",
 NULL, "fucompp", NULL, NULL,			/* DAE8 */
 NULL, NULL, NULL, NULL,
 "feni", "fdisi", "fclex", "finit",		/* DBE0 */
 "fsetpm", NULL, NULL, NULL,
 NULL, "fcompp", NULL, NULL,			/* DED8 */
 NULL, NULL, NULL, NULL,
 NULL, NULL, NULL, NULL,			/* DFE0 */
 "fstsw\tax", NULL, NULL, NULL,
};

static char *str_e0[] =
{
 "loopnz\t", "loopz\t", "loop\t", "jcxz\t",
 "in\t", "in\t", "out\t", "out\t",
};

static char *str_e8[] =
{
 "call\t", "jmp\t", "jmp\t", "jmp\t",
 "in\t", "in\t", "out\t", "out\t",
};

static char *str_f0[] =
{
 "lock\t", "db\tf1", "repnz\t", "repz\t",
 "hlt", "cmc",
 /* other 2 from sstr_f0 */
};

static char *sstr_f0[] =
{
 "test\t", fishy, "not\t", "neg\t",
 "mul\t", "imul\t", "div\t", "idiv\t",
};

static char *str_f8[] =
{
 "clc", "stc", "cli", "sti",
 "cld", "std",
 /* other 2 from sstr_f8 */
};

static char *sstr_f8[] =
{
 "inc\t", "dec\t", "call\t", "call\tfar ",
 "jmp\t", "jmp\tfar ", "push\t", "???\t",
};

static int data_seg;		/* data segment (munged name for asld) */
static unsigned hasize;	/* half address size in bits */
static unsigned hdefsize;
static unsigned hosize;	/* half operand size in bits */
				/* for easy index into reg tables */
static opcode_pt mod;
static off_t offtable[2];
static off_t *offptr;
static off_t *off1ptr;
static opcode_pt reg;
static opcode_pt rm;

static su8_pt get8s()
{
    su8_pt got;

    if ((got = get8()) > MAX_SIGNED_CHAR)
	got -= (MAX_UNSIGNED_CHAR + 1);
    return got;
}

static void getmodregrm()
{
    opcode_pt modregrm;
    
    modregrm = get8();
    mod = modregrm & MOD_MASK;
    reg = (modregrm & REG_MASK) >> REG_SHIFT;
    rm = (modregrm & RM_MASK) >> RM_SHIFT;
}

static void i_00_to_3f(opc)
opcode_pt opc;
{
    opcode_pt sub;
    
    if (opc == 15)
	pagef();
    else if ((sub = opc & 7) >= 6)
    {
	outustr((sstr_00_to_3f - 6)[((opc >> 2) & 0x0E) + sub]);
	if (!(opc & 1))
	    data_seg = opc;
    }
    else
    {
	oututstr(str_00_to_3f[(opc >> 3) & 7]);
	if (sub == 4)
	{
	    outustr(genreg[0]);
	    outcomma();
	    Ib();
	}
	else if (sub == 5)
	{
	    outax();
	    outcomma();
	    Iv();
	}
	else
	    outad(sub);
    }
}

static void i_40_to_5f(opc)
opcode_pt opc;
{
    outustr(str_40_to_5f[(opc >> 3) & 3]);
    outustr(genreg[hosize + (opc & 7)]);
}

static void i_60_to_6f(opc)
opcode_pt opc;
{
/* most for 386, some for 286 */

    outustr((str_60_to_6f - 0x60)[opc]);
    switch (opc)
    {
    case 0x60:
    case 0x61:
	if (hosize == 16)
	    outwsize();
	break;
    case 0x62:
	GvMa();
	break;
    case 0x63:
	EwRw();
	break;
    case 0x64:
    case 0x65:
	data_seg = opc;
	break;
    case 0x66:
	hosize = (16 + 8) - hdefsize;
	break;
    case 0x67:
	hasize = (16 + 8) - hdefsize;
	break;
    case 0x68:
	outword();
	Iv();
	break;
    case 0x6A:
	outword();
	outimmed(SIGNBIT | WORDBIT);
	break;
    case 0x69:
	GvEv();
	outcomma();
	Iv();
	break;
    case 0x6B:
	GvEv();
	outcomma();
	outimmed(SIGNBIT | WORDBIT);
	break;
    case 0x6D:
    case 0x6F:
	outwsize();
	break;
    }
}

static void i_70_to_7f(opc)
opcode_pt opc;
{
    outustr("j");
    oututstr((str_flags - 0x70)[opc]);
    Jb();
}

static void i_80(opc)
opcode_pt opc;
{
    if (opc >= 4)
    {
	outustr(opc >= 6 ? "xchg\t" : "test\t");
	outad(opc);
    }
    else
    {
	getmodregrm();
	oututstr(str_00_to_3f[reg]);
	outbwptr(opc);
	outea(opc);
	outcomma();
	outimmed(opc);
#ifdef SIGNED_LOGICALS
	if (opc & SIGNBIT && (reg == 1 || reg == 4 || reg == 6))
	    /* and, or and xor with signe extension are not documented in some
	     * 8086 and 80286 manuals, but make sense and work
	     */
	    outfishy();
#endif
    }
}

static void i_88(opc)
opcode_pt opc;
{
    if (opc < 4)
    {
	outustr(movtab);
	outad(opc);
    }
    else if (opc == 5)
    {
	oututstr("lea");
	GvM();
    }
    else if (opc == 7)
    {
	oututstr("pop");
	getmodregrm();
	outwptr();
	Ev();
	if (reg != 0)
	    outfishy();
    }
    else
    {
	getmodregrm();
	outustr(movtab);
	if (!(opc & TOREGBIT))
	{
	    Ev();
	    outcomma();
	}
	outustr(segreg[reg]);
	if (opc & TOREGBIT)
	{
	    outcomma();
	    Ev();
	}
    }
}

static void i_90(opc)
opcode_pt opc;
{
    if (opc == 0)
	outustr("nop");
    else
    {
	outustr("xchg\t");
	outax();
	outcomma();
	outustr(genreg[hosize + opc]);
    }
}

static void i_98(opc)
opcode_pt opc;
{
    outustr((str_98 - 8)[opc + hosize]);
    if (opc == 2)
	outsegpc();
}

static void i_a0(opc)
opcode_pt opc;
{
    outustr(str_a0[opc]);
    if (opc < 4)
    {
	mod = MEM0_MOD;		/* fake */
	reg = 0;		/* fake ax */
	if (hasize == 16)
	    rm = 5;		/* fake [d16] */
	else
	    rm = 6;		/* fake [d32] */
	outad1(opc ^ TOREGBIT);
    }
    else if (opc & 1)
	outwsize();
}

static void i_a8(opc)
opcode_pt opc;
{
    outustr(str_a8[opc]);
    if (opc < 2)
    {
	outalorx(opc);
	outcomma();
	outimmed(opc);
    }
    else if (opc & 1)
	outwsize();
}

static void i_b0(opc)
opcode_pt opc;
{
    outustr(movtab);
    outustr(genreg[opc]);
    outcomma();
    Ib();
}

static void i_b8(opc)
opcode_pt opc;
{
    outustr(movtab);
    outustr(genreg[hosize + opc]);
    outcomma();
    Iv();
}

static void i_c0(opc)
opcode_pt opc;
{
    outustr(str_c0[opc]);
    if (opc >= 6)
    {
	getmodregrm();
	outbwptr(opc);
	outea(opc);
	outcomma();
	outimmed(opc & WORDBIT);
	if (reg != 0)
	    /* not completely decoded (like DEBUG) */
	    outfishy();
    }
    else if (opc >= 4)
	GvMp();
    else if (opc == 2)
	Iv();
    else if (opc < 2)
	shift(opc);
}

static void i_c8(opc)
opcode_pt opc;
{
    outustr(str_c8[opc]);
    if (opc == 0)
    {
	Iw();
	outcomma();
	Ib();
    }
    if (opc == 2)
	Iv();
    else if (opc == 5)
	Ib();
    else if (opc == 7 && hosize == 16)
	outwsize();
}

static void i_d0(opc)
opcode_pt opc;
{
    opcode_pt aabyte;

    if (opc < 4)
	shift(opc | 0xD0);
    else
    {
	outustr((str_d0 - 4)[opc]);
	if (opc < 6 && (aabyte = get8()) != 0x0A)
	{
	    outtab();
	    outh8(aabyte);
	    outfishy();
	}
    }
}

static void i_d8(opc)
opcode_pt opc;
{
    opcode_pt esc;
    char *str;

    getmodregrm();
    esc = (opc << 3) | reg;
    if ((str = (mod == REG_MOD ? str1_d8 : str_d8)[esc]) == NULL)
    {
escape:
	oututstr("esc");
	outh8(esc);
	outcomma();
	outea(0);
	return;
    }
    if (*str == 0)
    {
	str = sstr_d8[str[1] + rm];
	if (str == NULL)
	    goto escape;
	outustr(str);
	return;
    }
    outustr(str);
    outtab(); 
    if (mod == REG_MOD)
    {
	if (opc == 0 && reg != 2 && reg != 3)
	    outustr("st,");
	outf1();
	if (opc == 4 || opc == 6)
	    outustr(",st");
	return; 
    }
    switch(size_d8[esc])
    {
    case 4:
	outustr("d");
    case 2:
	outwptr();
	break;
    case 8:
	outustr("q");
	outwptr();
	break;
    case 10:
	outustr("t");
	outbptr();
	break;
    }
    outea(opc);
}

static void i_e0(opc)
opcode_pt opc;
{
    outustr(str_e0[opc]);
    if (opc < 4)
	Jb();
    else if (opc < 6)
    {
	outalorx(opc);
	outcomma();
	Ib();
    }
    else
    {
	Ib();
	outcomma();
	outalorx(opc);
    }
}

static void i_e8(opc)
opcode_pt opc;
{
    outustr(str_e8[opc]);
    if (opc < 2)
	Jv();
    else if (opc == 2)
	outsegpc();
    else if (opc == 3)
	Jb();
    else
    {
	if (opc & TOREGBIT)
	{
	    outustr(genreg[10]);
	    outcomma();
	    outalorx(opc);
	}
	else
	{
	    outalorx(opc);
	    outcomma();
	    outustr(genreg[10]);
	}
    }
}

static void i_f0(opc)
opcode_pt opc;
{
    if (opc < 6)
	outustr(str_f0[opc]);
    else
    {
	getmodregrm();
	outustr(sstr_f0[reg]);
	outbwptr(opc);
	outea(opc);
	if (reg == 0)
	{
	    outcomma();
	    outimmed(opc & WORDBIT);
	}
    }
}

static void i_f8(opc)
opcode_pt opc;
{
    if (opc < 6)
	outustr(str_f8[opc]);
    else
    {
	getmodregrm();
	if (opc == 6 && reg >= 2)
	    outustr("fishy\t");
	else
	    outustr(sstr_f8[reg]);
	outbwptr(opc);
	outea(opc);
    }
}

static void outad(opc)
opcode_pt opc;
{
    getmodregrm();
    outad1(opc);
}

static void outad1(opc)
opcode_pt opc;
{
    if (!(opc & TOREGBIT))
    {
	outea(opc);
	outcomma();
    }
    if (opc & WORDBIT)
	Gv1();
    else
	outustr(genreg[reg]);
    if (opc & TOREGBIT)
    {
	outcomma();
	outea(opc);
    }
}

static void outalorx(opc)
opcode_pt opc;
{
    if (opc & WORDBIT)
	outax();
    else
	outustr(genreg[0]);
}

static void outax()
{
    outustr(genreg[hosize]);
}

static void outbptr()
{
    outustr("byte ptr ");
}

static void outbwptr(opc)
opcode_pt opc;
{
    if (mod != REG_MOD)
    {
	if (opc & WORDBIT)
	    outwptr();
	else
	    outbptr();
    }
}

static void outea(wordflags)
opcode_pt wordflags;
{
    reg_pt base;
    reg_pt index;
    opcode_pt ss;
    opcode_pt ssindexbase;

    if (mod == REG_MOD)
	outustr(genreg[hosize * (wordflags & WORDBIT) + rm]);
    else
    {
	outbyte(LINDIRECT);
	if (hasize == 16)
	{
	    if (rm == 4)
	    {
		base = (ssindexbase = get8()) & BASE_MASK;
		if (mod == MEM0_MOD && base == 5)
		    outgetaddr();
		else
		    outustr((genreg + 16)[base]);
		ss = (ssindexbase & SS_MASK) >> SS_SHIFT;
		if ((index = (ssindexbase & INDEX_MASK) >> INDEX_SHIFT) != 4)
		{
		    outbyte('+');
		    outustr((genreg + 16)[index]);
		    outstr("\0\0\0*2\0*4\0*8\0" + (3 * ss));
		}
	    }
	    else if (mod == MEM0_MOD && rm == 5)
		outgetaddr();
	    else
		outustr((genreg + 16)[rm]);
	}
	else if (mod == MEM0_MOD && rm == 6)
	    outgetaddr();
	else
	    outustr(indreg[rm]);
	if (mod == MEM1_MOD)
	    /* fake sign extension to get +- */
	    outimmed(SIGNBIT | WORDBIT);
	else if (mod == MEM2_MOD)
	{
	    outbyte('+');
	    out32offset();
	}
	outbyte(RINDIRECT);
	if (hasize == 16 && rm == 4 && index == 4 && ss != 0)
	    outfishy();
    }
}

static void outf1()
{
    outustr("st(");
    outbyte((int) (rm + '0'));
    outbyte(')');
}

static void out32offset()
{
    off_t off;

    if (hasize == 16)
	off = get32();
    else
	outfishy();

    outh32(off);
}

static void outfishy()
{
    outustr("\t???");
}

static void outgetaddr()
{
    off_t off;

    if (hasize == 16)
	off = get32();
    else
	off = get16();

    if ( finds_data(off,data_seg) )
  	*offptr++ = off;
    else if (hasize == 16)
	outh32(off);
    else
	outh16((u16_t) off);
}

static void outimmed(signwordflag)
opcode_pt signwordflag;
{
    su8_pt byte;

    if (signwordflag & WORDBIT)
    {
	if (signwordflag & SIGNBIT)
	{
	    if ((byte = get8s()) < 0)
	    {
		outbyte('-');
		byte = -byte;
	    }
	    else
		outbyte('+');
	    outh8((u8_t) byte);
	}
	else
	    Iv();
    }
    else
	Ib();
}

static void outpc(pc)
off_t pc;
{
    if (hosize == 8)
	pc = (u16_t) pc;

    if ( finds_pc(pc) )
	*offptr++ = pc;
    else if (hosize == 16)
	outh32(pc);
    else
	outh16((u16_t) pc);
}

static void outsegpc()
{
    off_t oldbase;
    off_t pc;

    if (hosize == 16)
	pc = get32();
    else
	pc = get16();
    oldbase = uptr.base;
    outh16((u16_t) (uptr.base = get16()));	/* fake seg for lookup of pc */
			/* TODO - convert to offset in protected mode */
    outbyte(':');
    outpc(pc);
    uptr.base = oldbase;
}

static void oututstr(s)
char *s;
{
    outustr(s);
    outtab();
}

static void outword()
{
    outustr("dword " + ((16 - hosize) >> 3));
}

static void outwptr()
{
    outword();
    outustr("ptr ");
}

static void outwsize()
{
    if (hosize == 16)
	outustr("d");
    else
	outustr("w");
}

static void pagef()
{
    opcode_pt opc;
    int regbad;

    if ((opc = get8()) <= 1 || opc == 0xBA)
    {
	if (opc == 0xBA)
	    opc = 16;
	else
	    opc *= 8;
	getmodregrm();
	outustr(ssstr_0f[opc += reg]);
	if (opc < 6 || opc == 12 || opc == 14)
	    Ew();
	else if (opc >= 8 && opc < 13)
	    Ms();
	else if (opc >= 20)
	{
	    outbwptr(WORDBIT);
	    EvIb();
	}
    }
    else if (opc < 4)
    {
	oututstr("lar\0lsl" + 4 * (opc - 2));
	GvEw();
    }
    else if (opc == 5)
    {
	outustr("loadall");
	outfishy();
    }
    else if (opc == 6)
	outustr("clts");
    else if (opc < 0x20)
	outstr(fishy);
    else if (opc < 0x27 && opc != 0x25)
    {
	outustr(movtab);
	getmodregrm();
	hosize = 16;
	if (!(opc & TOREGBIT))
	{
	    Ev();		/* Rd() since hosize is 16 */
	    outcomma();
	}
	regbad = FALSE;
	if (opc & 1)
	{
	    outustr("dr");
	    if (reg == 4 || reg == 5)
		regbad = TRUE;
	}
	else if (opc < 0x24)
	{
	    outustr("cr");
	    if (reg >= 4 || reg == 1)
		regbad = TRUE;
	}
	else
	{
	    outustr("tr");
	    if (reg < 6)
		regbad = TRUE;
	}
	outbyte((int) (reg + '0'));
	if (opc & TOREGBIT)
	{
	    outcomma();
	    Ev();
	}
	if (regbad || mod != REG_MOD)
	    outfishy();
    }
    else if (opc < 0x80)
	outstr(fishy);
    else if (opc < 0x90)
    {
	outustr("j");
	oututstr((str_flags - 0x80)[opc]);
	Jv();
    }
    else if (opc < 0xA0)
    {
	outustr("set");
	oututstr((str_flags - 0x90)[opc]);
	getmodregrm();
	outbwptr(0);
	Eb();
    }
    else if (opc < 0xC0)
    {
	outustr((sstr_0f - 0xA0)[opc]);
	switch (opc)
	{
	case 0xA3:
	case 0xAB:
	case 0xB3:
	case 0xBB:
	    EvGv();
	    break;
	case 0xA4:
	case 0xAC:
	    EvGv();
	    outcomma();
	    Ib();
	    break;
	case 0xA5:
	case 0xAD:
	    EvGv();
	    outcomma();
	    CL();
	    break;
	case 0xAF:
	case 0xBC:
	case 0xBD:
	    GvEv();
	    break;
	case 0xB2:
	case 0xB4:
	case 0xB5:
	    GvMp();
	    break;
	case 0xB6:
	case 0xBE:
	    Gv();
	    outcomma();
	    outbwptr(opc);
	    Eb();
	    break;
	case 0xB7:
	case 0xBF:
	    Gv();
	    outcomma();
	    hosize = 8;		/* done in Ew(), but too late */
	    outbwptr(opc);
	    Ew();
	    break;
	}
    }
    else
	outstr(fishy);
}

static int puti()
{
    static int hadprefix;
    opcode_pt opcode;

more:
    offptr = offtable;
    opcode = get8();
    if (!hadprefix)
    {
	data_seg = DSEG;
	hdefsize = 8;
	if (bits32)
	    hdefsize = 16;
	hosize =
	    hasize = hdefsize;
    }
    (*optable[opcode >> 3])(opcode < 0x80 ? opcode : opcode & 7);
    if (offptr > offtable)
    {
	if (stringtab() >= 31)
	{
	    outspace();
	    outspace();
	}
	else
	    while (stringtab() < 32)
		outtab();
	outbyte(';');
	for (off1ptr = offtable; off1ptr < offptr; ++off1ptr)
	{
	    outspace();
	    if (*off1ptr < 0x10000)
		outh16((u16_t) *off1ptr);
	    else
		outh32(*off1ptr);
	}
	offptr = offtable;
    }
    if ((opcode & 0xE7) == 0x26 ||
	opcode >= 0x64 && opcode < 0x68 ||
	opcode == 0xF0 || opcode == 0xF2 || opcode == 0xF3)
	/* not finished instruction for 0x26, 0x2E, 0x36, 0x3E seg overrides
	 * and 0x64, 0x65 386 seg overrides
	 * and 0x66, 0x67 386 size prefixes
	 * and 0xF0 lock, 0xF2 repne, 0xF3 rep
	 */
    {
	hadprefix = TRUE;
	goto more;		/* TODO - print prefixes better */
	return FALSE;
    }
    hadprefix = FALSE;
    return TRUE;
}

static void shift(opc)
opcode_pt opc;
{
    getmodregrm();
    oututstr(sstr_d0[reg]);
    outbwptr(opc);
    outea(opc);
    outcomma();
    if (opc < 0xD0)
	Ib();
    else if (opc & 2)
	CL();
    else
	outbyte('1');
}

static void checkmemory()
{
    if (mod == REG_MOD)
	outfishy();
}

static void CL()
{
    outustr(genreg[1]);
}

static void Eb()
{
    outea(0);
}

static void Ev()
{
    outea(WORDBIT);
}

static void EvGv()
{
    getmodregrm();
    Ev();
    outcomma();
    Gv1();
}

static void EvIb()
{
    Ev();
    outcomma();
    Ib();
}

static void Ew()
{
    hosize = 8;
    Ev();
}

static void EwRw()
{
    hosize = 8;
    EvGv();
}

static void Gv()
{
    getmodregrm();
    Gv1();
}

static void Gv1()
{
    outustr(genreg[hosize + reg]);
}

static void GvEv()
{
    Gv();
    outcomma();
    Ev();
}

static void GvEw()
{
    Gv();
    outcomma();
    Ew();
}

static void GvM()
{
    GvEv();
    checkmemory();
}

static void GvMa()
{
    GvM();
}

static void GvMp()
{
    GvM();
}

static void Ib()
{
    outh8(get8());
}

static void Iw()
{
    outh16(get16());
}

static void Iv()
{
    if (hosize == 16)
	outh32(get32());
    else
	Iw();
}

static void Jb()
{
    off_t pcjump;

    pcjump = get8s();
    outpc(pcjump + uptr.off);
}

static void Jv()
{
    off_t pcjump;

    if (hosize == 16)
	pcjump = get32();
    else
	pcjump = (su16_t) get16();
    outpc(pcjump + uptr.off);
}

static void Ms()
{
    Ev();
    checkmemory();
}

/********************* DASM ******************************/

long dasm( addr, count, symflg )
long addr;
int count;
int symflg;
{
	bits32 = TRUE;		/* Set mode */
	uptr.off = addr;
	uptr.base = 0;		/* not known */
	while ( count-- != 0 && show1instruction() )
		;
}


static int show1instruction()
{
    register int column;
    int idone;
    static char line[81];
    int maxcol;
    struct address_s newuptr;
    struct address_s olduptr;

    outbyte('\r');
    do
    {
	if ( text_symbol(uptr.off) ) {
	    outbyte(':');
	    outbyte('\n');
	}
	olduptr = uptr;
	openstring(line);
	idone = puti();
	line[stringpos()] = 0;
	closestring();
	newuptr = uptr;
	uptr = olduptr;
	column = outssegaddr(&uptr);
	while (uptr.off != newuptr.off)
	{
	    outh8(get8());
	    column += 2;
	}
	maxcol = bits32 ? 24 : 16;
	while (column < maxcol)
	{
	    outtab();
	    column += 8;
	}
	outtab();
	outstr(line);
	outbyte('\n');
    }
    while (!idone);		/* eat all prefixes */
    return TRUE;
}


static u8_t get8()
{
/* get 8 bits current instruction pointer and advance pointer */

    u8_t temp;

    temp = peek_byte(uptr.off + uptr.base);
    ++uptr.off;
    return temp;
}

static u16_t get16()
{
/* get 16 bits from current instruction pointer and advance pointer */

    u16_t temp;

    temp = peek_word(uptr.off + uptr.base);
    uptr.off += 2;
    return temp;
}

static u32_t get32()
{
/* get 32 bits from current instruction pointer and advance pointer */

    u32_t temp;

    temp = peek_dword(uptr.off + uptr.base);
    uptr.off += 4;
    return temp;
}


static int outsegaddr(addr)
struct address_s *addr;
{
/* print segmented address */

    int bytes_printed;

    bytes_printed = 2;
	bytes_printed = outsegreg(addr->base);
    if (bytes_printed > 4)
	outbyte('+');
    else
	outbyte(':');
    ++bytes_printed;
    if (addr->off >= 0x10000)
    {
	outh32(addr->off);
	return bytes_printed + 8;
    }
    outh16((u16_t) addr->off);
    return bytes_printed + 4;
}

static int outssegaddr(addr)
struct address_s *addr;
{
/* print 32 bit segmented address and 2 spaces */

    int bytes_printed;

    bytes_printed = outsegaddr(addr);
    outspace();
    outspace();
    return bytes_printed + 2;
}

static u8_t peek_byte(addr)
off_t addr;
{
    return (u8_t) peek_dword(addr) & 0xFF; /* 8 bits only */
}

static u16_t peek_word(addr)
off_t addr;
{
    return (u16_t) peek_dword(addr);
}
