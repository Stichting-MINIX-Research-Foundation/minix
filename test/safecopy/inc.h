
#define _SYSTEM		1

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

/* TEST_PAGE_SHIFT =
 * log2(CLICK_SIZE * TEST_PAGE_NUM) = CLICK_SHIFT + log2(TEST_PAGE_NUM)
 */
#define TEST_PAGE_NUM    8
#define TEST_PAGE_SHIFT 15

#define BUF_SIZE (TEST_PAGE_NUM * CLICK_SIZE)
#define BUF_START 100

#define FIFO_REQUESTOR "/usr/src/test/safecopy/1fifo"
#define FIFO_GRANTOR   "/usr/src/test/safecopy/2fifo"

#define FIFO_WAIT(fid) {                                                      \
	int a;                                                                \
	if(read(fid, &a, sizeof(a)) != sizeof(a))                             \
		panic("FIFO_WAIT failed");                  \
}
#define FIFO_NOTIFY(fid) {                                                    \
	int a = 1;                                                            \
	if(write(fid, &a, sizeof(a)) != sizeof(a))                            \
		panic("FIFO_NOTIFY failed");                \
}

#define DEBUG 0
#if DEBUG
#	define dprint printf
#else
#	define dprint (void)
#endif

