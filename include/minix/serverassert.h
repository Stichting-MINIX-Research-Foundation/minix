#ifndef __SERVERASSERT_H
#define __SERVERASSERT_H

/* This file contains functions and macros used for debugging within
 * system servers. Also see <assert.h> which is used in regular programs.
 */

#ifndef NDEBUG	/* 8086 must do without training wheels. */
#define NDEBUG	(_WORD_SIZE == 2)
#endif

#if !NDEBUG

#define INIT_SERVER_ASSERT	static char *server_assert_file= __FILE__;

void server_assert_failed(char *file, int line, char *what);
void server_compare_failed(char *file, int line, int lhs, char *what, int rhs);

#define server_assert(x)	(!(x) ? server_assert_failed( \
	server_assert_file, __LINE__, #x) : (void) 0)
#define server_compare(a,t,b)	(!((a) t (b)) ? server_compare_failed( \
	server_assert_file, __LINE__, (a), #a " " #t " " #b, (b)) : (void) 0)


#else /* NDEBUG */

#define INIT_SERVER_ASSERT	/* nothing */

#define server_assert(x)	(void) 0
#define server_compare(a,t,b)	(void) 0

#endif /* NDEBUG */


#endif /* __SERVERASSERT_H */

