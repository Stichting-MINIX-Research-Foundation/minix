/*
 * dis386.h: header file for dis386.c.
 *
 * $Id: dis386.h,v 1.1 1997/10/20 12:00:00 cwr Exp cwr $
 *
 * Written by C W Rose.
 */

#ifndef EXTERN
#define EXTERN extern
#endif

/* Generally used variables */
struct locname {			/* local symbol table entry */
  char l_name[8];			/* symbol name */
  unsigned char l_sclass;		/* storage class */
  long l_value;				/* symbol value */
  struct locname *l_next;		/* pointer to next entry */
};
EXTERN struct locname *locsym[MAXSECT];	/* local symbol tables */

EXTERN FILE *aoutfp;			/* executable file pointer */
EXTERN FILE *corefp;			/* core file pointer */
EXTERN FILE *disfp;			/* disassembly file pointer */
EXTERN FILE *objfp;			/* object file pointer */
EXTERN FILE *symfp;			/* symbol file pointer */

/* executable file variables */
EXTERN struct exec a_hdrbuf; 		/* executable header structure */
EXTERN struct nlist *a_symtab;		/* executable symbol table */

/* .o file variables */
EXTERN struct outhead o_hdrbuf;		/* object file header data */
EXTERN struct outsect o_sectab[MAXSECT];/* object file section data */
EXTERN char *o_secnam[MAXSECT];		/* object file section names */
EXTERN struct outrelo *o_reltab;	/* object file relocation table */
EXTERN struct outname *o_symtab;	/* object file symbol table */
EXTERN char *o_strtab;			/* object file symbol names */

/* Generally used functions */
PUBLIC int dasm(unsigned long addr, unsigned long count); /* disassemble opcodes */

/*
 * EOF
 */

