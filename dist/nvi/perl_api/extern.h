/*	$NetBSD: extern.h,v 1.1.1.1 2008/05/18 14:31:34 aymeric Exp $ */

/* Do not edit: automatically built by build/distrib. */
#ifdef USE_SFIO
Sfdisc_t* sfdcnewnvi __P((SCR*));
#endif
int perl_end __P((GS *));
int perl_init __P((SCR *));
int perl_screen_end __P((SCR*));
int perl_setenv __P((SCR* sp, const char *name, const char *value));
int perl_ex_perl __P((SCR*, CHAR_T *, size_t, db_recno_t, db_recno_t));
int perl_ex_perldo __P((SCR*, CHAR_T *, size_t, db_recno_t, db_recno_t));
