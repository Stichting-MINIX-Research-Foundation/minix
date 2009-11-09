 /*
 ** @(#) dis.h, Ver. 2.1 created 00:00:00 87/09/01
 */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  *  Copyright (C) 1987 G. M. Harding, all rights reserved  *
  *                                                         *
  * Permission to copy and  redistribute is hereby granted, *
  * provided full source code,  with all copyright notices, *
  * accompanies any redistribution.                         *
  *                                                         *
  * This file contains declarations and definitions used by *
  * the 8088 disassembler program. The program was designed *
  * for execution on a machine of its own type (i.e., it is *
  * not designed as a cross-disassembler);  consequently, A *
  * SIXTEEN-BIT INTEGER SIZE HAS BEEN ASSUMED. This assump- *
  * tion is not particularly important,  however, except in *
  * the machine-specific  portions of the code  (i.e.,  the *
  * handler  routines and the optab[] array).  It should be *
  * possible to override this assumption,  for execution on *
  * 32-bit machines,  by use of a  pre-processor  directive *
  * (see below); however, this has not been tested.         *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <sys/types.h>
#include <a.out.h>      /* Object file format definitions   */
#include <fcntl.h>      /* System file-control definitions  */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>      /* System standard I/O definitions  */

#define MAXSYM 1500     /* Maximum entries in symbol table  */

extern struct nlist     /* Array to hold the symbol table   */
   symtab[MAXSYM];

extern struct reloc     /* Array to hold relocation table   */
   relo[MAXSYM];

extern int symptr;      /* Index into the symtab[] array    */
extern int relptr;      /* Index into the relo[] array      */

struct opcode           /* Format for opcode data records   */
{
   char *text;          /* Pointer to mnemonic text   */
   void (*func)();      /* Pointer to handler routine */
   unsigned min;        /* Minimum # of object bytes  */
   unsigned max;        /* Maximum # of object bytes  */
};

extern struct opcode    /* Array to hold the opcode table   */
  optab[256];

extern char *REGS[];    /* Table of register names          */
extern char *REGS0[];   /* Mode 0 register name table       */
extern char *REGS1[];   /* Mode 1 register name table       */

#define AL REGS[0]      /* CPU register manifests           */
#define CL REGS[1]
#define DL REGS[2]
#define BL REGS[3]
#define AH REGS[4]
#define CH REGS[5]
#define DH REGS[6]
#define BH REGS[7]
#define AX REGS[8]
#define CX REGS[9]
#define DX REGS[10]
#define BX REGS[11]
#define SP REGS[12]
#define BP REGS[13]
#define SI REGS[14]
#define DI REGS[15]
#define ES REGS[16]
#define CS REGS[17]
#define SS REGS[18]
#define DS REGS[19]
#define BX_SI REGS0[0]
#define BX_DI REGS0[1]
#define BP_SI REGS0[2]
#define BP_DI REGS0[3]

extern int symrank[6][6];     /* Symbol type/rank matrix    */
extern unsigned long PC;      /* Current program counter    */
extern int segflg;      /* Flag: segment override in effect */
extern int objflg;      /* Flag: output object as a comment */

#define OBJMAX 8        /* Size of the object code buffer   */

extern unsigned char    /* Internal buffer for object code  */
   objbuf[OBJMAX];

extern int objptr;      /* Index into the objbuf[] array    */

extern char ADD[],      /* Opcode family mnemonic strings   */
            OR[],
            ADC[],
            SBB[],
            AND[],
            SUB[],
            XOR[],
            CMP[],
            NOT[],
            NEG[],
            MUL[],
            DIV[],
            MOV[],
            ESC[],
            TEST[],
            AMBIG[];

extern char *OPFAM[];   /* Indexed mnemonic family table    */
extern struct exec HDR; /* Holds the object file's header   */

#define LOOK_ABS 0      /* Arguments to lookup() function   */
#define LOOK_REL 1
#define LOOK_LNG 2

#define TR_STD 0        /* Arguments to mtrans() function   */
#define TR_SEG 8

                        /* Macro for byte input primitive   */
#define FETCH(p)  ++PC; p = getchar() & 0xff; objbuf[objptr++] = p


/* disfp.c */
_PROTOTYPE(void eshand, (int j ));
_PROTOTYPE(void fphand, (int j ));
_PROTOTYPE(void inhand, (int j ));

/* dishand.c */
_PROTOTYPE(void objini, (int j ));
_PROTOTYPE(void objout, (void));
_PROTOTYPE(void badseq, (int j, int k ));
_PROTOTYPE(void dfhand, (int j ));
_PROTOTYPE(void sbhand, (int j ));
_PROTOTYPE(void aohand, (int j ));
_PROTOTYPE(void sjhand, (int j ));
_PROTOTYPE(void imhand, (int j ));
_PROTOTYPE(void mvhand, (int j ));
_PROTOTYPE(void mshand, (int j ));
_PROTOTYPE(void pohand, (int j ));
_PROTOTYPE(void cihand, (int j ));
_PROTOTYPE(void mihand, (int j ));
_PROTOTYPE(void mqhand, (int j ));
_PROTOTYPE(void tqhand, (int j ));
_PROTOTYPE(void rehand, (int j ));
_PROTOTYPE(void mmhand, (int j ));
_PROTOTYPE(void srhand, (int j ));
_PROTOTYPE(void aahand, (int j ));
_PROTOTYPE(void iohand, (int j ));
_PROTOTYPE(void ljhand, (int j ));
_PROTOTYPE(void mahand, (int j ));
_PROTOTYPE(void mjhand, (int j ));

/* dismain.c */
_PROTOTYPE(void main, (int argc, char **argv ));

/* distabs.c */
_PROTOTYPE(char *getnam, (int k ));
_PROTOTYPE(int lookext, (long off, long loc, char *buf ));
_PROTOTYPE(char *lookup, (long addr, int type, int kind, long ext ));
_PROTOTYPE(char *mtrans, (int c, int m, int type ));
_PROTOTYPE(void mtrunc, (char *a ));
