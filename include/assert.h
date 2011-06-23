/* The <assert.h> header contains a macro called "assert" that allows 
 * programmers to put assertions in the code.  These assertions can be verified
 * at run time.  If an assertion fails, an error message is printed and the
 * program aborts.
 * Assertion checking can be disabled by adding the statement
 *
 *	#define NDEBUG
 *
 * to the program before the 
 *
 *	#include <assert.h>
 *
 * statement.
 */

#undef assert

#ifndef _MINIX_ANSI_H
#include <minix/ansi.h>
#endif

#if TIME_ASSERTS
#define _ASSERT_EVALUATE(st) do { TIME_BLOCK(st); } while(0)
#else
#define _ASSERT_EVALUATE(st) do { st } while(0)
#endif

#ifdef NDEBUG
/* Debugging disabled -- do not evaluate assertions. */
#define assert(expr)  ((void) 0)
#else
/* Debugging enabled -- verify assertions at run time. */
#ifdef _ANSI
#define	__makestr(x)	# x
#define	__xstr(x)	__makestr(x)

_PROTOTYPE( void __bad_assertion, (const char *_mess) );
#define	assert(expr)	do { int _av;	\
			_ASSERT_EVALUATE(_av = !!(expr););	\
			if(!_av) {	\
				__bad_assertion("Assertion \"" #expr "\" failed, file " __xstr(__FILE__) ", line " __xstr(__LINE__) "\n"); \
		    } } while(0)
#else
#define assert(expr) ((void) ((expr) ? 0 : __assert( __FILE__,  __LINE__)))
#endif /* _ANSI */
#endif
