/* Test 75 - getrusage functionality test.
 */

#include <sys/resource.h>
#include <sys/time.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
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
	if (gettimeofday(&start_time, NULL) == -1) {
		e(1);
		exit(1);
	}
	end_time = start_time;
	do {
		if ((++loop % 3000000000) == 0) {
			if (gettimeofday(&end_time, NULL) == -1) {
				e(1);
				exit(1);
			}
		}
	} while (start_time.tv_sec + 10 > end_time.tv_sec);
}

int
main(int argc, char *argv[])
{
	struct rusage r_usage1;
	struct rusage r_usage2;
	struct rusage r_usage3;
	pid_t child;
	int status = 0;
	start(75);
	if ((getrusage(RUSAGE_SELF + 1, &r_usage1) != -1 || errno != EINVAL) ||
		(getrusage(RUSAGE_CHILDREN - 1, &r_usage1) != -1 ||
		 errno != EINVAL) || (getrusage(RUSAGE_SELF, NULL) != -1 ||
		 errno != EFAULT)) {
		e(1);
		exit(1);
	}
	spin();
	if (getrusage(RUSAGE_SELF, &r_usage1) != 0) {
		e(1);
		exit(1);
	}
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_utime.tv_sec);
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_maxrss);
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_ixrss);
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_idrss);
	CHECK_NOT_ZERO_FIELD(r_usage1, ru_isrss);
	if (getrusage(RUSAGE_CHILDREN, &r_usage2) != 0) {
		e(1);
		exit(1);
	}
	CHECK_NOT_ZERO_FIELD(r_usage2, ru_maxrss);
	CHECK_NOT_ZERO_FIELD(r_usage2, ru_ixrss);
	CHECK_NOT_ZERO_FIELD(r_usage2, ru_idrss);
	CHECK_NOT_ZERO_FIELD(r_usage2, ru_isrss);
	CHECK_EQUAL_FIELD(r_usage1, r_usage2, ru_ixrss);
	CHECK_EQUAL_FIELD(r_usage1, r_usage2, ru_idrss);
	CHECK_EQUAL_FIELD(r_usage1, r_usage2, ru_isrss);
	if ((child = fork()) == 0) {
		/*
		 * We cannot do this part of the test in the parent, since
		 * start() calls system() which spawns a child process.
		 */
		if (getrusage(RUSAGE_CHILDREN, &r_usage1) != 0) {
			e(1);
			exit(1);
		}
		CHECK_ZERO_FIELD(r_usage1, ru_utime.tv_sec);
		CHECK_ZERO_FIELD(r_usage1, ru_utime.tv_usec);
		spin();
		exit(0);
	} else {
		if (child != waitpid(child, &status, 0)) {
			e(1);
			exit(1);
		}
		if (WEXITSTATUS(status) != 0) {
			e(1);
			exit(1);
		}
		if (getrusage(RUSAGE_CHILDREN, &r_usage3) != 0) {
			e(1);
			exit(1);
		}
		CHECK_NOT_ZERO_FIELD(r_usage3, ru_utime.tv_sec);
		CHECK_NOT_ZERO_FIELD(r_usage3, ru_maxrss);
		CHECK_NOT_ZERO_FIELD(r_usage3, ru_ixrss);
		CHECK_NOT_ZERO_FIELD(r_usage3, ru_idrss);
		CHECK_NOT_ZERO_FIELD(r_usage3, ru_isrss);
		CHECK_EQUAL_FIELD(r_usage1, r_usage3, ru_ixrss);
		CHECK_EQUAL_FIELD(r_usage1, r_usage3, ru_idrss);
		CHECK_EQUAL_FIELD(r_usage1, r_usage3, ru_isrss);
	}
	quit();

	return 0;
}
