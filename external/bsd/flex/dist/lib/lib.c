/*	$NetBSD: lib.c,v 1.1.1.1 2013/04/06 14:05:53 christos Exp $	*/

/* Since building an empty library could cause problems, we provide a
 * function to go into the library. We could make this non-trivial by
 * moving something that flex treats as a library function into this
 * directory. */

void do_nothing(){ return;}

