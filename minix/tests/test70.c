/* Test 70 - regression test for m_out vfs race condition.
 *
 * Regression test for vfs overwriting m_out fields by competing threads.
 * lseek() uses one of these fields too so this test performs concurrent
 * lseek()s to trigger this situation.
 *
 * The program consists of 2 processes, each seeking to different ranges in
 * a test file. The bug would return the wrong value in one of the messaeg
 * fields so the lseek() return value would be wrong sometimes.
 *
 * The first instance seeks from 0 to SEEKWINDOW, the other instance seeks
 * from SEEKWINDOW to SEEKWINDOW+SEEKWINDOW.
 */

#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "common.h"

#define SEEKWINDOW 1000

static int
doseeks(int seekbase)
{
	char template[30] = "tempfile.XXXXXXXX";
	int iteration, fd = mkstemp(template);
	int limit = seekbase + SEEKWINDOW;

	/* make a temporary file, unlink it so it's always gone
	 * afterwards, and make it the size we need.
	 */
	if(fd < 0) { perror("mkstemp"); e(2); return 1; }
	if(unlink(template) < 0) { perror("unlink"); e(3); return 1; }
	if(ftruncate(fd, limit) < 0) { perror("ftruncate"); e(4); return 1; }

	/* try lseek() lots of times with different arguments and make
	 * sure we get the right return value back, while this happens
	 * in a concurrent process too.
	 */
#define ITERATIONS 5000
	for(iteration = 0; iteration < ITERATIONS; iteration++) {
		int o;
		for(o = seekbase; o < limit; o++) {
			int r;
			if((r=lseek(fd, o, SEEK_SET)) != o) {
				if(r < 0) perror("lseek");
				fprintf(stderr, "%d/%d  %d != %d\n",
					iteration, ITERATIONS, r, o);
				e(5);
				return 1;
			}	
		}
	}

	return 0;
}

int
main()
{
  start(70);
  pid_t f;
  int result;

  if((f=fork()) < 0) { e(1); quit(); }

  if(f == 0) { exit(doseeks(0)); }

  if(doseeks(SEEKWINDOW)) { e(10); }

  if (waitpid(f, &result, 0) == -1) e(11);
  if (WEXITSTATUS(result) != 0) e(12);

  quit();

  return(-1);			/* impossible */
}

