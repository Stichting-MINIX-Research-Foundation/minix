/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

/********************************************************/
/*
	Type definitions for C Floating Point Package
	include file for floating point package
*/
/********************************************************/
/*
	THESE STRUCTURES ARE USED TO ADDRESS THE INDIVIDUAL
	PARTS OF THE FLOATING POINT NUMBER REPRESENTATIONS.

	THREE STRUCTURES ARE DEFINED:
		SINGLE:	single precision floating format
		DOUBLE:	double precision floating format
		EXTEND:	double precision extended format
*/
/********************************************************/

#ifndef __FPTYPES
#define __FPTYPES

typedef	struct	{
	unsigned long	h_32;	/* higher 32 bits of 64 */
	unsigned long	l_32;	/* lower  32 bits of 64 */
}	B64;

typedef	unsigned long	SINGLE;

typedef	struct	{
	unsigned long	d[2];
}	DOUBLE;

typedef	struct	{	/* expanded float format	*/
	short	sign;
	short	exp;
	B64	mantissa;
#define m1 mantissa.h_32
#define m2 mantissa.l_32
} EXTEND;

struct	fef4_returns {
	int	e;
	SINGLE	f;
};

struct	fef8_returns {
	int	e;
	DOUBLE	f;
};

struct fif4_returns {
	SINGLE ipart;
	SINGLE fpart;
};

struct fif8_returns {
	DOUBLE ipart;
	DOUBLE fpart;
};

#if __STDC__
#define _PROTOTYPE(function, params)	function params
#else
#define _PROTOTYPE(function, params)	function()
#endif
_PROTOTYPE( void add_ext, (EXTEND *e1, EXTEND *e2));
_PROTOTYPE( void mul_ext, (EXTEND *e1, EXTEND *e2));
_PROTOTYPE( void div_ext, (EXTEND *e1, EXTEND *e2));
_PROTOTYPE( void sub_ext, (EXTEND *e1, EXTEND *e2));
_PROTOTYPE( void sft_ext, (EXTEND *e1, EXTEND *e2));
_PROTOTYPE( void nrm_ext, (EXTEND *e1));
_PROTOTYPE( void zrf_ext, (EXTEND *e1));
_PROTOTYPE( void extend, (unsigned long *from, EXTEND *to, int size));
_PROTOTYPE( void compact, (EXTEND *from, unsigned long *to, int size));
_PROTOTYPE( void _fptrp, (int));
_PROTOTYPE( void adf4, (SINGLE s2, SINGLE s1));
_PROTOTYPE( void adf8, (DOUBLE s2, DOUBLE s1));
_PROTOTYPE( void sbf4, (SINGLE s2, SINGLE s1));
_PROTOTYPE( void sbf8, (DOUBLE s2, DOUBLE s1));
_PROTOTYPE( void dvf4, (SINGLE s2, SINGLE s1));
_PROTOTYPE( void dvf8, (DOUBLE s2, DOUBLE s1));
_PROTOTYPE( void mlf4, (SINGLE s2, SINGLE s1));
_PROTOTYPE( void mlf8, (DOUBLE s2, DOUBLE s1));
_PROTOTYPE( void ngf4, (SINGLE f));
_PROTOTYPE( void ngf8, (DOUBLE f));
_PROTOTYPE( void zrf4, (SINGLE *l));
_PROTOTYPE( void zrf8, (DOUBLE *z));
_PROTOTYPE( void cff4, (DOUBLE src));
_PROTOTYPE( void cff8, (SINGLE src));
_PROTOTYPE( void cif4, (int ss, long src));
_PROTOTYPE( void cif8, (int ss, long src));
_PROTOTYPE( void cuf4, (int ss, long src));
_PROTOTYPE( void cuf8, (int ss, long src));
_PROTOTYPE( long cfu, (int ds, int ss, DOUBLE src));
_PROTOTYPE( long cfi, (int ds, int ss, DOUBLE src));
_PROTOTYPE( int cmf4, (SINGLE s2, SINGLE s1));
_PROTOTYPE( int cmf8, (DOUBLE d1, DOUBLE d2));
_PROTOTYPE( void fef4, (struct fef4_returns *r, SINGLE s1));
_PROTOTYPE( void fef8, (struct fef8_returns *r, DOUBLE s1));
_PROTOTYPE( void fif4, (struct fif4_returns *p, SINGLE x, SINGLE y));
_PROTOTYPE( void fif8, (struct fif8_returns *p, DOUBLE x, DOUBLE y));

_PROTOTYPE( void b64_sft, (B64 *, int));
_PROTOTYPE( void b64_lsft, (B64 *));
_PROTOTYPE( void b64_rsft, (B64 *));
_PROTOTYPE( int b64_add, (B64 *, B64 *));
#endif
