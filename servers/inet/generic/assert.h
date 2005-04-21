/*
assert.h

Copyright 1995 Philip Homburg
*/
#ifndef INET_ASSERT_H
#define INET_ASSERT_H

#if !NDEBUG

void bad_assertion(char *file, int line, char *what);
void bad_compare(char *file, int line, int lhs, char *what, int rhs);

#define assert(x)	(!(x) ? bad_assertion(this_file, __LINE__, #x) \
								: (void) 0)
#define compare(a,t,b)	(!((a) t (b)) ? bad_compare(this_file, __LINE__, \
				(a), #a " " #t " " #b, (b)) : (void) 0)

#else /* NDEBUG */

#define assert(x)		0
#define compare(a,t,b)		0

#endif /* NDEBUG */

#endif /* INET_ASSERT_H */


/*
 * $PchId: assert.h,v 1.4 1995/11/21 06:45:27 philip Exp $
 */
