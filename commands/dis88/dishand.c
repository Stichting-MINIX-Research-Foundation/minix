static char *sccsid =
   "@(#) dishand.c, Ver. 2.1 created 00:00:00 87/09/01";

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  *  Copyright (C) 1987 G. M. Harding, all rights reserved  *
  *                                                         *
  * Permission to copy and  redistribute is hereby granted, *
  * provided full source code,  with all copyright notices, *
  * accompanies any redistribution.                         *
  *                                                         *
  * This file contains the source code for most of the spe- *
  * cialized handler routines of the disassembler  program. *
  * (The file disfp.c contains handler routines specific to *
  * the 8087 numeric  co-processor.)  Each handler  routine *
  * interprets the opcode byte  (and subsequent data bytes, *
  * if any)  of a particular family of opcodes,  and is re- *
  * sponsible for generating appropriate output. All of the *
  * code in this file is highly MACHINE-SPECIFIC, and would *
  * have to be rewritten for a different  CPU.  The handler *
  * routines are accessed  only via pointers in the optab[] *
  * array, however, so machine dependencies are confined to *
  * this file, its sister file "disfp.c", and the data file *
  * "distabs.c".                                            *
  *                                                         *
  * All of the code in this file is based on the assumption *
  * of sixteen-bit integers.                                *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "dis.h"              /* Disassembler declarations  */

int segflg;                   /* Segment-override flag      */

unsigned char objbuf[OBJMAX]; /* Buffer for object code     */

int objptr;                   /* Index into objbuf[]        */

unsigned long PC;             /* Current program counter    */

 /* * * * * *  MISCELLANEOUS SUPPORTING ROUTINES  * * * * * */


void
objini(j)                     /* Object code init routine   */

   register int j;

{
   if ((segflg == 1) || (segflg == 2))
      segflg *= 3;
   else
      segflg = 0;
   objptr = 0;
   objbuf[objptr++] = (unsigned char)(j);
}


void
objout()                      /* Object-code output routine */

{
    register int k;

   if ( ! objflg )
      return;
   else
      {
      printf("\t|");
      if (symptr >= 0)
         printf(" %05.5lx:",(PC + 1L - (long)(objptr)));
      for (k = 0; k < objptr; ++k)
         printf(" %02.2x",objbuf[k]);
      putchar('\n');
      }
}


void
badseq(j,k)                   /* Invalid-sequence routine   */

   register int j, k;

{
   printf("\t.byte\t0x%02.2x\t\t| invalid code sequence\n",j);
   printf("\t.byte\t0x%02.2x\n",k);
}

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  routine  is the first of several  opcode-specific *
  * handlers,  each of which is  dedicated  to a particular *
  * opcode family.  A pointer to a handler  routine is con- *
  * tained in the second field of each optab[]  entry.  The *
  * dfhand()  routine is the default handler,  invoked when *
  * no other handler is appropriate (generally, when an in- *
  * valid opcode is encountered).                           *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
dfhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF dfhand()  * * * * * * * * * */

   segflg = 0;

   printf("\t.byte\t0x%02.2x",j);

   if (optab[j].min || optab[j].max)
      putchar('\n');
   else
      printf("\t\t| unimplemented opcode\n");

}/* * * * * * * * * * * END OF dfhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  single-byte  handler,  invoked  whenever a *
  * one-byte opcode is encountered.                         *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
sbhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF sbhand()  * * * * * * * * * */

   objini(j);

   if (j == 0x2e)                               /* seg cs   */
      segflg = 1;

   if ((j == 0x26)                              /* seg es   */
    || (j == 0x36)                              /* seg ss   */
    || (j == 0x3e))                             /* seg ds   */
      segflg = 2;

   printf("%s\n",optab[j].text);

   objout();

}/* * * * * * * * * * * END OF sbhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for most of the processor's regular *
  * arithmetic operations.                                  *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
aohand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF aohand()  * * * * * * * * * */

   register int k;
   int m, n;
   char b[64];

   objini(j);

   switch (j & 7)
      {
      case 0 :
      case 1 :
      case 2 :
      case 3 :
         printf("%s\t",optab[j].text);
         FETCH(k);
         printf("%s\n",mtrans(j,k,TR_STD));
         break;
      case 4 :
         FETCH(k);
         printf("%s\tal,*0x%02.2x\n",optab[j].text,k);
         break;
      case 5 :
         FETCH(m);
         FETCH(n);
         k = (n << 8) | m;
         if (lookext((long)(k),(PC - 1),b))
            printf("%s\tax,#%s\n",optab[j].text,b);
         else
            printf("%s\tax,#0x%04.4x\n",optab[j].text,k);
         break;
      default :
         dfhand(j);
         break;
      }

   objout();

}/* * * * * * * * * * * END OF aohand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler for opcodes  which  perform  short *
  * (eight-bit) relative jumps.                             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
sjhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF sjhand()  * * * * * * * * * */

   register int k;
   int m;

   objini(j);

   FETCH(m);

   if (m & 0x80)
      k = 0xff00;
   else
      k = 0;

   k |= m;

   printf("%s\t%s\t\t| loc %05.5lx\n",optab[j].text,
    lookup((PC + k + 1L),N_TEXT,LOOK_REL,-1L),
    (PC + k + 1L));

   objout();

}/* * * * * * * * * * * END OF sjhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler for a  loosely-knit  family of op- *
  * codes which perform  arithmetic and logical operations, *
  * and which take immediate  data.  The routine's logic is *
  * rather complex,  so,  in an effort to avoid  additional *
  * complexity,  the search for external  references in the *
  * relocation table has been dispensed with. Eager hackers *
  * can try their hand at coding such a search.             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
imhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF imhand()  * * * * * * * * * */

   unsigned long pc;
   register int k;
   int offset, oflag, immed, iflag, mod, opi, w, rm;
   int m, n;
   static char a[100], b[30];

   objini(j);

   FETCH(k);

   pc = PC + 1;

   offset = 0;
   mod = (k & 0xc0) >> 6;
   opi = (k & 0x38) >> 3;
   w = j & 1;
   rm = k & 7;

   if ((j & 2)
    && ((opi == 1)
     || (opi == 4)
     || (opi == 6)))
      {
      badseq(j,k);
      return;
      }

   strcpy(a,OPFAM[opi]);

   if ( ! w )
      strcat(a,"b");

   if ((oflag = mod) > 2)
      oflag = 0;

   if ((mod == 0) && (rm == 6))
      {
      FETCH(m);
      FETCH(n);
      offset = (n << 8) | m;
      }
   else if (oflag)
      if (oflag == 2)
         {
         FETCH(m);
         FETCH(n);
         offset = (n << 8) | m;
         }
      else
         {
         FETCH(m);
         if (m & 0x80)
            n = 0xff00;
         else
            n = 0;
         offset = n | m;
         }

   switch (j & 3)
      {
      case 0 :
      case 2 :
         FETCH(immed);
         iflag = 0;
         break;
      case 1 :
         FETCH(m);
         FETCH(n);
         immed = (n << 8) | m;
         iflag = 1;
         break;
      case 3 :
         FETCH(immed);
         if (immed & 0x80)
            immed &= 0xff00;
         iflag = 0;
         break;
      }

   strcat(a,"\t");

   switch (mod)
      {
      case 0 :
         if (rm == 6)
            strcat(a,
             lookup((long)(offset),N_DATA,LOOK_ABS,pc));
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

   strcat(a,",");
   if (iflag)
      strcat(a,"#");
   else
      strcat(a,"*");
   sprintf(b,"%d",immed);
   strcat(a,b);

   printf("%s\n",a);

   objout();

}/* * * * * * * * * * * END OF imhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler  for  various  "mov"-type  opcodes *
  * which use the mod,  reg,  and r/m  fields of the second *
  * code byte in a standard, straightforward way.           *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mvhand(j)

   int j;                     /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mvhand()  * * * * * * * * * */

   register int k, m = j;

   objini(j);

   FETCH(k);

   if ((m == 0x84) || (m == 0x85)      /* Kind of kludgey   */
    || (m == 0xc4) || (m == 0xc5)
    || (m == 0x8d))
      if (m & 0x40)
         m |= 0x03;
      else
         m |= 0x02;

   printf("%s\t%s\n",optab[j].text,mtrans(m,k,TR_STD));

   objout();

}/* * * * * * * * * * * END OF mvhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for segment-register "mov" opcodes. *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mshand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mshand()  * * * * * * * * * */

   register int k;

   objini(j);

   FETCH(k);

   if (k & 0x20)
      {
      badseq(j,k);
      return;
      }

   printf("%s\t%s\n",optab[j].text,mtrans(j,k,TR_SEG));

   objout();

}/* * * * * * * * * * * END OF mshand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler for pops,  other than  single-byte *
  * pops.  (The 8088 allows  popping into any register,  or *
  * directly into memory,  accessed  either  immediately or *
  * through a register and an index.)                       *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
pohand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF pohand()  * * * * * * * * * */

   char *a;
   register int k;

   objini(j);

   FETCH(k);

   if (k & 0x38)
      {
      badseq(j,k);
      return;
      }

   printf("%s\t",optab[j].text);

   a = mtrans((j & 0xfd),k,TR_STD);

   mtrunc(a);

   printf("%s\n",a);

   objout();

}/* * * * * * * * * * * END OF pohand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler routine for intersegment  calls and *
  * jumps.  Its output is never symbolic,  because the host *
  * linker  does not allow  symbolic  intersegment  address *
  * references except by means of symbolic  constants,  and *
  * any such  constants in the symbol  table,  even if they *
  * are of the  appropriate  value,  may be misleading.  In *
  * compiled code,  intersegment  references  should not be *
  * encountered,  and even in assembled  code,  they should *
  * occur infrequently. If and when they do occur, however, *
  * they will be disassembled in absolute form.             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
cihand(j)

   int j;                     /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF cihand()  * * * * * * * * * */

   register int m, n;

   objini(j);

   printf("%s\t",optab[j].text);

   FETCH(m);
   FETCH(n);

   printf("#0x%04.4x,",((n << 8) | m));

   FETCH(m);
   FETCH(n);

   printf("#0x%04.4x\n",((n << 8) | m));

   objout();

}/* * * * * * * * * * * END OF cihand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for  "mov"  opcodes with  immediate *
  * data.                                                   *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mihand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mihand()  * * * * * * * * * */

   register int k;
   int m, n;
   char b[64];

   objini(j);

   printf("%s",optab[j].text);

   if (j & 8)
      {
      FETCH(m);
      FETCH(n);
      k = ((n << 8) | m);
      if (lookext((long)(k),(PC - 1),b))
         printf("#%s\n",b);
      else
         printf("#%d\n",k);
      }
   else
      {
      FETCH(m);
      printf("*%d\n",m);
      }

   objout();

}/* * * * * * * * * * * END OF mihand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for a family of quick-move opcodes. *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mqhand(j)

   int j;                     /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mqhand()  * * * * * * * * * */

   unsigned long pc;
   register int m, n;

   objini(j);

   pc = PC + 1;

   FETCH(m);
   FETCH(n);

   m = (n << 8) | m;

   printf("%s\t",optab[j].text);

   if (j & 2)
      printf("%s,%s\n",
       lookup((long)(m),N_DATA,LOOK_ABS,pc),
       REGS[(j & 1) << 3]);
   else
      printf("%s,%s\n",
       REGS[(j & 1) << 3],
       lookup((long)(m),N_DATA,LOOK_ABS,pc));

   objout();

}/* * * * * * * * * * * END OF mqhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for a family of quick-test opcodes. *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
tqhand(j)

   int j;                     /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF tqhand()  * * * * * * * * * */

   register int m, n;
   int k;
   char b[64];

   objini(j);

   printf("%s\t%s,",optab[j].text,REGS[(j & 1) << 3]);

   FETCH(m);

   if (j & 1)
      {
      FETCH(n);
      k = ((n << 8) | m);
      if (lookext((long)(k),(PC - 1),b))
         printf("#%s\n",b);
      else
         printf("#%d\n",k);
      }
   else
      {
      if (m & 80)
         m |= 0xff00;
      printf("*%d\n",m);
      }

   objout();

}/* * * * * * * * * * * END OF tqhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for multiple-byte "return" opcodes. *
  * The 8088 allows returns to take an optional  16-bit ar- *
  * gument,  which  reflects  the  amount to be added to SP *
  * after  the pop of the  return  address.  The idea is to *
  * facilitate  the use of local  parameters  on the stack. *
  * After some  rumination,  it was decided to  disassemble *
  * any such arguments as absolute quantities,  rather than *
  * rummaging  through the symbol table for possible corre- *
  * sponding constants.                                     *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
rehand(j)

   int j;                     /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF rehand()  * * * * * * * * * */

   register int m, n;

   objini(j);

   FETCH(m);
   FETCH(n);

   m = (n << 8) | m;

   printf("%s\t#0x%04.4x\n",optab[j].text,m);

   objout();

}/* * * * * * * * * * * END OF rehand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for "mov"  opcodes involving memory *
  * and immediate data.                                     *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mmhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mmhand()  * * * * * * * * * */

   char *a;
   register int k;
   char b[64];

   objini(j);

   FETCH(k);

   if (k & 0x38)
      {
      badseq(j,k);
      return;
      }

   printf("%s",optab[j].text);

   if ( ! (j & 1) )
      putchar('b');

   a = mtrans((j & 0xfd),(k & 0xc7),TR_STD);

   mtrunc(a);

   printf("\t%s,",a);

   if (j & 1)
      {
      FETCH(j);
      FETCH(k);
      k = (k << 8) | j;
      if (lookext((long)(k),(PC - 1),b))
         printf("#%s\n",b);
      else
         printf("#%d\n",k);
      }
   else
      {
      FETCH(k);
      printf("*%d\n",k);
      }

   objout();

}/* * * * * * * * * * * END OF mmhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler  for the 8088  family of shift and *
  * rotate instructions.                                    *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
srhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF srhand()  * * * * * * * * * */

   char *a;
   register int k;

   objini(j);

   FETCH(k);

   if ((k & 0x38) == 0x30)
      {
      badseq(j,k);
      return;
      }

   printf("%s",OPFAM[((k & 0x38) >> 3) + 16]);

   if ( ! (j & 1) )
      putchar('b');

   a = mtrans((j & 0xfd),(k & 0xc7),TR_STD);

   mtrunc(a);

   printf("\t%s",a);

   if (j & 2)
      printf(",cl\n");
   else
      printf(",*1\n");

   objout();

}/* * * * * * * * * * * END OF srhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for the ASCII-adjust opcodes.       *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
aahand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF aahand()  * * * * * * * * * */

   register int k;

   objini(j);

   FETCH(k);

   if (k != 0x0a)
      {
      badseq(j,k);
      return;
      }

   printf("%s\n",optab[j].text);

   objout();

}/* * * * * * * * * * * END OF aahand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for port I/O opcodes  which specify *
  * the port address as an immediate operand.               *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
iohand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF iohand()  * * * * * * * * * */

   register int k;

   objini(j);

   FETCH(k);

   printf("%s\t0x%02.2x\n",optab[j].text,k);

   objout();

}/* * * * * * * * * * * END OF iohand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler  for opcodes  which  perform  long *
  * (sixteen-bit) relative jumps and calls.                 *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
ljhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF ljhand()  * * * * * * * * * */

   register int k;
   int m, n;

   objini(j);

   FETCH(m);
   FETCH(n);

   k = (n << 8) | m;

   printf("%s\t%s\t\t| loc %05.5lx\n",optab[j].text,
    lookup((PC + k + 1L),N_TEXT,LOOK_LNG,(PC - 1L)),
    (PC + k + 1L));

   objout();

}/* * * * * * * * * * * END OF ljhand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for a pair of oddball opcodes (0xf6 *
  * and 0xf7) which perform miscellaneous arithmetic opera- *
  * tions not dealt with elsewhere.                         *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mahand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mahand()  * * * * * * * * * */

   char *a;
   register int k;
   char b[64];

   objini(j);

   FETCH(k);

   a = mtrans((j & 0xfd),(k & 0xc7),TR_STD);

   mtrunc(a);

   switch (((k = objbuf[1]) & 0x38) >> 3)
      {
      case 0 :
         printf("\ttest");
         break;
      case 1 :
         badseq(j,k);
         return;
      case 2 :
         printf("\tnot");
         break;
      case 3 :
         printf("\tneg");
         break;
      case 4 :
         printf("\tmul");
         break;
      case 5 :
         printf("\timul");
         break;
      case 6 :
         printf("\tdiv");
         break;
      case 7 :
         printf("\tidiv");
         break;
      }

   if ( ! (j & 1) )
      putchar('b');

   printf("\t%s",a);

   if (k & 0x38)
      putchar('\n');
   else
      if (j & 1)
         {
         FETCH(j);
         FETCH(k);
         k = (k << 8) | j;
         if (lookext((long)(k),(PC - 1),b))
            printf(",#%s\n",b);
         else
            printf(",#%d\n",k);
         }
      else
         {
         FETCH(k);
         printf(",*%d\n",k);
         }

   objout();

}/* * * * * * * * * * * END OF mahand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler for miscellaneous jump, call, push, *
  * and increment/decrement opcodes  (0xfe and 0xff)  which *
  * are not dealt with elsewhere.                           *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
mjhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF mjhand()  * * * * * * * * * */

   char *a;
   register int k;

   objini(j);

   FETCH(k);

   a = mtrans((j & 0xfd),(k & 0xc7),TR_STD);

   mtrunc(a);

   switch (((k = objbuf[1]) & 0x38) >> 3)
      {
      case 0 :
         printf("\tinc");
         if ( ! (j & 1) )
            putchar('b');
         putchar('\t');
         break;
      case 1 :
         printf("\tdec");
         if ( ! (j & 1) )
            putchar('b');
         putchar('\t');
         break;
      case 2 :
         if (j & 1)
            printf("\tcall\t@");
         else
            goto BAD;
         break;
      case 3 :
         if (j & 1)
            printf("\tcalli\t@");
         else
            goto BAD;
         break;
      case 4 :
         if (j & 1)
            printf("\tjmp\t@");
         else
            goto BAD;
         break;
      case 5 :
         if (j & 1)
            printf("\tjmpi\t@");
         else
            goto BAD;
         break;
      case 6 :
         if (j & 1)
            printf("\tpush\t");
         else
            goto BAD;
         break;
      case 7 :
 BAD :
         badseq(j,k);
         return;
      }

   printf("%s\n",a);

   objout();

}/* * * * * * * * * * * END OF mjhand() * * * * * * * * * * */


