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

extern		_trp();

int _mdi(j,i) int j,i; {

        if (j <= 0)
                _trp(EMOD);
        i = i % j;
        if (i < 0)
                i += j;
        return(i);
}

long _mdil(j,i) long j,i; {

	if (j <= 0)
		_trp(EMOD);
	i = i % j;
	if (i < 0)
		i += j;
	return(i);
}

int _dvi(j, i) unsigned int j,i; {
	int neg = 0;

	if ((int)j < 0) {
		j = -(int)j; neg = 1;
	}
	if ((int)i < 0) {
		i = -(int)i; neg = !neg;
	}
	i = i / j;
	if (neg) return -(int)i;
	return i;
}

long _dvil(j, i) unsigned long j,i; {
	int neg = 0;

	if ((long)j < 0) {
		j = -(long)j; neg = 1;
	}
	if ((long)i < 0) {
		i = -(long)i; neg = !neg;
	}
	i = i / j;
	if (neg) return -(long)i;
	return i;
}
