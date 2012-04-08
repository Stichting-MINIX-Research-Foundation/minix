
/* Code to test runtime linking functionality.
 * Load a shared object at runtime and verify that arguments passed to
 * and from a function that is dynamically looked up make sense.
 * This tests that (a) dynamic linking works at all (otherwise all the dl*
 * functions don't work) and (b) the dynamic loading functionality works
 * and (c) the PLT is sane and calling convention makes sense.
 *
 * We have to pass an absolute path to dlopen() for which we rely on 
 * the test run script.
 *
 * The module we load is in mod.c.
 */

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#define MAX_ERROR 2

#include "magic.h"
#include "common.c"

int main (int argc, char *argv[])
{
  void *dlhandle;
  long (*modf) (long, long *, long);
  long v, *cookie = NULL, cookie2 = 0;
  
  start(63);

  if(argc != 2) {
	fprintf(stderr, "Usage: %s <module>\n", argv[0]);
	exit(1);
  }

  if(!(dlhandle = dlopen(argv[1], RTLD_LAZY))) e(1);

  if(!(modf = dlsym(dlhandle, "modfunction"))) e(2);
  if(!(cookie = (long *) dlsym(dlhandle, "cookie"))) e(3);

  if(*cookie == MAGIC2) { fprintf(stderr, "cookie already set\n"); e(4); }
  if(cookie2 == MAGIC3) { fprintf(stderr, "cookie2 already set\n"); e(5); }

  v = modf(MAGIC4, &cookie2, MAGIC5);

  if(v != MAGIC1) { fprintf(stderr, "return value wrong\n"); e(9); }
  if(*cookie != MAGIC2) { fprintf(stderr, "cookie set wrongly\n"); e(6); }
  if(cookie2 != MAGIC3) { fprintf(stderr, "cookie2 set wrongly\n"); e(7); }

  dlclose(dlhandle);

  if(v != MAGIC1) { fprintf(stderr, "wrong return value.\n"); e(8); }

  quit();

  return(0);
}
