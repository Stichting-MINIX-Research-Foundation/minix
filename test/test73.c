/* Test 73 - VM secondary cache blackbox test.
 *
 * Blackbox test of the VM secondary cache in isolation, implemented
 * in testvm.c, started as a service by this test program.
 */

#include <minix/libminixfs.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/vm.h>
#include <minix/bdev.h>
#include <minix/paths.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioc_memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#include "testvm.h"

int max_error = 0;

#include "common.h"
#include "testcache.h"

int
main(int argc, char *argv[])
{
	char pipefn[30], cwd[400], cmdline[400];
	int pipefd;
	static struct info i;
	ssize_t r;
	int big = 0;

#define ITER 3
#define BLOCKS 200

	start(73);

	unlink(pipefn);

	/* 'big' as a substring indicates to testvm that it's ok to
	 * run a long test
	 */
	if(getenv(BIGVARNAME)) big = 1;

	if(big) strcpy(pipefn, "pipe_testvm_big");
	else strcpy(pipefn, "pipe_testvm");

	umask(0);
	if(mkfifo(pipefn, 0666) < 0) { e(1); exit(1); }
	if(!getcwd(cwd, sizeof(cwd))) { e(2); exit(1); }

	/* stop residual testvm service if any */
	snprintf(cmdline, sizeof(cmdline), "%s down testvm >/dev/null 2>&1",
		_PATH_SERVICE);
	if(system(cmdline) < 0) { e(9); exit(1); }

	/* start the testvm service */
	snprintf(cmdline, sizeof(cmdline),
		"%s up /%s/../testvm -script /etc/rs.single "
		"-args /%s/%s -config %s/../testvm.conf",
			_PATH_SERVICE, cwd, cwd, pipefn, cwd);
	if(system(cmdline) < 0) { e(10); exit(1); }

	printf("%s:%d\n", __FILE__, __LINE__);

	/* don't hang forever if the open or read block */
	alarm(big ? 6000 : 800);

	printf("%s:%d\n", __FILE__, __LINE__);

	if((pipefd=open(pipefn, O_RDONLY)) < 0) { e(3); exit(1); }
	printf("%s:%d\n", __FILE__, __LINE__);

	if((r=read(pipefd, &i, sizeof(i))) != sizeof(i)) {
		printf("read returned %d\n", r);
		e(12);
		exit(1);
	}
	printf("%s:%d\n", __FILE__, __LINE__);

	if(i.result != 0) { e(i.result); }
	printf("%s:%d\n", __FILE__, __LINE__);

	quit();

	return 0;
}

