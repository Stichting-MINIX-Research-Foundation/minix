/*	$NetBSD: kern_ksyms_buf.c,v 1.4 2015/08/30 01:46:02 uebayasi Exp $	*/

#if defined(_KERNEL_OPT)
#include "opt_copy_symtab.h"
#endif

#define		SYMTAB_FILLER	"|This is the symbol table!"

#ifdef makeoptions_COPY_SYMTAB
#ifndef SYMTAB_SPACE
char		db_symtab[] = SYMTAB_FILLER;
#else
char		db_symtab[SYMTAB_SPACE] = SYMTAB_FILLER;
#endif
int		db_symtabsize = sizeof(db_symtab);
#endif
