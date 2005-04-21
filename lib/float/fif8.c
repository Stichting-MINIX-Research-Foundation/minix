/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/*
	MULTIPLY AND DISMEMBER PARTS (FIF 8)
*/

#include "FP_types.h"
#include "FP_shift.h"

void
fif8(p,x,y)
DOUBLE	x,y;
struct fif8_returns *p;
{

	EXTEND	e1,e2;

	extend(&y.d[0],&e1,sizeof(DOUBLE));
	extend(&x.d[0],&e2,sizeof(DOUBLE));
		/* do a multiply */
	mul_ext(&e1,&e2);
	e2 = e1;
	compact(&e2, &y.d[0], sizeof(DOUBLE));
	if (e1.exp < 0) {
		p->ipart.d[0] = 0;
		p->ipart.d[1] = 0;
		p->fpart = y;
		return;
	}
	if (e1.exp > 62 - DBL_M1LEFT) {
		p->ipart = y;
		p->fpart.d[0] = 0;
		p->fpart.d[1] = 0;
		return;
	}
	b64_sft(&e1.mantissa, 63 - e1.exp);
	b64_sft(&e1.mantissa, e1.exp - 63);	/* "loose" low order bits */
	compact(&e1, &(p->ipart.d[0]), sizeof(DOUBLE));
	extend(&(p->ipart.d[0]), &e2, sizeof(DOUBLE));
	extend(&y.d[0], &e1, sizeof(DOUBLE));
	sub_ext(&e1, &e2);
	compact(&e1, &(p->fpart.d[0]), sizeof(DOUBLE));
}
