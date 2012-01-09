/*	$NetBSD: ne.h,v 1.3 2008/12/14 18:46:33 christos Exp $	*/

void ne2000_readmem(int, uint8_t *, size_t);
void ne2000_writemem(uint8_t *, int, size_t);
