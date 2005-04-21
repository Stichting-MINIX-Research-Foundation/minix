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

/* function argc:integer; extern; */
/* function argv(i:integer):string; extern; */
/* procedure argshift; extern; */
/* function environ(i:integer):string; extern; */

extern int	_pargc;
extern char	**_pargv;
extern char	***_penviron;

int argc() {
	return(_pargc);
}

char *argv(i) {
	if (i >= _pargc)
		return(0);
	return(_pargv[i]);
}

argshift() {

	if (_pargc > 1) {
		--_pargc;
		_pargv++;
	}
}

char *environ(i) {
	char **p; char *q;

	if (p = *_penviron)
		while (q = *p++)
			if (i-- < 0)
				return(q);
	return(0);
}
