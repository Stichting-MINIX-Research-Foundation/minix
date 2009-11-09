/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	MULTIPLY AND DISMEMBER PARTS (FIF 4)
*/

#include "FP_types.h"
#include "FP_shift.h"

void
fif4(p,x,y)
SINGLE	x,y;
struct fif4_returns *p;
{

	EXTEND	e1,e2;

	extend(&y,&e1,sizeof(SINGLE));
	extend(&x,&e2,sizeof(SINGLE));
		/* do a multiply */
	mul_ext(&e1,&e2);
	e2 = e1;
	compact(&e2,&y,sizeof(SINGLE));
	if (e1.exp < 0) {
		p->ipart = 0;
		p->fpart = y;
		return;
	}
	if (e1.exp > 30 - SGL_M1LEFT) {
		p->ipart = y;
		p->fpart = 0;
		return;
	}
	b64_sft(&e1.mantissa, 63 - e1.exp);
	b64_sft(&e1.mantissa, e1.exp - 63);	/* "loose" low order bits */
	compact(&e1,&(p->ipart),sizeof(SINGLE));
	extend(&(p->ipart), &e2, sizeof(SINGLE));
	extend(&y, &e1, sizeof(SINGLE));
	sub_ext(&e1, &e2);
	compact(&e1, &(p->fpart), sizeof(SINGLE));
}
