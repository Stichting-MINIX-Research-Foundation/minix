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

extern struct file	**_extfl;
extern int		_extflc;
extern			_cls();
extern			_exit();

_hlt(ecode) int ecode; {
	int i;

	for (i = 0; i < _extflc; i++)
		if (_extfl[i] != (struct file *) 0)
			_cls(_extfl[i]);
	_exit(ecode);
}
