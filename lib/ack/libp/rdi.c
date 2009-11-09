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

#include	<pc_file.h>
#include	<pc_err.h>

extern		_trp();
extern		_rf();
extern		_incpt();

_skipsp(f) struct file *f; {
	while ((*f->ptr == ' ') || (*f->ptr == '\t'))
		_incpt(f);
}

int _getsig(f) struct file *f; {
	int sign;

	if ((sign = (*f->ptr == '-')) || *f->ptr == '+')
		_incpt(f);
	return(sign);
}

int _fstdig(f) struct file *f; {
	int ch;

	ch = *f->ptr - '0';
	if ((unsigned) ch > 9) {
		_trp(EDIGIT);
		ch = 0;
	}
	return(ch);
}

int _nxtdig(f) struct file *f; {
	int ch;

	_incpt(f);
	ch = *f->ptr - '0';
	if ((unsigned) ch > 9)
		return(-1);
	return(ch);
}

int _getint(f) struct file *f; {
	int is_signed,i,ch;

	is_signed = _getsig(f);
	ch = _fstdig(f);
	i = 0;
	do
		i = i*10 - ch;
	while ((ch = _nxtdig(f)) >= 0);
	return(is_signed ? i : -i);
}

int _rdi(f) struct file *f; {
	_rf(f);
	_skipsp(f);
	return(_getint(f));
}
