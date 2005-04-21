/*	assert.h	*/

#ifndef NDEBUG	/* 8086 must do without training wheels. */
#define NDEBUG	(_WORD_SIZE == 2)
#endif

#if !NDEBUG

#define INIT_ASSERT	static char *assert_file= __FILE__;

void bad_assertion(char *file, int line, char *what);
void bad_compare(char *file, int line, int lhs, char *what, int rhs);

#define assert(x)	(!(x) ? bad_assertion(assert_file, __LINE__, #x) \
								: (void) 0)
#define compare(a,t,b)	(!((a) t (b)) ? bad_compare(assert_file, __LINE__, \
				(a), #a " " #t " " #b, (b)) : (void) 0)
#else /* NDEBUG */

#define INIT_ASSERT	/* nothing */

#define assert(x)	(void)0
#define compare(a,t,b)	(void)0

#endif /* NDEBUG */
