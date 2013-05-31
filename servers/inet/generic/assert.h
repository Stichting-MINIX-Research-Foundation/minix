
#include <assert.h>

/*
assert.h

Copyright 1995 Philip Homburg
*/
#ifndef INET_ASSERT_H
#define INET_ASSERT_H

#if !NDEBUG

void bad_assertion(char *file, int line, char *what) _NORETURN;
void bad_compare(char *file, int line, int lhs, char *what, int rhs) _NORETURN;

#define compare(a,t,b)	assert((a) t (b))

#else /* NDEBUG */

#define compare(a,t,b)		0

#endif /* NDEBUG */

#endif /* INET_ASSERT_H */


/*
 * $PchId: assert.h,v 1.8 2002/03/18 21:50:32 philip Exp $
 */
