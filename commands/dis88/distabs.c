static char *sccsid =
   "@(#) distabs.c, Ver. 2.1 created 00:00:00 87/09/01";

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  *  Copyright (C) 1987 G. M. Harding, all rights reserved  *
  *                                                         *
  * Permission to copy and  redistribute is hereby granted, *
  * provided full source code,  with all copyright notices, *
  * accompanies any redistribution.                         *
  *                                                         *
  * This file  contains  the  lookup  tables and other data *
  * structures for the Intel 8088 symbolic disassembler. It *
  * also contains a few global  routines  which  facilitate *
  * access to the tables,  for use primarily by the handler *
  * functions.                                              *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "dis.h"              /* Disassembler declarations  */

struct exec HDR;              /* Used to hold header info   */

struct nlist symtab[MAXSYM];  /* Array of symbol table info */

struct reloc relo[MAXSYM];    /* Array of relocation info   */

int symptr = -1,              /* Index into symtab[]        */
    relptr = -1;              /* Index into relo[]          */

char *REGS[] =                /* Table of register names    */
   {
   "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
   "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
   "es", "cs", "ss", "ds"
   };

char *REGS0[] =               /* Mode 0 register name table */
   {
   "bx_si", "bx_di", "bp_si", "bp_di", "si", "di", "", "bx"
   };

char *REGS1[] =               /* Mode 1 register name table */
   {
   "bx_si", "bx_di", "bp_si", "bp_di", "si", "di", "bp", "bx"
   };

int symrank[6][6] =           /* Symbol type/rank matrix    */
   {
              /* UND  ABS  TXT  DAT  BSS  COM */
   /* UND */      5,   4,   1,   2,   3,   0,
   /* ABS */      1,   5,   4,   3,   2,   0,
   /* TXT */      4,   1,   5,   3,   2,   0,
   /* DAT */      3,   1,   2,   5,   4,   0,
   /* BSS */      3,   1,   2,   4,   5,   0,
   /* COM */      2,   0,   1,   3,   4,   5
   };

 /* * * * * * * * * * * * OPCODE DATA * * * * * * * * * * * */

char ADD[]   = "\tadd",             /* Mnemonics by family  */
     OR[]    = "\tor",
     ADC[]   = "\tadc",
     SBB[]   = "\tsbb",
     AND[]   = "\tand",
     SUB[]   = "\tsub",
     XOR[]   = "\txor",
     CMP[]   = "\tcmp",
     NOT[]   = "\tnot",
     NEG[]   = "\tneg",
     MUL[]   = "\tmul",
     DIV[]   = "\tdiv",
     MOV[]   = "\tmov",
     ESC[]   = "\tesc",
     TEST[]  = "\ttest",
     AMBIG[] = "",
     ROL[]   = "\trol",
     ROR[]   = "\tror",
     RCL[]   = "\trcl",
     RCR[]   = "\trcr",
     SAL[]   = "\tsal",
     SHR[]   = "\tshr",
     SHL[]   = "\tshl",
     SAR[]   = "\tsar";

char *OPFAM[] =                     /* Family lookup table  */
   {
   ADD, OR, ADC, SBB, AND, SUB, XOR, CMP,
   NOT, NEG, MUL, DIV, MOV, ESC, TEST, AMBIG,
   ROL, ROR, RCL, RCR, SAL, SHR, SHL, SAR
   };

