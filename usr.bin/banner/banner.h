/*	$NetBSD: banner.h,v 1.4 2000/10/04 19:33:16 mjl Exp $	*/

/*
 *	Various defines needed for code lifted from lpd.
 *	
 *	@(#)Copyright (c) 1995, Simon J. Gerraty.
 *      
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code 
 *      is granted subject to the following conditions.
 *      1/ that the above copyright notice and this notice 
 *      are preserved in all copies and that due credit be given 
 *      to the author.  
 *      2/ that any changes to this code are clearly commented 
 *      as such so that the author does not get blamed for bugs 
 *      other than his own.
 *      
 *      Please send copies of changes and bug-fixes to:
 *      sjg@zen.void.oz.au
 */

#define LINELEN		132
#define BACKGND		' '
#define HEIGHT	9		/* height of characters */
#define WIDTH	8		/* width of characters */

extern const char scnkey_lpd[][HEIGHT];
extern const char scnkey_def[][HEIGHT];
