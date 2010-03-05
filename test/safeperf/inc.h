#define _SYSTEM
#define _MINIX
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <minix/config.h>
#include <minix/com.h>
#include <minix/type.h>
#include <minix/const.h>
#include <minix/endpoint.h>
#include <minix/safecopies.h>
#include <minix/syslib.h>
#include <minix/sysutil.h>
#include <errno.h>

#define TEST_PAGE_NUM    50
#define BUF_SIZE (TEST_PAGE_NUM * CLICK_SIZE)

#define NR_TEST_ITERATIONS 100

#define FIFO_REQUESTOR "/usr/src/test/safeperf/1fifo"
#define FIFO_GRANTOR   "/usr/src/test/safeperf/2fifo"

#define FIFO_WAIT(fid) {                                                      \
	int a;                                                                \
	if(read(fid, &a, sizeof(a)) != sizeof(a))                             \
		panic( "FIFO_WAIT failed");                  \
}
#define FIFO_NOTIFY(fid) {                                                    \
	int a = 1;                                                            \
	if(write(fid, &a, sizeof(a)) != sizeof(a))                            \
		panic( "FIFO_NOTIFY failed");                \
}

#define REPORT_TEST(who, test_name, diff) {                                   \
	printf("%-9s: test %s took an average of %d.%dus per page\n",         \
	    who, test_name, (int)diff, ((int)(diff*1000))%1000);              \
}

#define DEBUG 0
#if DEBUG
#	define dprint printf
#else
#	define dprint (void)
#endif

