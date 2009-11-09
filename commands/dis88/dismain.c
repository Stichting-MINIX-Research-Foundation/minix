static char *sccsid =  "@(#) dismain.c, Ver. 2.1 created 00:00:00 87/09/01";

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  *  Copyright (C) 1987 G. M. Harding, all rights reserved  *
  *                                                         *
  * Permission to copy and  redistribute is hereby granted, *
  * provided full source code,  with all copyright notices, *
  * accompanies any redistribution.                         *
  *                                                         *
  * This file  contains  the source  code for the  machine- *
  * independent  portions of a disassembler  program to run *
  * in a Unix (System III) environment.  It expects, as its *
  * input, a file in standard a.out format, optionally con- *
  * taining symbol table information.  If a symbol table is *
  * present, it will be used in the disassembly; otherwise, *
  * all address references will be literal (absolute).      *
  *                                                         *
  * The disassembler  program was originally written for an *
  * Intel 8088 CPU.  However, all details of the actual CPU *
  * architecture are hidden in three machine-specific files *
  * named  distabs.c,  dishand.c,  and disfp.c  (the latter *
  * file is specific to the 8087 numeric co-processor). The *
  * code in this file is generic,  and should require mini- *
  * mal revision if a different CPU is to be targeted. If a *
  * different version of Unix is to be targeted, changes to *
  * this file may be necessary, and if a completely differ- *
  * ent OS is to be targeted, all bets are off.             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "dis.h"              /* Disassembler declarations  */

extern char *release;         /* Contains release string    */
static char *IFILE = NULL;    /* Points to input file name  */
static char *OFILE = NULL;    /* Points to output file name */
static char *PRG;             /* Name of invoking program   */
static unsigned long zcount;  /* Consecutive "0" byte count */
int objflg = 0;               /* Flag: output object bytes  */

#define unix 1
#define i8086 1
#define ibmpc 1

#if unix && i8086 && ibmpc    /* Set the CPU identifier     */
static int cpuid = 1;
#else
static int cpuid = 0;
#endif

_PROTOTYPE(static void usage, (char *s ));
_PROTOTYPE(static void fatal, (char *s, char *t ));
_PROTOTYPE(static void zdump, (unsigned long beg ));
_PROTOTYPE(static void prolog, (void));
_PROTOTYPE(static void distext, (void));
_PROTOTYPE(static void disdata, (void));
_PROTOTYPE(static void disbss, (void));

_PROTOTYPE(static char *invoker, (char *s));
_PROTOTYPE(static int objdump, (char *c));
_PROTOTYPE(static char *getlab, (int type));
_PROTOTYPE(static void prolog, (void));

 /* * * * * * * MISCELLANEOUS UTILITY FUNCTIONS * * * * * * */

static void
usage(s)
   register char *s;
{
   fprintf(stderr,"Usage: %s [-o] ifile [ofile]\n",s);
   exit(-1);
}

static void
fatal(s,t)
   register char *s, *t;
{
   fprintf(stderr,"\07%s: %s\n",s,t);
   exit(-1);
}

static void
zdump(beg)
   unsigned long beg;
{
   beg = PC - beg;
   if (beg > 1L)
      printf("\t.zerow\t%ld\n",(beg >> 1));
   if (beg & 1L)
      printf("\t.byte\t0\n");
}

static char *
invoker(s)
   register char *s;
{
   register int k;

   k = strlen(s);

   while (k--)
      if (s[k] == '/')
         {
         s += k;
         ++s;
         break;
         }

   return (s);
}

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This rather tricky routine supports the disdata() func- *
  * tion.  Its job is to output the code for a sequence  of *
  * data bytes whenever the object buffer is full,  or when *
  * a symbolic label is to be output. However, it must also *
  * keep track of  consecutive  zero words so that  lengthy *
  * stretches of null data can be  compressed by the use of *
  * an  appropriate  assembler  pseudo-op.  It does this by *
  * setting and testing a file-wide  flag which counts suc- *
  * cessive full buffers of null data. The function returns *
  * a logical  TRUE value if it outputs  anything,  logical *
  * FALSE otherwise.  (This enables disdata()  to determine *
  * whether to output a new  synthetic  label when there is *
  * no symbol table.)                                       *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int
