#include "utils.h"

/* This file contains two very related procedures for debugging purposes:
 *	server_assert_failed
 *	server_compare_failed
 * Also see <minix/serverassert.h>.
 */

#if !NDEBUG	
/*=========================================================================*
 *			server_assert_failed				   *
 *=========================================================================*/
PUBLIC void server_assert_failed(file, line, what)
char *file;
int line;
char *what;
{
  printf("server panic at %s(%d): assertion \"%s\" failed\n",
      file, line, what);
  server_panic(NULL, NULL, NO_NUM);
}

/*=========================================================================*
 *			server_compare_failed				   *
 *=========================================================================*/
PUBLIC void server_compare_failed(file, line, lhs, what, rhs)
char *file;
int line;
int lhs;
char *what;
int rhs;
{
  printf("server panic at %s(%d): compare (%d) %s (%d) failed\n",
	file, line, lhs, what, rhs);
  server_panic(NULL, NULL, NO_NUM);
}
#endif /* !NDEBUG */