struct opcode optab[] =             /* Table of opcode data */
   {
   ADD,              aohand,  2,    4,             /* 0x00  */
   ADD,              aohand,  2,    4,             /* 0x01  */
   ADD,              aohand,  2,    4,             /* 0x02  */
   ADD,              aohand,  2,    4,             /* 0x03  */
   ADD,              aohand,  2,    2,             /* 0x04  */
   ADD,              aohand,  3,    3,             /* 0x05  */
   "\tpush\tes",     sbhand,  1,    1,             /* 0x06  */
   "\tpop\tes",      sbhand,  1,    1,             /* 0x07  */
   OR,               aohand,  2,    4,             /* 0x08  */
   OR,               aohand,  2,    4,             /* 0x09  */
   OR,               aohand,  2,    4,             /* 0x0a  */
   OR,               aohand,  2,    4,             /* 0x0b  */
   OR,               aohand,  2,    2,             /* 0x0c  */
   OR,               aohand,  3,    3,             /* 0x0d  */
   "\tpush\tcs",     sbhand,  1,    1,             /* 0x0e  */
   NULL,             dfhand,  0,    0,             /* 0x0f  */
   ADC,              aohand,  2,    4,             /* 0x10  */
   ADC,              aohand,  2,    4,             /* 0x11  */
   ADC,              aohand,  2,    4,             /* 0x12  */
   ADC,              aohand,  2,    4,             /* 0x13  */
   ADC,              aohand,  2,    2,             /* 0x14  */
   ADC,              aohand,  3,    3,             /* 0x15  */
   "\tpush\tss",     sbhand,  1,    1,             /* 0x16  */
   "\tpop\tss",      sbhand,  1,    1,             /* 0x17  */
   SBB,              aohand,  2,    4,             /* 0x18  */
   SBB,              aohand,  2,    4,             /* 0x19  */
   SBB,              aohand,  2,    4,             /* 0x1a  */
   SBB,              aohand,  2,    4,             /* 0x1b  */
   SBB,              aohand,  2,    2,             /* 0x1c  */
   SBB,              aohand,  3,    3,             /* 0x1d  */
   "\tpush\tds",     sbhand,  1,    1,             /* 0x1e  */
   "\tpop\tds",      sbhand,  1,    1,             /* 0x1f  */
   AND,              aohand,  2,    4,             /* 0x20  */
   AND,              aohand,  2,    4,             /* 0x21  */
   AND,              aohand,  2,    4,             /* 0x22  */
   AND,              aohand,  2,    4,             /* 0x23  */
   AND,              aohand,  2,    2,             /* 0x24  */
   AND,              aohand,  3,    3,             /* 0x25  */
   "\tseg\tes",      sbhand,  1,    1,             /* 0x26  */
   "\tdaa",          sbhand,  1,    1,             /* 0x27  */
   SUB,              aohand,  2,    4,             /* 0x28  */
   SUB,              aohand,  2,    4,             /* 0x29  */
   SUB,              aohand,  2,    4,             /* 0x2a  */
   SUB,              aohand,  2,    4,             /* 0x2b  */
   SUB,              aohand,  2,    2,             /* 0x2c  */
   SUB,              aohand,  3,    3,             /* 0x2d  */
   "\tseg\tcs",      sbhand,  1,    1,             /* 0x2e  */
   "\tdas",          sbhand,  1,    1,             /* 0x2f  */
   XOR,              aohand,  2,    4,             /* 0x30  */
   XOR,              aohand,  2,    4,             /* 0x31  */
   XOR,              aohand,  2,    4,             /* 0x32  */
   XOR,              aohand,  2,    4,             /* 0x33  */
   XOR,              aohand,  2,    2,             /* 0x34  */
   XOR,              aohand,  3,    3,             /* 0x35  */
   "\tseg\tss",      sbhand,  1,    1,             /* 0x36  */
   "\taaa",          sbhand,  1,    1,             /* 0x37  */
   CMP,              aohand,  2,    4,             /* 0x38  */
   CMP,              aohand,  2,    4,             /* 0x39  */
   CMP,              aohand,  2,    4,             /* 0x3a  */
   CMP,              aohand,  2,    4,             /* 0x3b  */
   CMP,              aohand,  2,    2,             /* 0x3c  */
   CMP,              aohand,  3,    3,             /* 0x3d  */
   "\tseg\tds",      sbhand,  1,    1,             /* 0x3e  */
   "\taas",          sbhand,  1,    1,             /* 0x3f  */
   "\tinc\tax",      sbhand,  1,    1,             /* 0x40  */
   "\tinc\tcx",      sbhand,  1,    1,             /* 0x41  */
   "\tinc\tdx",      sbhand,  1,    1,             /* 0x42  */
   "\tinc\tbx",      sbhand,  1,    1,             /* 0x43  */
   "\tinc\tsp",      sbhand,  1,    1,             /* 0x44  */
   "\tinc\tbp",      sbhand,  1,    1,             /* 0x45  */
   "\tinc\tsi",      sbhand,  1,    1,             /* 0x46  */
   "\tinc\tdi",      sbhand,  1,    1,             /* 0x47  */
   "\tdec\tax",      sbhand,  1,    1,             /* 0x48  */
   "\tdec\tcx",      sbhand,  1,    1,             /* 0x49  */
   "\tdec\tdx",      sbhand,  1,    1,             /* 0x4a  */
   "\tdec\tbx",      sbhand,  1,    1,             /* 0x4b  */
   "\tdec\tsp",      sbhand,  1,    1,             /* 0x4c  */
   "\tdec\tbp",      sbhand,  1,    1,             /* 0x4d  */
   "\tdec\tsi",      sbhand,  1,    1,             /* 0x4e  */
   "\tdec\tdi",      sbhand,  1,    1,             /* 0x4f  */
   "\tpush\tax",     sbhand,  1,    1,             /* 0x50  */
   "\tpush\tcx",     sbhand,  1,    1,             /* 0x51  */
   "\tpush\tdx",     sbhand,  1,    1,             /* 0x52  */
   "\tpush\tbx",     sbhand,  1,    1,             /* 0x53  */
   "\tpush\tsp",     sbhand,  1,    1,             /* 0x54  */
   "\tpush\tbp",     sbhand,  1,    1,             /* 0x55  */
   "\tpush\tsi",     sbhand,  1,    1,             /* 0x56  */
   "\tpush\tdi",     sbhand,  1,    1,             /* 0x57  */
   "\tpop\tax",      sbhand,  1,    1,             /* 0x58  */
   "\tpop\tcx",      sbhand,  1,    1,             /* 0x59  */
   "\tpop\tdx",      sbhand,  1,    1,             /* 0x5a  */
   "\tpop\tbx",      sbhand,  1,    1,             /* 0x5b  */
   "\tpop\tsp",      sbhand,  1,    1,             /* 0x5c  */
   "\tpop\tbp",      sbhand,  1,    1,             /* 0x5d  */
   "\tpop\tsi",      sbhand,  1,    1,             /* 0x5e  */
   "\tpop\tdi",      sbhand,  1,    1,             /* 0x5f  */
   NULL,             dfhand,  0,    0,             /* 0x60  */
   NULL,             dfhand,  0,    0,             /* 0x61  */
   NULL,             dfhand,  0,    0,             /* 0x62  */
   NULL,             dfhand,  0,    0,             /* 0x63  */
   NULL,             dfhand,  0,    0,             /* 0x64  */
   NULL,             dfhand,  0,    0,             /* 0x65  */
   NULL,             dfhand,  0,    0,             /* 0x66  */
   NULL,             dfhand,  0,    0,             /* 0x67  */
   NULL,             dfhand,  0,    0,             /* 0x68  */
   NULL,             dfhand,  0,    0,             /* 0x69  */
   NULL,             dfhand,  0,    0,             /* 0x6a  */
   NULL,             dfhand,  0,    0,             /* 0x6b  */
   NULL,             dfhand,  0,    0,             /* 0x6c  */
   NULL,             dfhand,  0,    0,             /* 0x6d  */
   NULL,             dfhand,  0,    0,             /* 0x6e  */
   NULL,             dfhand,  0,    0,             /* 0x6f  */
   "\tjo",           sjhand,  2,    2,             /* 0x70  */
   "\tjno",          sjhand,  2,    2,             /* 0x71  */
   "\tjc",           sjhand,  2,    2,             /* 0x72  */
   "\tjnc",          sjhand,  2,    2,             /* 0x73  */
   "\tjz",           sjhand,  2,    2,             /* 0x74  */
   "\tjnz",          sjhand,  2,    2,             /* 0x75  */
   "\tjna",          sjhand,  2,    2,             /* 0x76  */
   "\tja",           sjhand,  2,    2,             /* 0x77  */
   "\tjs",           sjhand,  2,    2,             /* 0x78  */
   "\tjns",          sjhand,  2,    2,             /* 0x79  */
   "\tjp",           sjhand,  2,    2,             /* 0x7a  */
   "\tjnp",          sjhand,  2,    2,             /* 0x7b  */
   "\tjl",           sjhand,  2,    2,             /* 0x7c  */
   "\tjnl",          sjhand,  2,    2,             /* 0x7d  */
   "\tjng",          sjhand,  2,    2,             /* 0x7e  */
   "\tjg",           sjhand,  2,    2,             /* 0x7f  */
   AMBIG,            imhand,  3,    5,             /* 0x80  */
   AMBIG,            imhand,  4,    6,             /* 0x81  */
   AMBIG,            imhand,  3,    5,             /* 0x82  */
   AMBIG,            imhand,  3,    5,             /* 0x83  */
   TEST,             mvhand,  2,    4,             /* 0x84  */
   TEST,             mvhand,  2,    4,             /* 0x85  */
   "\txchg",         mvhand,  2,    4,             /* 0x86  */
   "\txchg",         mvhand,  2,    4,             /* 0x87  */
   MOV,              mvhand,  2,    4,             /* 0x88  */
   MOV,              mvhand,  2,    4,             /* 0x89  */
   MOV,              mvhand,  2,    4,             /* 0x8a  */
   MOV,              mvhand,  2,    4,             /* 0x8b  */
   MOV,              mshand,  2,    4,             /* 0x8c  */
   "\tlea",          mvhand,  2,    4,             /* 0x8d  */
   MOV,              mshand,  2,    4,             /* 0x8e  */
   "\tpop",          pohand,  2,    4,             /* 0x8f  */
   "\tnop",          sbhand,  1,    1,             /* 0x90  */
   "\txchg\tax,cx",  sbhand,  1,    1,             /* 0x91  */
   "\txchg\tax,dx",  sbhand,  1,    1,             /* 0x92  */
   "\txchg\tax,bx",  sbhand,  1,    1,             /* 0x93  */
   "\txchg\tax,sp",  sbhand,  1,    1,             /* 0x94  */
   "\txchg\tax,bp",  sbhand,  1,    1,             /* 0x95  */
   "\txchg\tax,si",  sbhand,  1,    1,             /* 0x96  */
   "\txchg\tax,di",  sbhand,  1,    1,             /* 0x97  */
   "\tcbw",          sbhand,  1,    1,             /* 0x98  */
   "\tcwd",          sbhand,  1,    1,             /* 0x99  */
   "\tcalli",        cihand,  5,    5,             /* 0x9a  */
   "\twait",         sbhand,  1,    1,             /* 0x9b  */
   "\tpushf",        sbhand,  1,    1,             /* 0x9c  */
   "\tpopf",         sbhand,  1,    1,             /* 0x9d  */
   "\tsahf",         sbhand,  1,    1,             /* 0x9e  */
   "\tlahf",         sbhand,  1,    1,             /* 0x9f  */
   MOV,              mqhand,  3,    3,             /* 0xa0  */
   MOV,              mqhand,  3,    3,             /* 0xa1  */
   MOV,              mqhand,  3,    3,             /* 0xa2  */
   MOV,              mqhand,  3,    3,             /* 0xa3  */
   "\tmovb",         sbhand,  1,    1,             /* 0xa4  */
   "\tmovw",         sbhand,  1,    1,             /* 0xa5  */
   "\tcmpb",         sbhand,  1,    1,             /* 0xa6  */
   "\tcmpw",         sbhand,  1,    1,             /* 0xa7  */
   TEST,             tqhand,  2,    2,             /* 0xa8  */
   TEST,             tqhand,  3,    3,             /* 0xa9  */
   "\tstob",         sbhand,  1,    1,             /* 0xaa  */
   "\tstow",         sbhand,  1,    1,             /* 0xab  */
   "\tlodb",         sbhand,  1,    1,             /* 0xac  */
   "\tlodw",         sbhand,  1,    1,             /* 0xad  */
   "\tscab",         sbhand,  1,    1,             /* 0xae  */
   "\tscaw",         sbhand,  1,    1,             /* 0xaf  */
   "\tmov\tal,",     mihand,  2,    2,             /* 0xb0  */
   "\tmov\tcl,",     mihand,  2,    2,             /* 0xb1  */
   "\tmov\tdl,",     mihand,  2,    2,             /* 0xb2  */
   "\tmov\tbl,",     mihand,  2,    2,             /* 0xb3  */
   "\tmov\tah,",     mihand,  2,    2,             /* 0xb4  */
   "\tmov\tch,",     mihand,  2,    2,             /* 0xb5  */
   "\tmov\tdh,",     mihand,  2,    2,             /* 0xb6  */
   "\tmov\tbh,",     mihand,  2,    2,             /* 0xb7  */
   "\tmov\tax,",     mihand,  3,    3,             /* 0xb8  */
   "\tmov\tcx,",     mihand,  3,    3,             /* 0xb9  */
   "\tmov\tdx,",     mihand,  3,    3,             /* 0xba  */
   "\tmov\tbx,",     mihand,  3,    3,             /* 0xbb  */
   "\tmov\tsp,",     mihand,  3,    3,             /* 0xbc  */
   "\tmov\tbp,",     mihand,  3,    3,             /* 0xbd  */
   "\tmov\tsi,",     mihand,  3,    3,             /* 0xbe  */
   "\tmov\tdi,",     mihand,  3,    3,             /* 0xbf  */
   NULL,             dfhand,  0,    0,             /* 0xc0  */
   NULL,             dfhand,  0,    0,             /* 0xc1  */
   "\tret",          rehand,  3,    3,             /* 0xc2  */
   "\tret",          sbhand,  1,    1,             /* 0xc3  */
   "\tles",          mvhand,  2,    4,             /* 0xc4  */
   "\tlds",          mvhand,  2,    4,             /* 0xc5  */
   MOV,              mmhand,  3,    5,             /* 0xc6  */
   MOV,              mmhand,  4,    6,             /* 0xc7  */
   NULL,             dfhand,  0,    0,             /* 0xc8  */
   NULL,             dfhand,  0,    0,             /* 0xc9  */
   "\treti",         rehand,  3,    3,             /* 0xca  */
   "\treti",         sbhand,  1,    1,             /* 0xcb  */
   "\tint",          sbhand,  1,    1,             /* 0xcc  */
   "\tint",          inhand,  2,    2,             /* 0xcd  */
   "\tinto",         sbhand,  1,    1,             /* 0xce  */
   "\tiret",         sbhand,  1,    1,             /* 0xcf  */
   AMBIG,            srhand,  2,    4,             /* 0xd0  */
   AMBIG,            srhand,  2,    4,             /* 0xd1  */
   AMBIG,            srhand,  2,    4,             /* 0xd2  */
   AMBIG,            srhand,  2,    4,             /* 0xd3  */
   "\taam",          aahand,  2,    2,             /* 0xd4  */
   "\taad",          aahand,  2,    2,             /* 0xd5  */
   NULL,             dfhand,  0,    0,             /* 0xd6  */
   "\txlat",         sbhand,  1,    1,             /* 0xd7  */
   ESC,              eshand,  2,    2,             /* 0xd8  */
   ESC,              eshand,  2,    2,             /* 0xd9  */
   ESC,              eshand,  2,    2,             /* 0xda  */
   ESC,              eshand,  2,    2,             /* 0xdb  */
   ESC,              eshand,  2,    2,             /* 0xdc  */
   ESC,              eshand,  2,    2,             /* 0xdd  */
   ESC,              eshand,  2,    2,             /* 0xde  */
   ESC,              eshand,  2,    2,             /* 0xdf  */
   "\tloopne",       sjhand,  2,    2,             /* 0xe0  */
   "\tloope",        sjhand,  2,    2,             /* 0xe1  */
   "\tloop",         sjhand,  2,    2,             /* 0xe2  */
   "\tjcxz",         sjhand,  2,    2,             /* 0xe3  */
   "\tin",           iohand,  2,    2,             /* 0xe4  */
   "\tinw",          iohand,  2,    2,             /* 0xe5  */
   "\tout",          iohand,  2,    2,             /* 0xe6  */
   "\toutw",         iohand,  2,    2,             /* 0xe7  */
   "\tcall",         ljhand,  3,    3,             /* 0xe8  */
   "\tjmp",          ljhand,  3,    3,             /* 0xe9  */
   "\tjmpi",         cihand,  5,    5,             /* 0xea  */
   "\tj",            sjhand,  2,    2,             /* 0xeb  */
   "\tin",           sbhand,  1,    1,             /* 0xec  */
   "\tinw",          sbhand,  1,    1,             /* 0xed  */
   "\tout",          sbhand,  1,    1,             /* 0xee  */
   "\toutw",         sbhand,  1,    1,             /* 0xef  */
   "\tlock",         sbhand,  1,    1,             /* 0xf0  */
   NULL,             dfhand,  0,    0,             /* 0xf1  */
   "\trepnz",        sbhand,  1,    1,             /* 0xf2  */
   "\trepz",         sbhand,  1,    1,             /* 0xf3  */
   "\thlt",          sbhand,  1,    1,             /* 0xf4  */
   "\tcmc",          sbhand,  1,    1,             /* 0xf5  */
   AMBIG,            mahand,  2,    5,             /* 0xf6  */
   AMBIG,            mahand,  2,    6,             /* 0xf7  */
   "\tclc",          sbhand,  1,    1,             /* 0xf8  */
   "\tstc",          sbhand,  1,    1,             /* 0xf9  */
   "\tcli",          sbhand,  1,    1,             /* 0xfa  */
   "\tsti",          sbhand,  1,    1,             /* 0xfb  */
   "\tcld",          sbhand,  1,    1,             /* 0xfc  */
   "\tstd",          sbhand,  1,    1,             /* 0xfd  */
   AMBIG,            mjhand,  2,    4,             /* 0xfe  */
   AMBIG,            mjhand,  2,    4              /* 0xff  */
   };

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This simple routine  returns the name field of a symbol *
  * table entry as a printable string.                      *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

