/* This file contains a collection of miscellaneous procedures:
 *   panic	    abort MINIX due to a fatal error
 *   bad_assertion  for debugging
 *   bad_compare    for debugging
 */

#include "kernel.h"
#include "assert.h"
#include <unistd.h>
#include <minix/com.h>


/*===========================================================================*
 *                                   panic                                   *
 *===========================================================================*/
PUBLIC void panic(s,n)
_CONST char *s;
int n;
{
/* The system has run aground of a fatal error.  Terminate execution.
 * If the panic originated in MM or FS, the string will be empty and the
 * file system already syncked.  If the panic originates in the kernel, we are
 * kind of stuck.
 */
  static int panicking = 0;
  if (panicking ++)		/* prevent recursive panics */
  	return;

  if (s != NULL) {
	kprintf("\nKernel panic: %s", karg(s));
	if (n != NO_NUM) kprintf(" %d", n);
	kprintf("\n",NO_ARG);
  }
  prepare_shutdown(RBT_PANIC);
}


#if !NDEBUG
/*=========================================================================*
 *				bad_assertion				   *
 *=========================================================================*/
PUBLIC void bad_assertion(file, line, what)
char *file;
int line;
char *what;
{
  kprintf("panic at %s", karg(file));
  kprintf(" (line %d): ", line);
  kprintf("assertion \"%s\" failed.\n", karg(what));
  panic(NULL, NO_NUM);
}

/*=========================================================================*
 *				bad_compare				   *
 *=========================================================================*/
PUBLIC void bad_compare(file, line, lhs, what, rhs)
char *file;
int line;
int lhs;
char *what;
int rhs;
{
  kprintf("panic at %s", karg(file));
  kprintf(" (line %d): ", line);
  kprintf("compare (%d)", lhs);
  kprintf(" %s ", karg(what));
  kprintf("(%d) failed.\n", rhs);
  panic(NULL, NO_NUM);
}
#endif /* !NDEBUG */
