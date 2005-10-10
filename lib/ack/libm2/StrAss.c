/*
  (c) copyright 1988 by the Vrije Universiteit, Amsterdam, The Netherlands.
  See the copyright notice in the ACK home directory, in the file "Copyright".
*/

/*
  Module:	assign string to character array, with possible 0-byte
		extension
  Author:	Ceriel J.H. Jacobs
  Version:	$Header$
*/
StringAssign(dstsiz, srcsiz, dstaddr, srcaddr)
	register char *dstaddr, *srcaddr;
{
	while (srcsiz > 0) {
		*dstaddr++ = *srcaddr++;
		srcsiz--;
		dstsiz--;
	}
	if (dstsiz > 0) {
		*dstaddr = 0;
	}
}
