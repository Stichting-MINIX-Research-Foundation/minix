/* $Header$ */
/*
 * (c) copyright 1983 by the Vrije Universiteit, Amsterdam, The Netherlands.
 *
 *          This product is part of the Amsterdam Compiler Kit.
 *
 * Permission to use, sell, duplicate or disclose this software must be
 * obtained in writing. Requests for such permissions may be sent to
 *
 *      Dr. Andrew S. Tanenbaum
 *      Wiskundig Seminarium
 *      Vrije Universiteit
 *      Postbox 7161
 *      1007 MC Amsterdam
 *      The Netherlands
 *
 */

/* Author: J.W. Stevenson */

#include	<pc_err.h>

#define assert()	/* nothing */

/*
 * use circular list of free blocks from low to high addresses
 * _highp points to free block with highest address
 */
struct adm {
	struct adm	*next;
	int		size;
};

extern struct adm	*_lastp;
extern struct adm	*_highp;
extern			_trp();

static int merge(p1,p2) struct adm *p1,*p2; {
	struct adm *p;

	p = (struct adm *)((char *)p1 + p1->size);
	if (p > p2)
		_trp(EFREE);
	if (p != p2)
		return(0);
	p1->size += p2->size;
	p1->next = p2->next;
	return(1);
}

_dis(n,pp) int n; struct adm **pp; {
	struct adm *p1,*p2;

	/*
	 * NOTE: dispose only objects whose size is a multiple of sizeof(*pp).
	 *       this is always true for objects allocated by _new()
	 */
	n = ((n+sizeof(*p1)-1) / sizeof(*p1)) * sizeof(*p1);
	if (n == 0)
		return;
	if ((p1= *pp) == (struct adm *) 0)
		_trp(EFREE);
	p1->size = n;
	if ((p2 = _highp) == 0)  /*p1 is the only free block*/
		p1->next = p1;
	else {
		if (p2 > p1) {
			/*search for the preceding free block*/
			if (_lastp < p1)  /*reduce search*/
				p2 = _lastp;
			while (p2->next < p1)
				p2 = p2->next;
		}
		/* if p2 preceeds p1 in the circular list,
		 * try to merge them			*/
		p1->next = p2->next; p2->next = p1;
		if (p2 <= p1 && merge(p2,p1))
			p1 = p2;
		p2 = p1->next;
		/* p1 preceeds p2 in the circular list */
		if (p2 > p1) merge(p1,p2);
	}
	if (p1 >= p1->next)
		_highp = p1;
	_lastp = p1;
	*pp = (struct adm *) 0;
}
