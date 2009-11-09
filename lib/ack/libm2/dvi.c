/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	implementation of DIV and MOD
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
  Reason:	We cannot use DVI and RMI, because DVI rounds towards 0
		and Modula-2 requires truncation
*/

#include <em_abs.h>

int
dvi(j,i)
	int j,i;
{
	if (j == 0) TRP(EIDIVZ);
	if ((i < 0) != (j < 0)) {
		if (i < 0) i = -i;
		else j = -j;
		return -((i+j-1)/j);
	}
	else return i/j;
}

long
dvil(j,i)
	long j,i;
{
	if (j == 0) TRP(EIDIVZ);
	if ((i < 0) != (j < 0)) {
		if (i < 0) i = -i;
		else j = -j;
		return -((i+j-1)/j);
	}
	else return i/j;
}

int
rmi(j,i)
	int j,i;
{
	if (j == 0) TRP(EIDIVZ);
	if (i == 0) return 0;
	if ((i < 0) != (j < 0)) {
		if (i < 0) i = -i;
		else j = -j;
		return j*((i+j-1)/j)-i;
	}
	else return i%j;
}

long
rmil(j,i)
	long j,i;
{
	if (j == 0) TRP(EIDIVZ);
	if (i == 0) return 0L;
	if ((i < 0) != (j < 0)) {
		if (i < 0) i = -i;
		else j = -j;
		return j*((i+j-1)/j)-i;
	}
	else return i%j;
}