char *
getnam(k)

   register int k;

{/* * * * * * * * * *  START OF getnam()  * * * * * * * * * */

   register int j;
   static char a[9];

   for (j = 0; j < 8; ++j)
      if ( ! symtab[k].n_name[j] )
         break;
      else
         a[j] = symtab[k].n_name[j];

   a[j] = '\0';

   return (a);

}/* * * * * * * * * * * END OF getnam() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This function is  responsible  for mucking  through the *
  * relocation  table in  search of  externally  referenced *
  * symbols to be output as  operands.  It accepts two long *
  * arguments: the code-segment location at which an extern *
  * reference  is  expected,  and the offset value which is *
  * embedded  in the  object  code and used at link time to *
  * bias the external value.  In the most typical case, the *
  * function will be called by lookup(), which always makes *
  * a check for external names before  searching the symbol *
  * table proper.  However,  it may also be called directly *
  * by any function  (such as the  move-immediate  handler) *
  * which wants to make an independent check for externals. *
  * The caller is expected to supply, as the third argument *
  * to the function,  a pointer to a character buffer large *
  * enough to hold any possible  output  string.  Lookext() *
  * will fill this  buffer and return a logical  TRUE if it *
  * finds an extern reference;  otherwise, it will return a *
  * logical FALSE, leaving the buffer undisturbed.          *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

int
lookext(off,loc,buf)

   long off, loc;
   char *buf;

{/* * * * * * * * * * START OF  lookext() * * * * * * * * * */

   register int k;
   char c[16];

   if ((loc != -1L) && (relptr >= 0))
      for (k = 0; k <= relptr; ++k)
         if ((relo[k].r_vaddr == loc)
          && (relo[k].r_symndx < S_BSS))
            {
            strcpy(buf,getnam(relo[k].r_symndx));
            if (off)
               {
               if (off < 0)
                  sprintf(c,"%ld",off);
               else
                  sprintf(c,"+%ld",off);
               strcat(buf,c);
               }
            return (1);
            }

   return (0);

}/* * * * * * * * * *  END OF  lookext()  * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  function  finds an entry in the  symbol  table by *
  * value.  Its input is a (long) machine address,  and its *
  * output is a pointer to a string  containing  the corre- *
  * sponding symbolic name. The function first searches the *
  * relocation table for a possible external reference;  if *
  * none is found,  a linear  search of the symbol table is *
  * undertaken. If no matching symbol has been found at the *
  * end of these searches,  the function  returns a pointer *
  * to a string  containing the ASCII equivalent of the ad- *
  * dress which was to be located,  so that,  regardless of *
  * the success of the search,  the function's return value *
  * is suitable for use as a memory-reference operand.  The *
  * caller specifies the type of symbol to be found  (text, *
  * data, bss, undefined,  absolute, or common) by means of *
  * the function's  second  parameter.  The third parameter *
  * specifies  the  format to be used in the event of a nu- *
  * meric output:  zero for absolute format,  one for short *
  * relative  format,  two for long  relative  format.  The *
  * fourth  parameter is the address  which would appear in *
  * the relocation table for the reference in question,  or *
  * -1 if the relocation  table is not to be searched.  The *
  * function attempts to apply a certain amount of intelli- *
  * gence in its  selection  of symbols,  so it is possible *
  * that,  in the absence of a type match,  a symbol of the *
  * correct value but different type will be returned.      *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

char *
lookup(addr,type,kind,ext)

   long addr;              /* Machine address to be located */

   int type,               /* Type of symbol to be matched  */
       kind;               /* Addressing output mode to use */

   long ext;               /* Value for extern ref, if any  */

