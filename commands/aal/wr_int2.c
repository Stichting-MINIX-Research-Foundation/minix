/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include "object.h"
#include "wr_int2.h"
#include "wr_bytes.h"

void wr_int2(int fd, int i)
{
	char buf[2];

	put2(i, buf);
	wr_bytes(fd, buf, 2L);
}
