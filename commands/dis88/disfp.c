static char *sccsid =
   "@(#) disfp.c, Ver. 2.1 created 00:00:00 87/09/01";

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  *  Copyright (C) 1987 G. M. Harding, all rights reserved  *
  *                                                         *
  * Permission to copy and  redistribute is hereby granted, *
  * provided full source code,  with all copyright notices, *
  * accompanies any redistribution.                         *
  *                                                         *
  * This file contains handler routines for the numeric op- *
  * codes of the 8087 co-processor,  as well as a few other *
  * opcodes which are related to 8087 emulation.            *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "dis.h"              /* Disassembler declarations  */

#define FPINT0 0xd8           /* Floating-point interrupts  */
#define FPINT1 0xd9
#define FPINT2 0xda
#define FPINT3 0xdb
#define FPINT4 0xdc
#define FPINT5 0xdd
#define FPINT6 0xde
#define FPINT7 0xdf

                              /* Test for floating opcodes  */
#define ISFLOP(x) \
   (((x) >= FPINT0) && ((x) <= FPINT7))

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler for the escape  family of opcodes. *
  * These opcodes place the contents of a specified  memory *
  * location on the system bus,  for access by a peripheral *
  * or by a co-processor such as the 8087. (The 8087 NDP is *
  * accessed  only  via bus  escapes.)  Due to a bug in the *
  * PC/IX assembler,  the "esc" mnemonic is not recognized; *
  * consequently,  escape opcodes are disassembled as .byte *
  * directives,  with the appropriate  mnemonic and operand *
  * included as a comment.  FOR NOW, those escape sequences *
  * corresponding  to 8087  opcodes  are  treated as simple *
  * escapes.                                                *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
eshand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF eshand()  * * * * * * * * * */

   register char *a;
   register int k;

   objini(j);

   FETCH(k);

   a = mtrans((j & 0xfd),(k & 0xc7),TR_STD);

   mtrunc(a);

   printf("\t.byte\t0x%02.2x\t\t| esc\t%s\n",j,a);

   for (k = 1; k < objptr; ++k)
      printf("\t.byte\t0x%02.2x\n",objbuf[k]);

}/* * * * * * * * * * * END OF eshand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the handler routine for floating-point opcodes. *
  * Since PC/IX must  accommodate  systems with and without *
  * 8087 co-processors, it allows floating-point operations *
  * to be  initiated  in either of two ways:  by a software *
  * interrput whose type is in the range 0xd8 through 0xdf, *
  * or by a CPU escape sequence, which is invoked by an op- *
  * code in the same range.  In either case, the subsequent *
  * byte determines the actual numeric operation to be per- *
  * formed.  However,  depending  on the  method of access, *
  * either  one or two code bytes will  precede  that byte, *
  * and the fphand()  routine has no way of knowing whether *
  * it was invoked by  interrupt or by an escape  sequence. *
  * Therefore, unlike all of the other handler routines ex- *
  * cept dfhand(),  fphand() does not initialize the object *
  * buffer, leaving that chore to the caller.               *
  *                                                         *
  * FOR NOW,  fphand()  does not disassemble floating-point *
  * opcodes to floating  mnemonics,  but simply outputs the *
  * object code as .byte directives.                        *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
fphand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF fphand()  * * * * * * * * * */

   register int k;

   segflg = 0;

   FETCH(k);

   printf("\t.byte\t0x%02.2x\t\t| 8087 code sequence\n",
    objbuf[0]);

   for (k = 1; k < objptr; ++k)
      printf("\t.byte\t0x%02.2x\n",objbuf[k]);

/* objout();                                       FOR NOW  */

}/* * * * * * * * * * * END OF fphand() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the  handler for  variable  software  interrupt *
  * opcodes.  It is included in this file because PC/IX im- *
  * plements its software floating-point emulation by means *
  * of interrupts.  Any interrupt in the range 0xd8 through *
  * 0xdf is an  NDP-emulation  interrupt,  and is specially *
  * handled by the assembler.                               *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
inhand(j)

   register int j;            /* Pointer to optab[] entry   */

{/* * * * * * * * * *  START OF inhand()  * * * * * * * * * */

   register int k;

   objini(j);

   FETCH(k);

   if (ISFLOP(k))
      {
      fphand(k);
      return;
      }

   printf("%s\t%d\n",optab[j].text,k);

   objout();

}/* * * * * * * * * * * END OF inhand() * * * * * * * * * * */


