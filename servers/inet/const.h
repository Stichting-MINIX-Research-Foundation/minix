/*
inet/const.h

Created:	Dec 30, 1991 by Philip Homburg

Copyright 1995 Philip Homburg
*/

#ifndef INET__CONST_H
#define INET__CONST_H

#ifndef DEBUG
#define DEBUG	0
#endif

#ifndef NDEBUG
#define NDEBUG	(CRAMPED)
#endif

#define CLOCK_GRAN	1	/* in HZ */

#if DEBUG
#define where()	printf("%s, %d: ", __FILE__, __LINE__)
#endif

#define NW_SUSPEND	SUSPEND
#define NW_WOULDBLOCK	EWOULDBLOCK
#define NW_OK		OK

#define BUF_S		512

#endif /* INET__CONST_H */

/*
 * $PchId: const.h,v 1.6 1995/11/21 06:54:39 philip Exp $
 */
