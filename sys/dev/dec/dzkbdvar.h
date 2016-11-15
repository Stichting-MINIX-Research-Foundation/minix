/* $NetBSD: dzkbdvar.h,v 1.3 2003/01/06 21:05:37 matt Exp $ */

#ifndef _DEV_DEC_DZKBDVAR_H_
#define _DEV_DEC_DZKBDVAR_H_

struct dzkm_attach_args {
	int	daa_line;	/* Line to search */
	int	daa_flags;	/* Console etc...*/
#define	DZKBD_CONSOLE	1
};



/* These functions must be present for the keyboard/mouse to work */
int dzgetc(struct dz_linestate *);
void dzputc(struct dz_linestate *, int);
void dzsetlpr(struct dz_linestate *, int);

/* Exported functions */
int dzkbd_cnattach(struct dz_linestate *);

#endif /* _DEV_DEC_DZKBDVAR_H_ */
