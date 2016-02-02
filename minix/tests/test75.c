/* Test 75 - getrusage and wait4 test.
 */

#include <sys/resource.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"

#define CHECK_ZERO_FIELD(rusage, field)		\
	if (rusage.field != 0)			\
		em(1, #field " must be zero");

#define CHECK_NOT_ZERO_FIELD(rusage, field)	\
	if (rusage.field == 0)			\
		em(1, #field " can't be zero");

#define CHECK_EQUAL_FIELD(rusage1, rusage2, field)		  \
	if (rusage1.field != rusage2.field)			  \
		em(1, #field " of " #rusage1 " doesn't equal to " \
			#field " of " #rusage2);

static void
spin(void)
{
	struct timeval start_time;
	struct timeval end_time;
	unsigned int loop = 0;
	if (gettimeofday(&start_time, NULL) == -1) e(1);
	end_time = start_time;
	do {
		if ((++loop % 3000000000) == 0) {
			if (gettimeofday(&end_time, NULL) == -1) e(1);
		}
	} while (start_time.tv_sec + 10 > end_time.tv_sec);
}

/*
 * Test getrusage(2).
 */
static void
test75a(void)
{
	struct rusage r_usage1;
	struct rusage r_usage2;
	struct rusage r_usage3;
	pid_t child;
	int status = 0;

	if ((getrusage(RUSAGE_SELF + 1, &r_usage1) != -1 || errno != EINVAL) ||
	    (getrusage(RUSAGE_CHILDREN - 1, &r_usage1) != -1 ||
	     errno != EINVAL) || (getrusage(RUSAGE_SELF, NULL) != -1 ||
	     errno != EFAULT))
		e(1);

	spin();
	if (getrusage(RUSAGE_SELF, &r_usage1) != 0) e(1);
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_utime.tv_sec);
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_maxrss);
	if (getrusage(RUSAGE_CHILDREN, &r_usage2) != 0) e(1);

	if ((child = fork()) == 0) {
		/*
		 * We cannot do this part of the test in the parent, since
		 * start() calls system() which spawns a child process.
		 */
		if (getrusage(RUSAGE_CHILDREN, &r_usage1) != 0) e(1);
		CHECK_ZERO_FIELD(r_usage1, ru_utime.tv_sec);
		CHECK_ZERO_FIELD(r_usage1, ru_utime.tv_usec);
		spin();
		exit(errct);
	} else {
		if (child != waitpid(child, &status, 0)) e(1);
		if (WEXITSTATUS(status) != 0) e(1);
		if (getrusage(RUSAGE_CHILDREN, &r_usage3) != 0) e(1);
		CHECK_NOT_ZERO_FIELD(r_usage3, ru_utime.tv_sec);
	}
}

/*
 * Test the wait4 system call with good and bad rusage pointers, and with the
 * wait4 either being satisfied immediately or blocking until the child exits:
 * - mode 0: child has exited when parent calls wait4;
 * - mode 1: parent blocks waiting for child, using a bad rusage pointer;
 * - mode 2: parent blocks waiting for child, using a good rusage pointer.
 */
static void
sub75b(int mode, void * bad_ptr)
{
	struct rusage r_usage;
	pid_t pid;
	int status;

	pid = fork();

	switch (pid) {
	case -1:
		e(0);
		break;
	case 0:
		if (mode != 0)
			spin();
		exit(0);
	default:
		if (mode == 0)
			sleep(1);

		if (mode != 2) {
			/*
			 * Try with a bad pointer.  This call may fail only
			 * once the child has exited, but it must not clean up
			 * the child.
			 */
			if (wait4(-1, &status, 0, bad_ptr) != -1) e(0);
			if (errno != EFAULT) e(0);
		}

		r_usage.ru_nsignals = 1234; /* see if it's written at all */

		/* Wait for the actual process. */
		if (wait4(-1, &status, 0, &r_usage) != pid) e(0);
		if (!WIFEXITED(status)) e(0);
		if (WEXITSTATUS(status) != 0) e(0);

		if (r_usage.ru_nsignals != 0) e(0);

		/* Only check for actual time spent if the child spun. */
		if (mode != 0)
			CHECK_NOT_ZERO_FIELD(r_usage, ru_utime.tv_sec);
	}
}

/*
 * Test wait4().
 */
static void
test75b(void)
{
	void *ptr;

	if ((ptr = mmap(NULL, sizeof(struct rusage), PROT_READ,
	    MAP_PRIVATE | MAP_ANON, -1, 0)) == MAP_FAILED) e(0);
	if (munmap(ptr, sizeof(struct rusage)) != 0) e(0);
	/* "ptr" is now a known-bad pointer */

	sub75b(0, ptr);
	sub75b(1, ptr);
	sub75b(2, NULL);
}

int
main(int argc, char *argv[])
{

	start(75);

	test75a();
	test75b();

	quit();

	return 0;
}
