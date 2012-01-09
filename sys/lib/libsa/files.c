/* $NetBSD: files.c,v 1.1 2002/03/15 13:23:34 simonb Exp $ */

/*
 *	files.c:
 *
 *	libsa file table.  separate from other global variables so that
 *	all of those don't need to be linked in just to use open, et al.
 */

#include "stand.h"

struct open_file files[SOPEN_MAX];
