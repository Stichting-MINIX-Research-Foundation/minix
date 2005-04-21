/*
 * remove.c - remove a file
 */
/* $Header$ */

#include	<stdio.h>

int _unlink(const char *path);

int
remove(const char *filename) {
	return _unlink(filename);
}
