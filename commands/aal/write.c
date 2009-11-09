/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#include <system.h>

int
sys_write(fp, bufptr, nbytes)
	File *fp;
	char *bufptr;
	int nbytes;
{
	if (! fp) return 0;
	return write(fp->o_fd, bufptr, nbytes) == nbytes;
}
