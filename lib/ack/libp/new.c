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

extern		_sav();
extern		_rst();

#define assert(x)	/* nothing */
#define	UNDEF		0x8000

struct adm {
	struct adm	*next;
	int		size;
};

struct adm	*_lastp = 0;
struct adm	*_highp = 0;

_new(n,pp) int n; struct adm **pp; {
	struct adm *p,*q;
	int *ptmp;

	n = ((n+sizeof(*p)-1) / sizeof(*p)) * sizeof(*p);
	if ((p = _lastp) != 0)
		do {
			q = p->next;
			if (q->size >= n) {
				assert(q->size%sizeof(adm) == 0);
				if ((q->size -= n) == 0) {
					if (p == q)
						p = 0;
					else
						p->next = q->next;
					if (q == _highp)
						_highp = p;
				}
				_lastp = p;
				p = (struct adm *)((char *)q + q->size);
				q = (struct adm *)((char *)p + n);
				goto initialize;
			}
			p = q;
		} while (p != _lastp);
	/*no free block big enough*/
	_sav(&p);
	q = (struct adm *)((char *)p + n);
	_rst(&q);
initialize:
	*pp = p;
	ptmp = (int *)p;
	while (ptmp < (int *)q)
		*ptmp++ = UNDEF;
}
