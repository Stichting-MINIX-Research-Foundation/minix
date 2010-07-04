/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* $Header$ */

#ifndef __RANLIB_H_INCLUDED
#define __RANLIB_H_INCLUDED

#ifndef SYMDEF
#	define SYMDEF	"__.SYMDEF"
#endif /* SYMDEF */

/*
 * Structure of the SYMDEF table of contents for an archive.
 * SYMDEF begins with a long giving the number of ranlib
 * structures that immediately follow, and then continues with a string
 * table consisting of a long giving the number of bytes of
 * strings that follow and then the strings themselves.
 */
struct ranlib {
	union {
	  char	*ran__ptr;	/* symbol name (in core) */
	  long	ran__off;	/* symbol name (in file) */
	}	ran_u;
#define ran_ptr ran_u.ran__ptr
#define ran_off ran_u.ran__off
	long	ran_pos;	/* library member is at this position */
};

#define SZ_RAN	8
#define SF_RAN	"44"

extern void wr_ranlib(int fd, struct ranlib ran[], long cnt);
#endif /* __RANLIB_H_INCLUDED */

