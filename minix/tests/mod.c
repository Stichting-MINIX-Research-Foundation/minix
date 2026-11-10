
/* Code for module to be loaded by test63. */

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#include "magic.h"

long cookie = 0;

void exithandler(void);

long modfunction(long v1, long *argcookie, long v2) {
  if(v1 != MAGIC4 || v2 != MAGIC5) {
	fprintf(stderr, "wrong args to modfunction\n");
	exit(1);
  }
  *argcookie = MAGIC3;
  cookie = MAGIC2;
  return MAGIC1;
}

void exithandler(void) {
	/* OK */
}

/* atexit() from a dlopen()ed object: the handler must run when the
 * object is dlclose()d (it must not be left for exit(), which would call
 * into unmapped memory).
 */
static long *exitflag;

static void modexit(void) {
	if(exitflag != NULL) *exitflag = MAGIC6;
}

int modatexit(long *flag) {
	exitflag = flag;
	return atexit(modexit);
}