objdump(c)

   register char *c;

{/* * * * * * * * * * START OF  objdump() * * * * * * * * * */

   register int k;
   int retval = 0;

   if (objptr == OBJMAX)
      {
      for (k = 0; k < OBJMAX; ++k)
         if (objbuf[k])
            break;
      if (k == OBJMAX)
         {
         zcount += k;
         objptr = 0;
         if (c == NULL)
            return (retval);
         }
      }

   if (zcount)
      {
      printf("\t.zerow\t%ld\n",(zcount >> 1));
      ++retval;
      zcount = 0L;
      }

   if (objptr)
      {
      printf("\t.byte\t");
      ++retval;
      }
   else
      return (retval);

   for (k = 0; k < objptr; ++k)
      {
      printf("0x%02.2x",objbuf[k]);
      if (k < (objptr - 1))
         putchar(',');
      else
         putchar('\n');
      }

   objptr = 0;

   return (retval);

}/* * * * * * * * * *  END OF  objdump()  * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  routine,  called  at the  beginning  of the input *
  * cycle for each object byte,  and before any interpreta- *
  * tion is  attempted,  searches  the symbol table for any *
  * symbolic  name with a value  corresponding  to the cur- *
  * rent PC and a type  corresponding  to the segment  type *
  * (i.e.,  text, data, or bss) specified by the function's *
  * argument. If any such name is found, a pointer to it is *
  * returned; otherwise, a NULL pointer is returned.        *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static char *
getlab(type)
register int type;
{/* * * * * * * * * *  START OF getlab()  * * * * * * * * * */

   register int k;
   static char b[32], c[10];

   if (symptr < 0)
      if ((type == N_TEXT)
       || ((type == N_DATA) && ( ! objptr ) && ( ! zcount )))
         {
         if (type == N_TEXT)
            sprintf(b,"T%05.5lx:",PC);
         else
            sprintf(b,"D%05.5lx:",PC);
         return (b);
         }
      else
         return (NULL);

   for (k = 0; k <= symptr; ++k)
      if ((symtab[k].n_value == PC)
       && ((symtab[k].n_sclass & N_SECT) == type))
         {
         sprintf(b,"%s:\n",getnam(k));
         if (objflg && (type != N_TEXT))
            sprintf(c,"| %05.5lx\n",PC);
         strcat(b,c);
         return (b);
         }

   return (NULL);

}/* * * * * * * * * * * END OF getlab() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This routine  performs a preliminary scan of the symbol *
  * table,  before disassembly begins, and outputs declara- *
  * tions of globals and constants.                         *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
prolog()

{/* * * * * * * * * *  START OF prolog()  * * * * * * * * * */

   register int j, flag;

   if (symptr < 0)
      return;

   for (j = flag = 0; j <= symptr; ++j)
      if ((symtab[j].n_sclass & N_CLASS) == C_EXT)
         if (((symtab[j].n_sclass & N_SECT) > N_UNDF)
          && ((symtab[j].n_sclass & N_SECT) < N_COMM))
            {
            char *c = getnam(j);
            printf("\t.globl\t%s",c);
            if (++flag == 1)
               {
               putchar('\t');
               if (strlen(c) < 8)
                  putchar('\t');
               printf("| Internal global\n");
               }
            else
               putchar('\n');
            }
         else
            if (symtab[j].n_value)
               {
               char *c = getnam(j);
               printf("\t.comm\t%s,0x%08.8lx",c,
                symtab[j].n_value);
               if (++flag == 1)
                  printf("\t| Internal global\n");
               else
                  putchar('\n');
               }

   if (flag)
      putchar('\n');

   for (j = flag = 0; j <= relptr; ++j)
      if (relo[j].r_symndx < S_BSS)
         {
         char *c = getnam(relo[j].r_symndx);
         ++flag;
         printf("\t.globl\t%s",c);
         putchar('\t');
         if (strlen(c) < 8)
            putchar('\t');
         printf("| Undef: %05.5lx\n",relo[j].r_vaddr);
         }

   if (flag)
      putchar('\n');

   for (j = flag = 0; j <= symptr; ++j)
      if ((symtab[j].n_sclass & N_SECT) == N_ABS)
         {
         char *c = getnam(j);
         printf("%s=0x%08.8lx",c,symtab[j].n_value);
         if (++flag == 1)
            {
            printf("\t\t");
            if (strlen(c) < 5)
               putchar('\t');
            printf("| Literal\n");
            }
         else
            putchar('\n');
         }

   if (flag)
      putchar('\n');

}/* * * * * * * * * * * END OF prolog() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This function is  responsible  for  disassembly  of the *
  * object file's text segment.                             *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
distext()

{/* * * * * * * * * * START OF  distext() * * * * * * * * * */

   char *c;
   register int j;
   register void (*f)();

   for (j = 0; j < (int)(HDR.a_hdrlen); ++j)
      getchar();

   printf("| %s, %s\n\n",PRG,release);

   printf("| @(");

   printf("#)\tDisassembly of %s",IFILE);

   if (symptr < 0)
      printf(" (no symbols)\n\n");
   else
      printf("\n\n");

   if (HDR.a_flags & A_EXEC)
      printf("| File is executable\n\n");

   if (HDR.a_flags & A_SEP)
      {
      printf("| File has split I/D space, and may have\n");
      printf("| extraneous instructions in text segment\n\n");
      }

   prolog();

   printf("\t.text\t\t\t| loc = %05.5lx, size = %05.5lx\n\n",
    PC,HDR.a_text);

   segflg = 0;

   for (PC = 0L; PC < HDR.a_text; ++PC)
      {
      j = getchar() & 0xff;
      if ((j == 0) && ((PC + 1L) == HDR.a_text))
         {
         ++PC;
         break;
         }
      if ((c = getlab(N_TEXT)) != NULL)
         printf("%s",c);
      f = optab[j].func;
      (*f)(j);
      }

}/* * * * * * * * * *  END OF  distext()  * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  function  handles the object file's data segment. *
  * There is no good way to disassemble a data segment, be- *
  * cause it is  impossible  to tell,  from the object code *
  * alone,  what each data byte refers to.  If it refers to *
  * an external symbol,  the reference can be resolved from *
  * the relocation table, if there is one.  However,  if it *
  * refers to a static symbol,  it cannot be  distinguished *
  * from numeric, character, or other pointer data. In some *
  * cases,  one might make a semi-educated  guess as to the *
  * nature of the data,  but such  guesses  are  inherently *
  * haphazard,  and they are  bound to be wrong a good por- *
  * tion of the time.  Consequently,  the data  segment  is *
  * disassembled  as a byte  stream,  which will satisfy no *
  * one but which, at least, will never mislead anyone.     *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void
disdata()

{/* * * * * * * * * * START OF  disdata() * * * * * * * * * */

   register char *c;
   register int j;
   unsigned long end;

   putchar('\n');

   if (HDR.a_flags & A_SEP)
      {
      PC = 0L;
      end = HDR.a_data;
      }
   else
      end = HDR.a_text + HDR.a_data;

   printf("\t.data\t\t\t| loc = %05.5lx, size = %05.5lx\n\n",
    PC,HDR.a_data);

   segflg = 0;

   for (objptr = 0, zcount = 0L; PC < end; ++PC)
      {
      if ((c = getlab(N_DATA)) != NULL)
         {
         objdump(c);
         printf("%s",c);
         }
      if (objptr >= OBJMAX)
         if (objdump(NULL) && (symptr < 0))
            printf("D%05.5lx:",PC);
      j = getchar() & 0xff;
      objbuf[objptr++] = j;
      }

   objdump("");

}/* * * * * * * * * *  END OF  disdata()  * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This  function  handles the object  file's bss segment. *
  * Disassembly of the bss segment is easy,  because every- *
  * thing in it is zero by definition.                      *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void disbss()

{/* * * * * * * * * *  START OF disbss()  * * * * * * * * * */

   register int j;
   register char *c;
   unsigned long beg, end;

   putchar('\n');

   if (HDR.a_flags & A_SEP)
      end = HDR.a_data + HDR.a_bss;
   else
      end = HDR.a_text + HDR.a_data + HDR.a_bss;

   printf("\t.bss\t\t\t| loc = %05.5lx, size = %05.5lx\n\n",
    PC,HDR.a_bss);

   segflg = 0;

   for (beg = PC; PC < end; ++PC)
      if ((c = getlab(N_BSS)) != NULL)
         {
         if (PC > beg)
            {
            zdump(beg);
            beg = PC;
            }
         printf("%s",c);
         }

   if (PC > beg)
      zdump(beg);

}/* * * * * * * * * * * END OF disbss() * * * * * * * * * * */

 /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
  *                                                         *
  * This is the program  entry  point.  The command line is *
  * searched for an input file name, which must be present. *
  * An optional output file name is also permitted; if none *
  * is found, standard output is the default.  One command- *
  * line option is available:  "-o",  which causes the pro- *
  * gram to include  object code in comments along with its *
  * mnemonic output.                                        *
  *                                                         *
  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
