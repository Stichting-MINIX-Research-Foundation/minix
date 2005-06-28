/*
assert.h

Copyright 1995 Philip Homburg
*/
#ifndef INET_ASSERT_H
#define INET_ASSERT_H

#if !NDEBUG

void bad_assertion(char *file, int line, char *what) _NORETURN;
void bad_compare(char *file, int line, int lhs, char *what, int rhs) _NORETURN;

#define assert(x)	((void)(!(x) ? bad_assertion(this_file, __LINE__, \
			#x),0 : 0))
#define compare(a,t,b)	(!((a) t (b)) ? bad_compare(this_file, __LINE__, \
				(a), #a " " #t " " #b, (b)) : (void) 0)

#else /* NDEBUG */

#define assert(x)		0
#define compare(a,t,b)		0

#endif /* NDEBUG */

#endif /* INET_ASSERT_H */


/*
 * $PchId: assert.h,v 1.8 2002/03/18 21:50:32 philip Exp $
 */