{/* * * * * * * * * *  START OF lookup()  * * * * * * * * * */

   register int j, k;
   static char b[64];

   struct
      {
      int   i;
      int   t;
      }
   best;

   if (lookext(addr,ext,b))
      return (b);

   if (segflg)
      if (segflg & 1)
         type = N_TEXT;
      else
         type = N_DATA;

   for (k = 0, best.i = -1; k <= symptr; ++k)
      if (symtab[k].n_value == addr)
         if ((j = symtab[k].n_sclass & N_SECT) == type)
            {
            best.t = j;
            best.i = k;
            break;
            }
         else if (segflg || (HDR.a_flags & A_SEP))
            continue;
         else if (best.i < 0)
            best.t = j, best.i = k;
         else if (symrank[type][j] > symrank[type][best.t])
            best.t = j, best.i = k;

   if (best.i >= 0)
      return (getnam(best.i));

   if (kind == LOOK_ABS)
      sprintf(b,"0x%05.5x",addr);
   else
      {
      long x = addr - (PC - kind);
      if (x < 0)
         sprintf(b,".%ld",x);
      else
         sprintf(b,".+%ld",x);
      }

   return (b);

}/* * * * * * * * * * * END OF lookup() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This function  translates an 8088  addressing mode byte *
  * to an equivalent assembler string,  returning a pointer *
  * thereto.  If necessary,  it performs  successive inputs *
  * of bytes from the object file in order to obtain offset *
  * data,  adjusting PC  accordingly.  (The addressing mode *
  * byte  appears in several  8088  opcodes;  it is used to *
  * specify source and destination operand locations.)  The *
  * third  argument to the function is zero if the standard *
  * registers are to be used,  or eight if the segment reg- *
  * isters are to be used; these constants are defined sym- *
  * bolically in dis.h.  NOTE:  The mtrans()  function must *
  * NEVER be called except  immediately  after fetching the *
  * mode byte.  If any additional  object bytes are fetched *
  * after  the fetch of the mode  byte,  mtrans()  will not *
  * produce correct output!                                 *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

char *
mtrans(c,m,type)

   register int c;            /* Primary instruction byte   */
   register int m;            /* Addressing mode byte       */

   int type;                  /* Type code: standard or seg */