main(argc,argv)

   int argc;                  /* Command-line args from OS  */
   register char **argv;

{/* * * * * * * * * * * START OF main() * * * * * * * * * * */

   char a[1024];
   register int fd;
   long taboff, tabnum;
   long reloff, relnum;

   PRG = invoker(*argv);

   while (*++argv != NULL)    /* Process command-line args  */
      if (**argv == '-')
         switch (*++*argv)
            {
            case 'o' :
               if (*++*argv)
                  usage(PRG);
               else
                  ++objflg;
               break;
            default :
               usage(PRG);
            }
      else
         if (IFILE == NULL)
            IFILE = *argv;
         else if (OFILE == NULL)
            OFILE = *argv;
         else
            usage(PRG);

   if (IFILE == NULL)
      usage(PRG);
   else
      if ((fd = open(IFILE,0)) < 0)
         {
         sprintf(a,"can't access input file %s",IFILE);
         fatal(PRG,a);
         }

   if (OFILE != NULL)
      if (freopen(OFILE,"w",stdout) == NULL)
         {
         sprintf(a,"can't open output file %s",OFILE);
         fatal(PRG,a);
         }

   if ( ! cpuid )
      fprintf(stderr,"\07%s: warning: host/cpu clash\n",PRG);

   read(fd, (char *) &HDR,sizeof(struct exec));

   if (BADMAG(HDR))
      {
      sprintf(a,"input file %s not in object format",IFILE);
      fatal(PRG,a);
      }

   if (HDR.a_cpu != A_I8086)
      {
      sprintf(a,"%s is not an 8086/8088 object file",IFILE);
      fatal(PRG,a);
      }

   if (HDR.a_hdrlen <= A_MINHDR)
      HDR.a_trsize = HDR.a_drsize = 0L;
      HDR.a_tbase = HDR.a_dbase = 0L;
/*   AST emergency patch
      HDR.a_lnums = HDR.a_toffs = 0L;
*/

   reloff = HDR.a_text        /* Compute reloc data offset  */
          + HDR.a_data
          + (long)(HDR.a_hdrlen);

   relnum =
      (HDR.a_trsize + HDR.a_drsize) / sizeof(struct reloc);

   taboff = reloff            /* Compute name table offset  */
          + HDR.a_trsize
          + HDR.a_drsize;

   tabnum = HDR.a_syms / sizeof(struct nlist);

   if (relnum > MAXSYM)
      fatal(PRG,"reloc table overflow");

   if (tabnum > MAXSYM)
      fatal(PRG,"symbol table overflow");

   if (relnum)                            /* Get reloc data */
      if (lseek(fd,reloff,0) != reloff)
         fatal(PRG,"lseek error");
      else
         {
         for (relptr = 0; relptr < relnum; ++relptr)
            read(fd, (char *) &relo[relptr],sizeof(struct reloc));
         relptr--;
         }

   if (tabnum)                            /* Read in symtab */
      if (lseek(fd,taboff,0) != taboff)
         fatal(PRG,"lseek error");
      else
         {
         for (symptr = 0; symptr < tabnum; ++symptr)
            read(fd, (char *) &symtab[symptr],sizeof(struct nlist));
         symptr--;
         }
   else
      fprintf(stderr,"\07%s: warning: no symbols\n",PRG);

   close(fd);

   if (freopen(IFILE,"r",stdin) == NULL)
      {
      sprintf(a,"can't reopen input file %s",IFILE);
      fatal(PRG,a);
      }

   distext();

   disdata();

   disbss();

   exit(0);

}/* * * * * * * * * * *  END OF main()  * * * * * * * * * * */
