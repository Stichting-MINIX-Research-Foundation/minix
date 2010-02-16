/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/* $Header$ */

# include "FP_types.h"

void
b64_sft(e1,n)
B64	*e1;
int	n;
{
	if (n > 0) {
		if (n > 63) {
			e1->l_32 = 0;
			e1->h_32 = 0;
			return;
		}
		if (n >= 32) {
			e1->l_32 = e1->h_32;
			e1->h_32 = 0;
			n -= 32;
		}
		if (n > 0) {
			e1->l_32 >>= n;
			if (e1->h_32 != 0) {
				e1->l_32 |= (e1->h_32 << (32 - n));
				e1->h_32 >>= n;
			}
		}
		return;
	}
	n = -n;
	if (n > 0) {
		if (n > 63) {
			e1->l_32 = 0;
			e1->h_32 = 0;
			return;
		}
		if (n >= 32) {
			e1->h_32 = e1->l_32;
			e1->l_32 = 0;
			n -= 32;
		}
		if (n > 0) {
			e1->h_32 <<= n;
			if (e1->l_32 != 0) {
				e1->h_32 |= (e1->l_32 >> (32 - n));
				e1->l_32 <<= n;
			}
		}
	}
}

void
b64_lsft(e1)
B64	*e1;
{
	/*	shift left 1 bit */
	e1->h_32 <<= 1;
	if (e1->l_32 & 0x80000000L) e1->h_32 |= 1;
	e1->l_32 <<= 1;
}

void
b64_rsft(e1)
B64	*e1;
{
	/*	shift right 1 bit */
	e1->l_32 >>= 1;
	if (e1->h_32 & 1) e1->l_32 |= 0x80000000L;
	e1->h_32 >>= 1;
}