{/* * * * * * * * * *  START OF mtrans()  * * * * * * * * * */

   unsigned long pc;
   int offset, oflag, dir, w, mod, reg, rm;
   static char a[100];
   static char b[30];

   offset = 0;
   dir = c & 2;
   w = c & 1;
   mod = (m & 0xc0) >> 6;
   reg = (m & 0x38) >> 3;
   rm = m & 7;
   pc = PC + 1;

   if (type)
      w = 1;

   if ((oflag = mod) > 2)
      oflag = 0;

   if (oflag)
      {
      int j, k;
      if (oflag == 2)
         {
         FETCH(j);
         FETCH(k);
         offset = (k << 8) | j;
         }
      else
         {
         FETCH(j);
         if (j & 0x80)
            k = 0xff00;
         else
            k = 0;
         offset = k | j;
         }
      }

   if (dir)
      {
      strcpy(a,REGS[type + ((w << 3) | reg)]);
      strcat(a,",");
      switch (mod)
         {
         case 0 :
            if (rm == 6)
               {
               int j, k;
               FETCH(j);
               FETCH(k);
               offset = (k << 8) | j;
               strcat(a,
                lookup((long)(offset),N_DATA,LOOK_ABS,pc));
               }
            else
               {
               sprintf(b,"(%s)",REGS0[rm]);
               strcat(a,b);
               }
            break;
         case 1 :
         case 2 :
            if (mod == 1)
               strcat(a,"*");
            else
               strcat(a,"#");
            sprintf(b,"%d(",offset);
            strcat(a,b);
            strcat(a,REGS1[rm]);
            strcat(a,")");
            break;
         case 3 :
            strcat(a,REGS[(w << 3) | rm]);
            break;
         }
      }
   else
      {
      switch (mod)
         {
         case 0 :
            if (rm == 6)
               {
               int j, k;
               FETCH(j);
               FETCH(k);
               offset = (k << 8) | j;
               strcpy(a,
                lookup((long)(offset),N_DATA,LOOK_ABS,pc));
               }
            else
               {
               sprintf(b,"(%s)",REGS0[rm]);
               strcpy(a,b);
               }
            break;
         case 1 :
         case 2 :
            if (mod == 1)
               strcpy(a,"*");
            else
               strcpy(a,"#");
            sprintf(b,"%d(",offset);
            strcat(a,b);
            strcat(a,REGS1[rm]);
            strcat(a,")");
            break;
         case 3 :
            strcpy(a,REGS[(w << 3) | rm]);
            break;
         }
      strcat(a,",");
      strcat(a,REGS[type + ((w << 3) | reg)]);
      }

   return (a);

}/* * * * * * * * * * * END OF mtrans() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This simple routine  truncates a string returned by the *
  * mtrans() function, removing its source operand. This is *
  * useful in handlers which ignore the "reg"  field of the *
  * mode byte.                                              *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mtrunc(a)

   register char *a;          /* Ptr. to string to truncate */

{/* * * * * * * * * *  START OF mtrunc()  * * * * * * * * * */

   register int k;

   for (k = strlen(a) - 1; k >= 0; --k)
      if (a[k] == ',')
         {
         a[k] = '\0';
         break;
         }

}/* * * * * * * * * * * END OF mtrunc() * * * * * * * * * * */


