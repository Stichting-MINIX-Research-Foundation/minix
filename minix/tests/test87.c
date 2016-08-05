/* Tests for sysctl(2) and the MIB service - by D.C. van Moolenbroek */
/* This test needs to run as root: many sysctl(2) calls are privileged. */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <minix/sysctl.h>
#include <assert.h>

#define ITERATIONS 2

#include "common.h"

#define NONROOT_USER	"bin"		/* name of any unprivileged user */

#define NEXT_VER(n)	(((n) + 1 == 0) ? 1 : ((n) + 1)) /* node version + 1 */

static void *bad_ptr;			/* a pointer to unmapped memory */
static unsigned int nodes, objects;	/* stats for pre/post test check */

/*
 * Spawn a child process that drops privileges and then executes the given
 * procedure.  The returned PID value is of the dead, cleaned-up child, and
 * should be used only to check whether the child could store its own PID.
 */
static pid_t
test_nonroot(void (* proc)(void))
{
	struct passwd *pw;
	pid_t pid;
	int status;

	pid = fork();

	switch (pid) {
	case -1:
		e(0);
		break;
	case 0:
		errct = 0;

		if ((pw = getpwnam(NONROOT_USER)) == NULL) e(0);

		if (setuid(pw->pw_uid) != 0) e(0);

		proc();

		exit(errct);
	default:
		if (wait(&status) != pid) e(0);
		if (!WIFEXITED(status)) e(0);
		if (WEXITSTATUS(status) != 0) e(0);
	}

	return pid;
}

/*
 * Test basic operations from an unprivileged process.
 */
static void
sub87a(void)
{
	size_t oldlen;
	pid_t pid;
	bool b;
	int i, mib[4];

	pid = getpid();

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;

	/* Regular reads should succeed. */
	mib[2] = TEST_INT;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != 0x01020304) e(0);

	mib[2] = TEST_BOOL;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	/* Regular writes should fail. */
	b = true;
	if (sysctl(mib, 3, NULL, NULL, &b, sizeof(b)) != -1) e(0);
	if (errno != EPERM) e(0);

	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	/* Privileged reads and writes should fail. */
	mib[2] = TEST_PRIVATE;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EPERM) e(0);

	oldlen = sizeof(i);
	i = 1;
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EPERM) e(0);
	if (i != 1) e(0);

	if (sysctl(mib, 3, NULL, NULL, &i, sizeof(i)) != -1) e(0);
	if (errno != EPERM) e(0);

	mib[2] = TEST_SECRET;
	mib[3] = SECRET_VALUE;
	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 4, &i, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EPERM) e(0);
	if (i == 12345) e(0);

	mib[3]++;
	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 4, &i, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EPERM) e(0);

	/* Free-for-all writes should succeed. */
	mib[2] = TEST_ANYWRITE;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);

	i = pid;
	if (sysctl(mib, 3, NULL, NULL, &i, sizeof(i)) != 0) e(0);

	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != pid) e(0);
}

/*
 * Test the basic sysctl(2) interface.
 */
static void
test87a(void)
{
	char buf[32];
	size_t len, oldlen;
	pid_t pid;
	u_quad_t q;
	bool b, b2;
	int i, va[2], lastva = -1 /*gcc*/, mib[CTL_MAXNAME + 1];

	subtest = 0;

	mib[0] = INT_MAX; /* some root-level identifier that does not exist */
	for (i = 1; i <= CTL_MAXNAME; i++)
		mib[i] = i;

	/*
	 * We cannot test for invalid 'name' and 'oldlenp' pointers, because
	 * those may be accessed directly by the libc system call stub.  The
	 * NetBSD part of the stub even accesses name[0] without checking
	 * namelen first.
	 */
	if (sysctl(mib, 0, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, INT_MAX, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, UINT_MAX, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);
	for (i = 1; i <= CTL_MAXNAME; i++) {
		if (sysctl(mib, i, NULL, NULL, NULL, 0) != -1) e(i);
		if (errno != ENOENT) e(i);
	}
	if (sysctl(mib, i, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Test names that are too short, right, and too long. */
	mib[0] = CTL_MINIX;
	if (sysctl(mib, 1, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EISDIR) e(0);
	mib[1] = MINIX_TEST;
	if (sysctl(mib, 2, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EISDIR) e(0);
	mib[2] = TEST_INT;
	if (sysctl(mib, 3, NULL, NULL, NULL, 0) != 0) e(0);
	mib[3] = 0;
	if (sysctl(mib, 4, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	/* Do some tests with meta-identifiers (special keys). */
	mib[3] = CTL_QUERY;
	if (sysctl(mib, 4, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = CTL_QUERY;
	mib[3] = 0;
	if (sysctl(mib, 4, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	mib[2] = CTL_EOL; /* a known-invalid meta-identifier */
	if (sysctl(mib, 3, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EOPNOTSUPP) e(0);

	/* This case returns EINVAL now but might as well return EOPNOTSUPP. */
	mib[3] = 0;
	if (sysctl(mib, 4, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EOPNOTSUPP && errno != EINVAL) e(0);

	/* Make sure the given oldlen value is ignored when unused. */
	mib[2] = TEST_INT;
	oldlen = 0;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(int)) e(0);
	oldlen = 1;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(int)) e(0);
	oldlen = SSIZE_MAX;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(int)) e(0);
	oldlen = SIZE_MAX;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(int)) e(0);

	/* Test retrieval with the exact length. */
	oldlen = sizeof(va[0]);
	va[0] = va[1] = -1;
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (va[0] != 0x01020304) e(0);
	if (va[1] != -1) e(0);

	/* Test retrieval with a length that is too short. */
	for (i = 0; i < sizeof(va[0]); i++) {
		va[0] = -1;
		oldlen = i;
		if (sysctl(mib, 3, va, &oldlen, NULL, 0) != -1) e(0);
		if (errno != ENOMEM) e(0);
		if (oldlen != sizeof(va[0])) e(0);
		if (i == 0 && va[0] != -1) e(0);
		if (i > 0 && va[0] >= lastva) e(0);
		if (va[1] != -1) e(0);
		lastva = va[0];
	}

	/* Test retrieval with a length that is too long. */
	oldlen = sizeof(va[0]) + 1;
	va[0] = -1;
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (va[0] != 0x01020304) e(0);
	if (va[1] != -1) e(0);

	oldlen = SSIZE_MAX;
	va[0] = -1;
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (va[0] != 0x01020304) e(0);
	if (va[1] != -1) e(0);

	oldlen = SIZE_MAX;
	va[0] = -1;
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (va[0] != 0x01020304) e(0);
	if (va[1] != -1) e(0);

	/*
	 * Ensure that we cannot overwrite this read-only integer.  A write
	 * request must have both a pointer and a nonzero length, though.
	 */
	va[0] = 0x05060708;
	if (sysctl(mib, 3, NULL, NULL, NULL, 1) != 0) e(0);
	if (sysctl(mib, 3, NULL, NULL, va, 0) != 0) e(0);
	if (sysctl(mib, 3, NULL, NULL, va, sizeof(va[0])) != -1) e(0);
	if (errno != EPERM) e(0);

	oldlen = sizeof(va[0]);
	va[0] = -1;
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (va[0] != 0x01020304) e(0);
	if (va[1] != -1) e(0);

	/* Test retrieval into a bad pointer. */
	oldlen = sizeof(int);
	if (sysctl(mib, 3, bad_ptr, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EFAULT) e(0);

	/*
	 * Test reading and writing booleans.  Booleans may actually be an int,
	 * a char, or just one bit of a char.  As a result, the MIB service can
	 * not test properly for non-bool values being passed in bool fields,
	 * and we can not do effective testing on this either, because in both
	 * cases our efforts may simply be optimized away, and result in
	 * unexpected success.
	 */
	mib[2] = TEST_BOOL;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false && b != true) e(0);

	b = true;
	if (sysctl(mib, 3, NULL, &oldlen, &b, sizeof(b)) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);

	b = false;
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != true) e(0);

	b = false;
	b2 = false;
	oldlen = sizeof(b2);
	if (sysctl(mib, 3, &b2, &oldlen, &b, sizeof(b)) != 0) e(0);
	if (oldlen != sizeof(b2)) e(0);
	if (b != false) e(0);
	if (b2 != true) e(0);

	if (sysctl(mib, 3, NULL, NULL, &b, sizeof(b) + 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * The MIB service does not support value swaps.  If we pass in the
	 * same buffer for old and new data, we expect that the old data stays.
	 */
	b = true;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, &b, sizeof(b)) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	b = true;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	/* Test reading and writing a quad. */
	mib[2] = TEST_QUAD;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);

	q = 0x1234567890abcdefULL;
	if (sysctl(mib, 3, NULL, NULL, &q, sizeof(q)) != 0) e(0);

	q = 0ULL;
	oldlen = sizeof(q);
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != 0x1234567890abcdefULL) e(0);

	q = ~0ULL;
	if (sysctl(mib, 3, NULL, NULL, &q, sizeof(q)) != 0) e(0);

	/* Test writing with a bad pointer.  The value must stay. */
	if (sysctl(mib, 3, NULL, NULL, bad_ptr, sizeof(q)) != -1) e(0);
	if (errno != EFAULT) e(0);

	q = 0ULL;
	oldlen = sizeof(q);
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != ~0ULL) e(0);

	q = 0ULL;
	if (sysctl(mib, 3, NULL, NULL, &q, sizeof(q)) != 0) e(0);

	q = 1ULL;
	oldlen = sizeof(q);
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != 0ULL) e(0);

	/* Test reading and writing a string. */
	mib[2] = TEST_STRING;
	strlcpy(buf, "test", sizeof(buf));
	len = strlen(buf);
	if (sysctl(mib, 3, NULL, NULL, buf, len + 1) != 0) e(0);

	oldlen = sizeof(buf);
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);

	memset(buf, 0x07, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (strcmp(buf, "test")) e(0);
	if (oldlen != len + 1) e(0);
	if (buf[len + 1] != 0x07) e(0);

	strlcpy(buf, "abc123", sizeof(buf));
	oldlen = 2;
	if (sysctl(mib, 3, NULL, &oldlen, buf, strlen(buf) + 1) != 0) e(0);
	if (oldlen != len + 1) e(0);
	len = strlen(buf);

	memset(buf, 0x07, sizeof(buf));
	oldlen = len - 1;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (oldlen != len + 1) e(0);
	if (strncmp(buf, "abc12", len - 1)) e(0);
	if (buf[len - 1] != 0x07 || buf[len] != 0x07) e(0);

	memset(buf, 0x07, sizeof(buf));
	oldlen = len + 1;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);
	if (strcmp(buf, "abc123")) e(0);

	/*
	 * Now put in a shorter string, without null terminator.  The string
	 * must be accepted; the null terminator must be added automatically.
	 */
	strlcpy(buf, "foolproof", sizeof(buf));
	len = strlen("foo");
	if (sysctl(mib, 3, NULL, NULL, buf, len) != 0) e(0);

	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);

	memset(buf, 0x07, sizeof(buf));
	oldlen = len;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (oldlen != len + 1) e(0);
	if (strncmp(buf, "foo", len)) e(0);
	if (buf[len] != 0x07) e(0);

	memset(buf, 0x07, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);
	if (strcmp(buf, "foo")) e(0);
	if (buf[len + 1] != 0x07) e(0);

	/*
	 * Passing in more data after the string is fine, but whatever comes
	 * after the first null terminator is disregarded.
	 */
	strlcpy(buf, "barbapapa", sizeof(buf));
	len = strlen(buf);
	buf[3] = '\0';
	if (sysctl(mib, 3, NULL, NULL, buf, len + 1)) e(0);
	len = strlen(buf);

	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);

	memset(buf, 0x07, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);
	if (strcmp(buf, "bar")) e(0);
	if (buf[len + 1] != 0x07) e(0);

	/* Test the maximum string length. */
	strlcpy(buf, "0123456789abcdef", sizeof(buf));
	len = strlen(buf);
	if (sysctl(mib, 3, NULL, NULL, buf, len + 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, buf, len) != -1) e(0);
	if (errno != EINVAL) e(0);

	buf[--len] = '\0';
	if (sysctl(mib, 3, NULL, NULL, buf, len + 1) != 0) e(0);
	memset(buf, 0x07, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);
	if (strcmp(buf, "0123456789abcde")) e(0);
	if (buf[len + 1] != 0x07) e(0);

	/*
	 * Clearing out the field with zero-length data is not possible,
	 * because zero-length updates are disregarded at a higher level.
	 */
	if (sysctl(mib, 3, NULL, NULL, "", 0) != 0) e(0);
	memset(buf, 0x07, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len + 1) e(0);
	if (strcmp(buf, "0123456789abcde")) e(0);

	/* To clear the field, the null terminator is required. */
	if (sysctl(mib, 3, NULL, NULL, "", 1) != 0) e(0);
	memset(buf, 0x07, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 1) e(0);
	if (buf[0] != '\0') e(0);
	if (buf[1] != 0x07) e(0);

	/*
	 * Test reading and writing structures.  Structures are just blobs of
	 * data, with no special handling by default.  They can only be read
	 * and written all at once.
	 */
	mib[2] = TEST_STRUCT;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 12) e(0);
	len = oldlen;

	for (i = 0; i < len + 1; i++)
		buf[i] = i + 1;
	if (sysctl(mib, 3, NULL, NULL, buf, len - 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, buf, len + 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, buf, len) != 0) e(0);

	memset(buf, 0x7f, sizeof(buf));
	oldlen = len - 1;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (oldlen != len) e(0);
	for (i = 0; i < len - 1; i++)
		if (buf[i] != i + 1) e(0);
	if (buf[i] != 0x7f) e(0);

	memset(buf, 0x7f, sizeof(buf));
	oldlen = len + 1;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len) e(0);
	for (i = 0; i < len; i++)
		if (buf[i] != i + 1) e(0);
	if (buf[i] != 0x7f) e(0);

	memset(buf, 0x7f, sizeof(buf));
	oldlen = len;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	for (i = 0; i < len; i++)
		if (buf[i] != i + 1) e(0);
	if (buf[len] != 0x7f) e(0);

	/* Null characters are not treated in any special way. */
	for (i = 0; i < len; i++)
		buf[i] = !!i;
	if (sysctl(mib, 3, NULL, NULL, buf, len) != 0) e(0);

	oldlen = len;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len) e(0);
	for (i = 0; i < len; i++)
		if (buf[i] != !!i) e(0);
	if (buf[len] != 0x7f) e(0);

	memset(buf, 0, len);
	if (sysctl(mib, 3, NULL, NULL, buf, len) != 0) e(0);

	oldlen = len;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len) e(0);
	for (i = 0; i < len; i++)
		if (buf[i] != 0) e(0);
	if (buf[len] != 0x7f) e(0);

	/*
	 * Test private read and free-for-all write operations.  For starters,
	 * this test should run with superuser privileges, and thus should be
	 * able to read and write private fields.
	 */
	mib[2] = TEST_PRIVATE;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (va[0] != -5375) e(0);
	if (sysctl(mib, 3, NULL, NULL, va, sizeof(va[0])) != 0) e(0);

	mib[2] = TEST_SECRET;
	mib[3] = SECRET_VALUE;
	oldlen = sizeof(va[0]);
	if (sysctl(mib, 4, va, &oldlen, NULL, 0) != 0) e(0);
	if (va[0] != 12345) e(0);
	if (sysctl(mib, 4, NULL, NULL, va, sizeof(va[0])) != -1) e(0);
	if (errno != EPERM) e(0);

	mib[3]++;
	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 4, &i, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOENT) e(0);

	/* Use a child process to test operations without root privileges. */
	pid = test_nonroot(sub87a);

	/* The change made by the child should be visible to the parent. */
	mib[2] = TEST_ANYWRITE;
	va[0] = 0;
	oldlen = sizeof(va[0]);
	if (sysctl(mib, 3, va, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(va[0])) e(0);
	if (va[0] != pid) e(0);
}

/*
 * Test queries from an unprivileged process.
 */
static void
sub87b(void)
{
	struct sysctlnode scn[32];
	unsigned int count;
	size_t oldlen;
	int i, mib[4];

	/* Query minix.test and make sure we do not get privileged values. */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_QUERY;

	oldlen = sizeof(scn);
	if (sysctl(mib, 3, scn, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(scn[0])) e(0);
	count = oldlen / sizeof(scn[0]);
	if (count < 8) e(0);

	/*
	 * Do not bother doing the entire check again, but test enough to
	 * inspire confidence that only the right values are hidden.
	 */
	if (scn[0].sysctl_num != TEST_INT) e(0);
	if (SYSCTL_TYPE(scn[0].sysctl_flags) != CTLTYPE_INT) e(0);
	if ((SYSCTL_FLAGS(scn[0].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READONLY | CTLFLAG_IMMEDIATE | CTLFLAG_HEX)) e(0);
	if (SYSCTL_VERS(scn[0].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[0].sysctl_name, "int")) e(0);
	if (scn[0].sysctl_ver == 0) e(0);
	if (scn[0].sysctl_size != sizeof(int)) e(0);
	if (scn[0].sysctl_idata != 0x01020304) e(0);

	for (i = 0; i < count; i++)
		if (scn[i].sysctl_num == TEST_PRIVATE)
			break;
	if (i == count) e(0);
	if (SYSCTL_TYPE(scn[i].sysctl_flags) != CTLTYPE_INT) e(0);
	if ((SYSCTL_FLAGS(scn[i].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READWRITE | CTLFLAG_PRIVATE | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(scn[i].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[i].sysctl_name, "private")) e(0);
	if (scn[i].sysctl_size != sizeof(int)) e(0);
	if (scn[i].sysctl_idata != 0) e(0); /* private */

	for (i = 0; i < count; i++)
		if (scn[i].sysctl_num == TEST_SECRET)
			break;
	if (i == count) e(0);
	if (SYSCTL_TYPE(scn[i].sysctl_flags) != CTLTYPE_NODE) e(0);
	if ((SYSCTL_FLAGS(scn[i].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READONLY | CTLFLAG_PRIVATE)) e(0);
	if (SYSCTL_VERS(scn[i].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[i].sysctl_name, "secret")) e(0);
	if (scn[i].sysctl_ver == 0) e(0);
	if (scn[i].sysctl_size != sizeof(scn[0])) e(0);
	if (scn[i].sysctl_csize != 0) e(0); /* private */
	if (scn[i].sysctl_clen != 0) e(0); /* private */

	/* Make sure that a query on minix.test.secret fails. */
	mib[2] = TEST_SECRET;
	mib[3] = CTL_QUERY;
	if (sysctl(mib, 4, NULL, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EPERM) e(0);
}

/*
 * Test sysctl(2) queries.
 */
static void
test87b(void)
{
	struct sysctlnode scn[32];
	unsigned int count;
	size_t len, oldlen;
	u_quad_t q;
	bool b;
	int i, mib[4];

	subtest = 1;

	/* We should be able to query the root key. */
	mib[0] = CTL_QUERY;

	oldlen = 0;
	if (sysctl(mib, 1, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen <= sizeof(scn[0])) e(0);
	if (oldlen % sizeof(scn[0])) e(0);

	oldlen = sizeof(scn[0]);
	if (sysctl(mib, 1, scn, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOMEM);
	if (oldlen <= sizeof(scn[0])) e(0);
	if (oldlen % sizeof(scn[0])) e(0);

	/*
	 * We assume that the root node's first child is always CTL_KERN, which
	 * must be read-only and may have only the CTLFLAG_PERMANENT flag set.
	 */
	if (scn[0].sysctl_num != CTL_KERN) e(0);
	if (SYSCTL_TYPE(scn[0].sysctl_flags) != CTLTYPE_NODE) e(0);
	if ((SYSCTL_FLAGS(scn[0].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    CTLFLAG_READONLY) e(0);
	if (SYSCTL_VERS(scn[0].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[0].sysctl_name, "kern")) e(0);
	if (scn[0].sysctl_ver == 0) e(0);
	if (scn[0].sysctl_size != sizeof(scn[0])) e(0);
	if ((int)scn[0].sysctl_csize <= 0) e(0);
	if ((int)scn[0].sysctl_clen <= 0) e(0);
	if (scn[0].sysctl_csize < scn[0].sysctl_clen) e(0);

	/* Now do a more complete test on the minix.test subtree. */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;

	/*
	 * Initialize a few immediate fields to nonzero so that we can test
	 * that their values are returned as a result of the query.
	 */
	mib[2] = TEST_BOOL;
	b = true;
	if (sysctl(mib, 3, NULL, NULL, &b, sizeof(b)) != 0) e(0);

	mib[2] = TEST_QUAD;
	q = ~0;
	if (sysctl(mib, 3, NULL, NULL, &q, sizeof(q)) != 0) e(0);

	mib[2] = CTL_QUERY;

	oldlen = 1;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(scn[0])) e(0);
	if (oldlen >= sizeof(scn)) e(0);
	len = oldlen;
	count = len / sizeof(scn[0]);
	if (count < 8) e(0);

	memset(scn, 0x7e, sizeof(scn));
	if (sysctl(mib, 3, scn, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len) e(0);
	if (scn[count].sysctl_name[0] != 0x7e) e(0);

	/*
	 * Again, we rely on the MIB service returning entries in ascending
	 * order for at least the static nodes.  We do not make assumptions
	 * about whether dynamic nodes are merged in or (as is the case as of
	 * writing) returned after the static nodes.  At this point there
	 * should be no dynamic nodes here yet anyway.  We mostly ignore
	 * CTLFLAG_PERMANENT in order to facilitate running this test on a
	 * remotely mounted subtree.
	 */
	if (scn[0].sysctl_num != TEST_INT) e(0);
	if (SYSCTL_TYPE(scn[0].sysctl_flags) != CTLTYPE_INT) e(0);
	if ((SYSCTL_FLAGS(scn[0].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READONLY | CTLFLAG_IMMEDIATE | CTLFLAG_HEX)) e(0);
	if (SYSCTL_VERS(scn[0].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[0].sysctl_name, "int")) e(0);
	if (scn[0].sysctl_ver == 0) e(0);
	if (scn[0].sysctl_size != sizeof(int)) e(0);
	if (scn[0].sysctl_idata != 0x01020304) e(0);

	if (scn[1].sysctl_num != TEST_BOOL) e(0);
	if (SYSCTL_TYPE(scn[1].sysctl_flags) != CTLTYPE_BOOL) e(0);
	if ((SYSCTL_FLAGS(scn[1].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READWRITE | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(scn[1].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[1].sysctl_name, "bool")) e(0);
	if (scn[1].sysctl_ver == 0) e(0);
	if (scn[1].sysctl_size != sizeof(bool)) e(0);
	if (scn[1].sysctl_bdata != true) e(0);

	if (scn[2].sysctl_num != TEST_QUAD) e(0);
	if (SYSCTL_TYPE(scn[2].sysctl_flags) != CTLTYPE_QUAD) e(0);
	if ((SYSCTL_FLAGS(scn[2].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READWRITE | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(scn[2].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[2].sysctl_name, "quad")) e(0);
	if (scn[2].sysctl_ver == 0) e(0);
	if (scn[2].sysctl_size != sizeof(u_quad_t)) e(0);
	if (scn[2].sysctl_qdata != q) e(0);

	if (scn[3].sysctl_num != TEST_STRING) e(0);
	if (SYSCTL_TYPE(scn[3].sysctl_flags) != CTLTYPE_STRING) e(0);
	if ((SYSCTL_FLAGS(scn[3].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    CTLFLAG_READWRITE) e(0);
	if (SYSCTL_VERS(scn[3].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[3].sysctl_name, "string")) e(0);
	if (scn[3].sysctl_ver == 0) e(0);
	if (scn[3].sysctl_size != 16) e(0);

	if (scn[4].sysctl_num != TEST_STRUCT) e(0);
	if (SYSCTL_TYPE(scn[4].sysctl_flags) != CTLTYPE_STRUCT) e(0);
	if ((SYSCTL_FLAGS(scn[4].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    CTLFLAG_READWRITE) e(0);
	if (SYSCTL_VERS(scn[4].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[4].sysctl_name, "struct")) e(0);
	if (scn[4].sysctl_ver == 0) e(0);
	if (scn[4].sysctl_size != 12) e(0);

	if (scn[5].sysctl_num != TEST_PRIVATE) e(0);
	if (SYSCTL_TYPE(scn[5].sysctl_flags) != CTLTYPE_INT) e(0);
	if ((SYSCTL_FLAGS(scn[5].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READWRITE | CTLFLAG_PRIVATE | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(scn[5].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[5].sysctl_name, "private")) e(0);
	if (scn[5].sysctl_ver == 0) e(0);
	if (scn[5].sysctl_size != sizeof(int)) e(0);
	if (scn[5].sysctl_idata != -5375) e(0);

	if (scn[6].sysctl_num != TEST_ANYWRITE) e(0);
	if (SYSCTL_TYPE(scn[6].sysctl_flags) != CTLTYPE_INT) e(0);
	if ((SYSCTL_FLAGS(scn[6].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READWRITE | CTLFLAG_ANYWRITE | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(scn[6].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[6].sysctl_name, "anywrite")) e(0);
	if (scn[6].sysctl_ver == 0) e(0);
	if (scn[6].sysctl_size != sizeof(int)) e(0);

	i = (scn[7].sysctl_num == TEST_DYNAMIC) ? 8 : 7;

	if (scn[i].sysctl_num != TEST_SECRET) e(0);
	if (SYSCTL_TYPE(scn[i].sysctl_flags) != CTLTYPE_NODE) e(0);
	if ((SYSCTL_FLAGS(scn[i].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READONLY | CTLFLAG_PRIVATE)) e(0);
	if (SYSCTL_VERS(scn[i].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[i].sysctl_name, "secret")) e(0);
	if (scn[i].sysctl_ver == 0) e(0);
	if (scn[i].sysctl_size != sizeof(scn[0])) e(0);
	if (scn[i].sysctl_csize != 1) e(0);
	if (scn[i].sysctl_clen != 1) e(0);

	/*
	 * Now that we know how many entries there are in minix.test, also look
	 * at whether the right child length is returned in a query on its
	 * parent.  While doing that, see whether data structure versioning
	 * works as expected as well.  MINIX_TEST is hardcoded to zero so we
	 * expect it to be the first entry returned from a query.
	 */
	mib[1] = CTL_QUERY;

	memset(scn, 0, sizeof(scn));
	scn[1].sysctl_flags = SYSCTL_VERS_0;
	if (sysctl(mib, 2, NULL, &oldlen, &scn[1], sizeof(scn[1])) != -1) e(0);
	if (errno != EINVAL) e(0);
	scn[1].sysctl_flags = SYSCTL_VERS_1;
	if (sysctl(mib, 2, NULL, &oldlen, &scn[1], sizeof(scn[1]) - 1) != -1)
		e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 2, NULL, &oldlen, &scn[1], sizeof(scn[1]) + 1) != -1)
		e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 2, NULL, &oldlen, &scn[1], sizeof(scn[1])) != 0) e(0);
	if (oldlen == 0) e(0);
	if (oldlen % sizeof(scn[0])) e(0);

	oldlen = sizeof(scn[0]);
	scn[1].sysctl_flags = SYSCTL_VERS_0;
	if (sysctl(mib, 2, scn, &oldlen, &scn[1], sizeof(scn[1])) != -1) e(0);
	if (errno != EINVAL) e(0);
	oldlen = sizeof(scn[0]);
	scn[1].sysctl_flags = SYSCTL_VERS_1;
	if (sysctl(mib, 2, scn, &oldlen, &scn[1], sizeof(scn[1])) != 0 &&
	    errno != ENOMEM) e(0);
	if (oldlen == 0) e(0);
	if (oldlen % sizeof(scn[0])) e(0);

	if (scn[0].sysctl_num != MINIX_TEST) e(0);
	if (SYSCTL_TYPE(scn[0].sysctl_flags) != CTLTYPE_NODE) e(0);
	if ((SYSCTL_FLAGS(scn[0].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READWRITE | CTLFLAG_HIDDEN)) e(0);
	if (SYSCTL_VERS(scn[0].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[0].sysctl_name, "test")) e(0);
	if (scn[0].sysctl_ver == 0) e(0);
	if (scn[0].sysctl_size != sizeof(scn[0])) e(0);
	if ((int)scn[0].sysctl_clen != count) e(0);
	if (scn[0].sysctl_csize < scn[0].sysctl_clen) e(0);

	/*
	 * Test querying minix.test.secret, which should have exactly one node.
	 * At the same time, test bad pointers.
	 */
	mib[1] = MINIX_TEST;
	mib[2] = TEST_SECRET;
	mib[3] = CTL_QUERY;
	oldlen = sizeof(scn);
	if (sysctl(mib, 4, NULL, &oldlen, bad_ptr, sizeof(scn[0])) != -1) e(0);
	if (errno != EFAULT) e(0);

	oldlen = sizeof(scn[0]) * 2;
	if (sysctl(mib, 4, bad_ptr, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EFAULT) e(0);

	memset(scn, 0x7, sizeof(scn[0]) * 2);
	oldlen = sizeof(scn[0]) * 2;
	if (sysctl(mib, 4, scn, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(scn[0])) e(0);

	if (scn[0].sysctl_num != SECRET_VALUE) e(0);
	if (SYSCTL_TYPE(scn[0].sysctl_flags) != CTLTYPE_INT) e(0);
	if ((SYSCTL_FLAGS(scn[0].sysctl_flags) & ~CTLFLAG_PERMANENT) !=
	    (CTLFLAG_READONLY | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(scn[0].sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(scn[0].sysctl_name, "value")) e(0);
	if (scn[0].sysctl_ver == 0) e(0);
	if (scn[0].sysctl_size != sizeof(int)) e(0);
	if (scn[0].sysctl_idata != 12345) e(0);
	if (scn[1].sysctl_name[0] != 0x07) e(0);

	/* Use a child process to test queries without root privileges. */
	(void)test_nonroot(sub87b);

	/* Do some more path-related error code tests unrelated to the rest. */
	mib[1] = INT_MAX;
	mib[2] = CTL_QUERY;
	oldlen = sizeof(scn[0]);
	if (sysctl(mib, 3, &scn, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOENT) e(0);

	mib[1] = MINIX_TEST;
	mib[2] = TEST_INT;
	mib[3] = CTL_QUERY;
	oldlen = sizeof(scn[0]);
	if (sysctl(mib, 4, &scn, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOTDIR) e(0); /* ..and not EPERM (_INT is read-only) */

	mib[2] = TEST_BOOL;
	oldlen = sizeof(scn[0]);
	if (sysctl(mib, 4, &scn, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOTDIR) e(0); /* (_BOOL is read-write) */

	mib[2] = CTL_QUERY;
	oldlen = sizeof(scn[0]);
	if (sysctl(mib, 4, &scn, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);
}

/*
 * Attempt to create a node, using a given node template, identifier, and name
 * string.  If other_id is nonnegative, the creation is expected to fail due to
 * a collision with an existing node, which should have the ID other_id and the
 * name string in other_name.  Otherwise, the creation may succeed or fail, and
 * the caller must perform the appropriate checks.  On success, return the new
 * node identifier.  On failure, return -1, with errno set.
 */
static int
create_node(const int * path, unsigned int pathlen, struct sysctlnode * tmpscn,
	int id, const char * name, int other_id, const char * other_name)
{
	struct sysctlnode scn, oldscn;
	size_t oldlen;
	int r, mib[CTL_MAXNAME];

	assert(pathlen < CTL_MAXNAME);
	memcpy(mib, path, sizeof(mib[0]) * pathlen);
	mib[pathlen] = CTL_CREATE;

	memcpy(&scn, tmpscn, sizeof(scn));
	scn.sysctl_num = id;
	strlcpy(scn.sysctl_name, name, sizeof(scn.sysctl_name));
	oldlen = sizeof(oldscn);
	r = sysctl(mib, pathlen + 1, &oldscn, &oldlen, &scn, sizeof(scn));
	if (other_id >= 0) { /* conflict expected */
		if (oldlen != sizeof(oldscn)) e(0);
		if (r != -1) e(0);
		if (errno != EEXIST) e(0);
		if (oldscn.sysctl_num != other_id) e(0);
		if (strcmp(oldscn.sysctl_name, other_name)) e(0);
		return -1;
	} else {
		if (r != 0)
			return r;
		if (oldlen != sizeof(oldscn)) e(0);
		return oldscn.sysctl_num;
	}
}

/*
 * Destroy a node by identifier in the given named node directory.  Return 0 on
 * success.  Return -1 on failure, with errno set.
 */
static int
destroy_node(const int * path, unsigned int pathlen, int id)
{
	struct sysctlnode scn;
	int mib[CTL_MAXNAME];

	assert(pathlen < CTL_MAXNAME);
	memcpy(mib, path, sizeof(mib[0]) * pathlen);
	mib[pathlen] = CTL_DESTROY;

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = id;

	return sysctl(mib, pathlen + 1, NULL, NULL, &scn, sizeof(scn));
}

/*
 * Obtain the node data for one particular node in a node directory, by its
 * parent path and identifier.  Return 0 on success, with the node details
 * stored in 'scn', or -1 on failure.
 */
static int
query_node(const int * path, unsigned int pathlen, int id,
	struct sysctlnode * scn)
{
	struct sysctlnode scnset[32];
	size_t oldlen;
	unsigned int i;
	int r, mib[CTL_MAXNAME];

	assert(pathlen < CTL_MAXNAME);
	memcpy(mib, path, sizeof(mib[0]) * pathlen);
	mib[pathlen] = CTL_QUERY;

	oldlen = sizeof(scnset);
	if ((r = sysctl(mib, pathlen + 1, scnset, &oldlen, NULL, 0)) != 0 &&
	    errno != ENOMEM) e(0);
	if (oldlen == 0 || oldlen % sizeof(scnset[0])) e(0);
	for (i = 0; i < oldlen / sizeof(scnset[0]); i++)
		if (scnset[i].sysctl_num == id)
			break;
	if (i == oldlen / sizeof(scnset[0])) {
		if (r != 0) e(0); /* if this triggers, make scnset[] bigger! */
		return -1;
	}
	memcpy(scn, &scnset[i], sizeof(*scn));
	return 0;
}

/*
 * Test unprivileged node creation.
 */
static void
sub87c(void)
{
	struct sysctlnode scn;
	int mib[4];

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = TEST_DYNAMIC;
	mib[3] = CTL_CREATE;

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_IMMEDIATE |
	    CTLFLAG_READONLY | CTLTYPE_INT;
	scn.sysctl_size = sizeof(int);
	scn.sysctl_num = CTL_CREATE;
	scn.sysctl_idata = 777;
	strlcpy(scn.sysctl_name, "nonroot", sizeof(scn.sysctl_name));
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);

	mib[0] = CTL_CREATE;
	scn.sysctl_num = CTL_MINIX + 1;
	if (sysctl(mib, 1, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);
}

/*
 * Test sysctl(2) node creation.
 */
static void
test87c(void)
{
	static const uint32_t badflags[] = {
		SYSCTL_VERS_MASK, SYSCTL_TYPEMASK, CTLFLAG_PERMANENT,
		CTLFLAG_ROOT, CTLFLAG_ANYNUMBER, CTLFLAG_ALIAS, CTLFLAG_MMAP,
		CTLFLAG_OWNDESC
	};
	static const size_t badintsizes[] = {
		0, 1, sizeof(int) - 1, sizeof(int) + 1, sizeof(int) * 2,
		sizeof(int) * 4, SSIZE_MAX, SIZE_MAX
	};
	static const char *goodnames[] = {
		"_", "a", "test_name", "_____foo", "bar_0_1_2_3", "_2bornot2b",
		"abcdefghijklmnopqrstuvwxyz12345",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ67890",
	};
	static const char *badnames[] = {
		"", "0", "test.name", "2bornot2b", "@a", "b[", "c`d", "{",
		"\n", "\xff", "dir/name", "foo:bar",
		"abcdefghijklmnopqrstuvwxyz123456"
	};
	struct sysctlnode scn, pscn, oldscn, newscn, tmpscn, scnset[32];
	size_t oldlen, len;
	char buf[32], seen[5];
	bool b;
	u_quad_t q;
	int i, mib[CTL_MAXNAME], id[3];

	subtest = 2;

	/*
	 * On the first run of this test, this call with actually destroy a
	 * static node.  On subsequent runs, it may clean up the most likely
	 * leftover from a previous failed test.
	 */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	(void)destroy_node(mib, 2, TEST_DYNAMIC);

	/* Get child statistics about the parent node, for later comparison. */
	if (query_node(mib, 1, MINIX_TEST, &pscn) != 0) e(0);
	if (pscn.sysctl_clen == 0) e(0);
	if (pscn.sysctl_csize <= pscn.sysctl_clen) e(0);

	/* Start by testing if we can actually create a node at all. */
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_IMMEDIATE |
	    CTLFLAG_READONLY | CTLTYPE_INT;
	scn.sysctl_size = sizeof(int);
	scn.sysctl_num = TEST_DYNAMIC;
	scn.sysctl_idata = 777;
	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	oldlen = sizeof(newscn);
	if (sysctl(mib, 3, &newscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(newscn)) e(0);

	memcpy(&tmpscn, &scn, sizeof(scn));

	if (newscn.sysctl_num != TEST_DYNAMIC) e(0);
	if (SYSCTL_TYPE(newscn.sysctl_flags) != CTLTYPE_INT) e(0);
	if (SYSCTL_FLAGS(newscn.sysctl_flags) !=
	    (CTLFLAG_READONLY | CTLFLAG_IMMEDIATE)) e(0);
	if (SYSCTL_VERS(newscn.sysctl_flags) != SYSCTL_VERSION) e(0);
	if (strcmp(newscn.sysctl_name, "dynamic")) e(0);
	if (newscn.sysctl_ver == 0) e(0);
	if (newscn.sysctl_size != sizeof(int)) e(0);
	if (newscn.sysctl_idata != 777) e(0);

	/* Can we also read its value? */
	mib[2] = TEST_DYNAMIC;
	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != 777) e(0);

	/* For now, we assume that basic node destruction works. */
	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Try some variants of invalid new node data. */
	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn) - 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn) + 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, bad_ptr, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	/* Try with an invalid flags field. */
	scn.sysctl_flags =
	    (scn.sysctl_flags & ~SYSCTL_VERS_MASK) | SYSCTL_VERS_0;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags &= ~SYSCTL_TYPEMASK; /* type 0 does not exist */
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	for (i = 0; i < __arraycount(badflags); i++) {
		memcpy(&scn, &tmpscn, sizeof(scn));
		scn.sysctl_flags |= badflags[i];
		if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(i);
		if (errno != EINVAL) e(i);
	}

	/* Try successful creation (and destruction) once more. */
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Try a combination of most valid flags. */
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags &= ~CTLFLAG_READONLY; /* noop */
	scn.sysctl_flags |= CTLFLAG_READWRITE | CTLFLAG_ANYWRITE |
	    CTLFLAG_PRIVATE | CTLFLAG_HEX | CTLFLAG_HIDDEN | CTLFLAG_UNSIGNED;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Try invalid integer sizes.  We will get to other types in a bit. */
	for (i = 0; i < __arraycount(badintsizes); i++) {
		memcpy(&scn, &tmpscn, sizeof(scn));
		scn.sysctl_size = badintsizes[i];
		if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(i);
		if (errno != EINVAL) e(i);
	}

	/*
	 * For the value, we can supply IMMEDIATE, OWNDATA, or neither.  For
	 * IMMEDIATE, the integer value is taken directly from sysctl_idata.
	 * If OWNDATA is set, sysctl_data may be set, in which case the integer
	 * value is copied in from there.  If sysctl_data is NULL, the integer
	 * is initalized to zero.  If neither flag is set, sysctl_data must be
	 * NULL, since we do not support kernel addresses, and the integer will
	 * similarly be initialized to zero.  If both flags are set, the call
	 * fails with EINVAL.
	 */
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_OWNDATA; /* both flags are now set */
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags &= ~(CTLFLAG_IMMEDIATE | CTLFLAG_OWNDATA);
	scn.sysctl_data = &i;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = NULL;
	oldlen = sizeof(newscn);
	if (sysctl(mib, 3, &newscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(newscn)) e(0);
	if (newscn.sysctl_flags & CTLFLAG_IMMEDIATE) e(0);
	if (!(newscn.sysctl_flags & CTLFLAG_OWNDATA)) e(0); /* auto-set */
	if (newscn.sysctl_idata != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	scn.sysctl_data = NULL;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	i = -1;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	scn.sysctl_data = bad_ptr;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	i = 999;
	scn.sysctl_data = (void *)&i;
	oldlen = sizeof(newscn);
	if (sysctl(mib, 3, &newscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(newscn)) e(0);
	if ((newscn.sysctl_flags & (CTLFLAG_IMMEDIATE | CTLFLAG_OWNDATA)) !=
	    CTLFLAG_OWNDATA) e(0);
	if (newscn.sysctl_idata != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != 999) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* The user may never supply a function pointer or a parent. */
	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_func = (sysctlfn)test87c;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_parent = &scn;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Test some good and bad node names. */
	for (i = 0; i < __arraycount(goodnames); i++) {
		memcpy(&scn, &tmpscn, sizeof(scn));
		len = strlen(goodnames[i]);
		memcpy(scn.sysctl_name, goodnames[i], len);
		memset(&scn.sysctl_name[len], 0, SYSCTL_NAMELEN - len);
		if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(i);

		if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(i);
	}

	for (i = 0; i < __arraycount(badnames); i++) {
		memcpy(&scn, &tmpscn, sizeof(scn));
		len = strlen(badnames[i]);
		memcpy(scn.sysctl_name, badnames[i], len);
		memset(&scn.sysctl_name[len], 0, SYSCTL_NAMELEN - len);
		if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(i);
		if (errno != EINVAL) e(i);
	}

	/*
	 * Check for ID and name conflicts with existing nodes, starting with
	 * the basics.
	 */
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EEXIST) e(0);

	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EEXIST) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	if (oldscn.sysctl_ver == 0) e(0);
	oldscn.sysctl_ver = 0;
	if (memcmp(&oldscn, &tmpscn, sizeof(oldscn))) e(0);

	oldlen = sizeof(oldscn) - 1;
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EEXIST) e(0); /* ..we should not get ENOMEM now */
	if (oldlen != sizeof(oldscn)) e(0);

	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, bad_ptr, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EEXIST) e(0); /* ..we should not get EFAULT now */
	if (oldlen != 0) e(0); /* this is arguably an implementation detail */

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test ID and name conflicts against static nodes. */
	if (create_node(mib, 2, &tmpscn, TEST_INT, "dynamic", TEST_INT,
	    "int") != -1) e(0);
	if (create_node(mib, 2, &tmpscn, TEST_SECRET, "dynamic", TEST_SECRET,
	    "secret") != -1) e(0);
	if (create_node(mib, 2, &tmpscn, TEST_DYNAMIC, "quad", TEST_QUAD,
	    "quad") != -1) e(0);

	if (create_node(mib, 2, &tmpscn, TEST_DYNAMIC, "dynamic", -1,
	    NULL) != TEST_DYNAMIC) e(0);
	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test unique ID generation and LL back insertion. */
	if ((id[0] = create_node(mib, 2, &tmpscn, CTL_CREATE, "id0", -1,
	    NULL)) == -1) e(0);
	if ((id[1] = create_node(mib, 2, &tmpscn, CTL_CREATE, "id1", -1,
	    NULL)) == -1) e(0);
	if ((id[2] = create_node(mib, 2, &tmpscn, CTL_CREATE, "id2", -1,
	    NULL)) == -1) e(0);
	if (id[0] < CREATE_BASE || id[1] < CREATE_BASE || id[2] < CREATE_BASE)
		e(0);
	if (id[0] == id[1] || id[1] == id[2] || id[0] == id[2]) e(0);

	if (destroy_node(mib, 2, id[1]) != 0) e(0);

	/* Test ID and name conflicts against dynamic nodes. */
	if (create_node(mib, 2, &tmpscn, id[0], "id1", id[0],
	    "id0") != -1) e(0);
	if (create_node(mib, 2, &tmpscn, id[2], "id1", id[2],
	    "id2") != -1) e(0);
	if (create_node(mib, 2, &tmpscn, id[1], "id0", id[0],
	    "id0") != -1) e(0);
	if (create_node(mib, 2, &tmpscn, id[1], "id2", id[2],
	    "id2") != -1) e(0);

	/* Test name conflicts before and after LL insertion point. */
	if (create_node(mib, 2, &tmpscn, CTL_CREATE, "id0", id[0],
	    "id0") != -1) e(0);
	if (create_node(mib, 2, &tmpscn, CTL_CREATE, "id2", id[2],
	    "id2") != -1) e(0);

	/* Test recreation by ID and LL middle insertion. */
	if (create_node(mib, 2, &tmpscn, id[1], "id1", -1, NULL) == -1) e(0);
	if (destroy_node(mib, 2, id[1]) != 0) e(0);

	/* Test dynamic recreation and more LL middle insertion. */
	if ((id[1] = create_node(mib, 2, &tmpscn, CTL_CREATE, "id1", -1,
	    NULL)) == -1) e(0);
	if (id[1] < CREATE_BASE) e(0);
	if (id[1] == id[0] || id[1] == id[2]) e(0);

	/* Test LL front insertion. */
	if (create_node(mib, 2, &tmpscn, TEST_DYNAMIC, "dynamic", -1,
	    NULL) == -1) e(0);

	/* Ensure that all dynamic nodes show up in a query. */
	mib[2] = CTL_QUERY;
	oldlen = sizeof(scnset);
	memset(seen, 0, sizeof(seen));
	memset(scnset, 0, sizeof(scnset));
	if (sysctl(mib, 3, scnset, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen % sizeof(scn)) e(0);
	for (i = 0; (unsigned int)i < oldlen / sizeof(scn); i++) {
		if (scnset[i].sysctl_num == TEST_INT) {
			if (strcmp(scnset[i].sysctl_name, "int")) e(0);
			seen[0]++;
		} else if (scnset[i].sysctl_num == TEST_DYNAMIC) {
			if (strcmp(scnset[i].sysctl_name, "dynamic")) e(0);
			seen[1]++;
		} else if (scnset[i].sysctl_num == id[0]) {
			if (strcmp(scnset[i].sysctl_name, "id0")) e(0);
			seen[2]++;
		} else if (scnset[i].sysctl_num == id[1]) {
			if (strcmp(scnset[i].sysctl_name, "id1")) e(0);
			seen[3]++;
		} else if (scnset[i].sysctl_num == id[2]) {
			if (strcmp(scnset[i].sysctl_name, "id2")) e(0);
			seen[4]++;
		}
	}
	for (i = 0; i < 5; i++)
		if (seen[i] != 1) e(i);

	/* Compare the parent's statistics with those obtained earlier. */
	if (query_node(mib, 1, MINIX_TEST, &scn) != 0) e(0);
	if (scn.sysctl_clen != pscn.sysctl_clen + 4) e(0);
	if (scn.sysctl_csize != pscn.sysctl_csize + 4) e(0);

	/* Clean up. */
	if (destroy_node(mib, 2, id[0]) != 0) e(0);
	if (destroy_node(mib, 2, id[1]) != 0) e(0);
	if (destroy_node(mib, 2, id[2]) != 0) e(0);
	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Copy-out errors should not result in the node not being created. */
	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	oldlen = sizeof(newscn) - 1;
	if (sysctl(mib, 3, &newscn, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (oldlen != sizeof(newscn)) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	oldlen = sizeof(newscn);
	if (sysctl(mib, 3, bad_ptr, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	oldlen = sizeof(newscn) + 1;
	if (sysctl(mib, 3, &newscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(newscn)) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * Now that we are done with the integer template, try other data
	 * types, starting with booleans.  A big part of these tests is that
	 * the creation results in a usable node, regardless of the way its
	 * contents were initialized.
	 */
	tmpscn.sysctl_flags =
	    SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_BOOL;
	tmpscn.sysctl_size = sizeof(b);
	tmpscn.sysctl_data = NULL;

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	b = true;
	if (sysctl(mib, 3, NULL, NULL, &b, sizeof(b)) != 0) e(0);

	oldlen = sizeof(b);
	b = false;
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != true) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_IMMEDIATE;
	scn.sysctl_bdata = true;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != true) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	scn.sysctl_bdata = false;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_data = &b;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	scn.sysctl_size++;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_size--;
	scn.sysctl_data = bad_ptr;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	b = true;
	scn.sysctl_data = &b;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != true) e(0);

	b = false;
	oldlen = sizeof(b);
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != true) e(0);

	b = false;
	if (sysctl(mib, 3, NULL, NULL, &b, sizeof(b)) != 0) e(0);

	oldlen = sizeof(b);
	b = true;
	if (sysctl(mib, 3, &b, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(b)) e(0);
	if (b != false) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test quads next. */
	tmpscn.sysctl_flags =
	    SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_QUAD;
	tmpscn.sysctl_size = sizeof(q);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(q);
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != 0) e(0);

	q = ~0ULL;
	if (sysctl(mib, 3, NULL, NULL, &q, sizeof(q)) != 0) e(0);

	oldlen = sizeof(q);
	q = 0;
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != ~0ULL) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_IMMEDIATE;
	scn.sysctl_qdata = 1ULL << 48;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(q);
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != (1ULL << 48)) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_data = &q;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	scn.sysctl_size <<= 1;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_size >>= 1;
	scn.sysctl_data = bad_ptr;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	q = 123ULL << 31;
	scn.sysctl_data = &q;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(q);
	if (sysctl(mib, 3, &q, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(q)) e(0);
	if (q != (123ULL << 31)) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test strings. */
	tmpscn.sysctl_flags =
	    SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_STRING;
	tmpscn.sysctl_size = 7;

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_data = buf;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = NULL;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 1) e(0);
	if (buf[0] != '\0') e(0);
	if (buf[1] != 0x7f) e(0);

	if (sysctl(mib, 3, NULL, NULL, "woobie!", 8) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, "woobie!", 7) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, "woobie", 7) != 0) e(0);

	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 7) e(0);
	if (strcmp(buf, "woobie")) e(0);
	if (buf[7] != 0x7f) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_IMMEDIATE;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_size = 0;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = buf;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_size = (size_t)SSIZE_MAX + 1;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	scn.sysctl_data = bad_ptr;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	scn.sysctl_data = "abc123?";
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = "abc123";
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 7) e(0);
	if (strcmp(buf, "abc123")) e(0);
	if (buf[7] != 0x7f) e(0);

	if (sysctl(mib, 3, NULL, NULL, "", 1) != 0) e(0);

	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 1) e(0);
	if (buf[0] != '\0') e(0);
	if (buf[1] != 0x7f) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	scn.sysctl_data = "";
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 1) e(0);
	if (buf[0] != '\0') e(0);
	if (buf[7] != 0x7f) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * For strings, a zero node size means that the string length
	 * determines the buffer size.
	 */
	mib[2] = CTL_CREATE;
	scn.sysctl_size = 0;
	scn.sysctl_data = NULL;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = bad_ptr;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	scn.sysctl_data = "This is a string initializer."; /* size 29+1 */
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != strlen(scn.sysctl_data) + 1) e(0);
	if (buf[oldlen - 1] != '\0') e(0);
	if (buf[oldlen] != 0x7f) e(0);

	if (query_node(mib, 2, TEST_DYNAMIC, &newscn) != 0) e(0);
	if (newscn.sysctl_size != strlen(scn.sysctl_data) + 1) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test structs. */
	tmpscn.sysctl_flags =
	    SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_STRUCT;
	tmpscn.sysctl_size = 21;

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_data = buf;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = NULL;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 21) e(0);
	for (i = 0; i < 21; i++)
		if (buf[i] != 0) e(i);
	if (buf[i] != 0x7f) e(0);

	memset(buf, 'x', 32);
	if (sysctl(mib, 3, NULL, NULL, buf, 20) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, buf, 22) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, buf, 21) != 0) e(0);

	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 21) e(0);
	for (i = 0; i < 21; i++)
		if (buf[i] != 'x') e(i);
	if (buf[i] != 0x7f) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_IMMEDIATE;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_size = 0;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_data = buf;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_size = (size_t)SSIZE_MAX + 1;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	scn.sysctl_data = bad_ptr;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	for (i = 0; i < sizeof(buf); i++)
		buf[i] = i;
	scn.sysctl_data = buf;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(buf, 0x7f, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 21) e(0);
	for (i = 0; i < 21; i++)
		if (buf[i] != i) e(i);
	if (buf[i] != 0x7f) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Finally, test node-type nodes. */
	tmpscn.sysctl_flags =
	    SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_NODE;
	tmpscn.sysctl_size = 0;

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	scn.sysctl_flags |= CTLFLAG_IMMEDIATE;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_IMMEDIATE;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_size = sizeof(scn);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags &= ~CTLFLAG_IMMEDIATE;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags |= CTLFLAG_OWNDATA;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_csize = 8;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_clen = 1;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_child = &scn;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_parent = &scn;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_func = (sysctlfn)test87c;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (query_node(mib, 2, TEST_DYNAMIC, &scn) != 0) e(0);
	if (scn.sysctl_csize != 0) e(0);
	if (scn.sysctl_clen != 0) e(0);

	mib[2] = TEST_DYNAMIC;

	for (i = 3; i < CTL_MAXNAME; i++) {
		memcpy(&scn, &tmpscn, sizeof(scn));
		if (i % 2)
			scn.sysctl_num = i - 3;
		else
			scn.sysctl_num = CTL_CREATE;
		/*
		 * Test both names with different length (depthN vs depthNN)
		 * and cross-directory name duplicates (depth7.depth7).
		 */
		snprintf(scn.sysctl_name, sizeof(scn.sysctl_name), "depth%u",
		    7 + i / 2);
		mib[i] = CTL_CREATE;

		oldlen = sizeof(newscn);
		if (sysctl(mib, i + 1, &newscn, &oldlen, &scn,
		    sizeof(scn)) != 0) e(0);
		mib[i] = newscn.sysctl_num;
	}

	id[0] = mib[i - 1];
	mib[i - 1] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_READONLY |
	    CTLFLAG_OWNDATA | CTLTYPE_STRING;
	scn.sysctl_num = id[0] + 1;
	scn.sysctl_data = "bar";
	scn.sysctl_size = strlen(scn.sysctl_data) + 1;
	strlcpy(scn.sysctl_name, "foo", sizeof(scn.sysctl_name));
	if (sysctl(mib, i, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);
	mib[i - 1] = id[0] + 1;

	oldlen = sizeof(buf);
	if (sysctl(mib, i, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != strlen(scn.sysctl_data) + 1) e(0);
	if (strcmp(buf, scn.sysctl_data)) e(0);

	if (query_node(mib, i - 2, mib[i - 2], &scn) != 0) e(0);
	if (scn.sysctl_csize != 2) e(0);
	if (scn.sysctl_clen != 2) e(0);

	if (query_node(mib, 2, TEST_DYNAMIC, &scn) != 0) e(0);
	if (scn.sysctl_csize != 1) e(0);
	if (scn.sysctl_clen != 1) e(0);

	if (destroy_node(mib, i - 1, mib[i - 1]) != 0) e(0);
	mib[i - 1]--;

	for (i--; i > 2; i--)
		if (destroy_node(mib, i, mib[i]) != 0) e(0);

	if (query_node(mib, 2, TEST_DYNAMIC, &scn) != 0) e(0);
	if (scn.sysctl_csize != 0) e(0);
	if (scn.sysctl_clen != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * Finally, ensure that unprivileged processes cannot create nodes,
	 * even in the most friendly place possible.
	 */
	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags |= CTLFLAG_ANYWRITE;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	(void)test_nonroot(sub87c);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * Now that we are done, compare the parent's statistics with those
	 * obtained earlier once more.  There must be no differences.
	 */
	if (query_node(mib, 1, MINIX_TEST, &scn) != 0) e(0);
	if (scn.sysctl_clen != pscn.sysctl_clen) e(0);
	if (scn.sysctl_csize != pscn.sysctl_csize) e(0);

	/* Do some more path-related error code tests unrelated to the rest. */
	memcpy(&scn, &tmpscn, sizeof(scn));
	mib[1] = INT_MAX;
	if (create_node(mib, 2, &scn, TEST_DYNAMIC, "d", -1, NULL) != -1) e(0);
	if (errno != ENOENT) e(0);

	mib[1] = MINIX_TEST;
	mib[2] = TEST_INT;
	if (create_node(mib, 3, &scn, TEST_DYNAMIC, "d", -1, NULL) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = TEST_BOOL;
	if (create_node(mib, 3, &scn, TEST_DYNAMIC, "d", -1, NULL) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = CTL_CREATE;
	if (create_node(mib, 3, &scn, TEST_DYNAMIC, "d", -1, NULL) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Finally, try to create a node in a read-only directory node. */
	mib[2] = TEST_SECRET;
	if (create_node(mib, 3, &scn, -1, "d", -1, NULL) != -1) e(0);
	if (errno != EPERM) e(0);
}

/*
 * Test unprivileged node destruction.
 */
static void
sub87d(void)
{
	struct sysctlnode scn;
	int mib[3];

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_DESTROY;

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = TEST_ANYWRITE;

	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);

	mib[0] = CTL_DESTROY;
	scn.sysctl_num = CTL_MINIX;
	if (sysctl(mib, 1, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);
}

/*
 * Test sysctl(2) node destruction.
 */
static void
test87d(void)
{
	struct sysctlnode scn, oldscn, newscn, tmpscn;
	size_t oldlen;
	char buf[16];
	int i, r, mib[4], id[15];

	subtest = 3;

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	(void)destroy_node(mib, 2, TEST_DYNAMIC);

	/* Start with the path-related error code tests this time. */
	mib[1] = INT_MAX;
	if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOENT) e(0);

	mib[1] = MINIX_TEST;
	mib[2] = TEST_INT;
	if (destroy_node(mib, 3, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = TEST_BOOL;
	if (destroy_node(mib, 3, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = CTL_DESTROY;
	if (destroy_node(mib, 3, TEST_DYNAMIC) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Actual API tests. */
	mib[1] = MINIX_TEST;
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_IMMEDIATE |
	    CTLFLAG_READONLY | CTLTYPE_INT;
	scn.sysctl_size = sizeof(int);
	scn.sysctl_num = TEST_DYNAMIC;
	scn.sysctl_idata = 31415926;
	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	memcpy(&tmpscn, &scn, sizeof(scn));

	mib[2] = CTL_DESTROY;
	if (sysctl(mib, 3, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (sysctl(mib, 3, NULL, NULL, bad_ptr, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERS_0;
	scn.sysctl_num = TEST_DYNAMIC;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = INT_MAX; /* anything not valid */
	oldlen = sizeof(scn);
	if (sysctl(mib, 3, NULL, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOENT) e(0);
	if (oldlen != 0) e(0);

	scn.sysctl_num = TEST_PERM;
	oldlen = sizeof(scn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);
	if (oldlen != 0) e(0);

	scn.sysctl_num = TEST_SECRET;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOTEMPTY) e(0);

	scn.sysctl_num = -1;
	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOENT) e(0);

	scn.sysctl_num = TEST_DYNAMIC;
	strlcpy(scn.sysctl_name, "dynami", sizeof(scn.sysctl_name));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	strlcpy(scn.sysctl_name, "dynamic2", sizeof(scn.sysctl_name));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	memset(scn.sysctl_name, 'd', sizeof(scn.sysctl_name));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	if (oldscn.sysctl_ver == 0) e(0);
	oldscn.sysctl_ver = 0;
	if (memcmp(&oldscn, &tmpscn, sizeof(oldscn))) e(0);

	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOENT) e(0);

	/*
	 * We already tested destruction of one static node, by destroying
	 * TEST_DYNAMIC on the first run.  We now do a second deletion of a
	 * static node, TEST_DESTROY2, to test proper adjustment of parent
	 * stats.  We do a third static node deletion (on TEST_DESTROY1) later,
	 * to see that static nodes with dynamic descriptions can be freed.
	 */
	if (query_node(mib, 1, MINIX_TEST, &oldscn) != 0) e(0);

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = TEST_DESTROY2;
	r = sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn));
	if (r != 0 && r != -1) e(0);
	if (r == -1 && errno != ENOENT) e(0);

	if (query_node(mib, 1, MINIX_TEST, &newscn) != 0) e(0);

	if (newscn.sysctl_csize != oldscn.sysctl_csize) e(0);
	if (newscn.sysctl_clen != oldscn.sysctl_clen - !r) e(0);

	/* Try to destroy a (static) node in a read-only directory node. */
	mib[2] = TEST_SECRET;
	if (destroy_node(mib, 3, SECRET_VALUE) != -1) e(0);
	if (errno != EPERM) e(0);

	/*
	 * Errors during data copy-out of the destroyed node should not undo
	 * its destruction.
	 */
	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(i)) e(0);
	if (i != 31415926) e(0);

	mib[2] = CTL_DESTROY;
	oldlen = sizeof(scn);
	if (sysctl(mib, 3, bad_ptr, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	mib[2] = TEST_DYNAMIC;
	i = 0;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOENT) e(0);
	if (oldlen != 0) e(0);
	if (i != 0) e(0);

	mib[2] = CTL_CREATE;
	memcpy(&scn, &tmpscn, sizeof(scn));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = CTL_DESTROY;
	oldlen = sizeof(scn) - 1;
	if (sysctl(mib, 3, &scn, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOMEM) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = sizeof(i);
	if (sysctl(mib, 3, &i, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOENT) e(0);

	/*
	 * Now create and destroy a whole bunch of nodes in a subtree, mostly
	 * test linked list manipulation, but also to ensure that a nonempty
	 * tree node cannot be destroyed.
	 */
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_NODE;
	if (create_node(mib, 2, &scn, TEST_DYNAMIC, "dynamic", -1, NULL) == -1)
		e(0);

	for (i = 0; i < 15; i++) {
		snprintf(buf, sizeof(buf), "node%d", i);
		if ((id[i] = create_node(mib, 3, &scn, -1, buf, -1,
		    NULL)) == -1) e(i);

		if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(i);
		if (errno != ENOTEMPTY) e(i);
	}

	for (i = 0; i < 15; i += 2)
		if (destroy_node(mib, 3, id[i]) != 0) e(i);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTEMPTY) e(0);

	for (i = 0; i < 15; i += 2) {
		snprintf(buf, sizeof(buf), "node%d", i);
		if ((id[i] = create_node(mib, 3, &scn, -1, buf, -1,
		    NULL)) == -1) e(i);
	}

	for (i = 0; i < 3; i++)
		if (destroy_node(mib, 3, id[i]) != 0) e(i);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTEMPTY) e(0);

	for (i = 12; i < 15; i++)
		if (destroy_node(mib, 3, id[i]) != 0) e(i);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTEMPTY) e(0);

	for (i = 6; i < 9; i++)
		if (destroy_node(mib, 3, id[i]) != 0) e(i);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTEMPTY) e(0);

	for (i = 3; i < 6; i++)
		if (destroy_node(mib, 3, id[i]) != 0) e(i);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != -1) e(0);
	if (errno != ENOTEMPTY) e(0);

	for (i = 9; i < 12; i++)
		if (destroy_node(mib, 3, id[i]) != 0) e(i);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Finally, ensure that unprivileged users cannot destroy nodes. */
	(void)test_nonroot(sub87d);
}

/*
 * Get or a set the description for a particular node.  Compare the results
 * with the given description.  Return 0 on success, or -1 on failure with
 * errno set.
 */
static int
describe_node(const int * path, unsigned int pathlen, int id,
	const char * desc, int set)
{
	char buf[256], *p;
	struct sysctlnode scn;
	struct sysctldesc *scd;
	size_t oldlen;
	int mib[CTL_MAXNAME];

	if (pathlen >= CTL_MAXNAME) e(0);
	memcpy(mib, path, sizeof(mib[0]) * pathlen);
	mib[pathlen] = CTL_DESCRIBE;

	memset(&scn, 0, sizeof(scn));
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = id;
	if (set)
		scn.sysctl_desc = desc;
	if (sysctl(mib, pathlen + 1, buf, &oldlen, &scn, sizeof(scn)) != 0)
		return -1;

	scd = (struct sysctldesc *)buf;
	if (scd->descr_num != id) e(0);
	if (scd->descr_ver == 0) e(0);
	if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
	if (scd->descr_len != strlen(scd->descr_str) + 1) e(0);
	if (strcmp(scd->descr_str, desc)) e(0);
	if (oldlen != (size_t)((char *)NEXT_DESCR(scd) - buf)) e(0);
	for (p = scd->descr_str + scd->descr_len; p != &buf[oldlen]; p++)
		if (*p != '\0') e(0);
	return 0;
}

/*
 * Test getting descriptions from an unprivileged process.
 */
static void
sub87e(void)
{
	static char buf[2048];
	char seen[32], *p;
	struct sysctldesc *scd, *endscd;
	size_t oldlen;
	int mib[4];

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_DESCRIBE;

	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);

	scd = (struct sysctldesc *)buf;
	endscd = (struct sysctldesc *)&buf[oldlen];
	memset(seen, 0, sizeof(seen));

	while (scd < endscd) {
		if (scd->descr_num >= __arraycount(seen)) e(0);
		if (seen[scd->descr_num]++) e(0);

		if (scd->descr_ver == 0) e(0);
		if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
		if (scd->descr_len != strlen(scd->descr_str) + 1) e(0);

		p = scd->descr_str + scd->descr_len;
		while (p != (char *)NEXT_DESCR(scd))
			if (*p++ != '\0') e(0);

		scd = NEXT_DESCR(scd);
	}
	if (scd != endscd) e(0);

	if (!seen[TEST_INT]) e(0);
	if (!seen[TEST_BOOL]) e(0);
	if (!seen[TEST_QUAD]) e(0);
	if (!seen[TEST_STRING]) e(0);
	if (!seen[TEST_STRUCT]) e(0);
	if (seen[TEST_PRIVATE]) e(0);
	if (!seen[TEST_ANYWRITE]) e(0);
	if (seen[TEST_SECRET]) e(0);
	if (!seen[TEST_PERM]) e(0);

	if (describe_node(mib, 2, TEST_INT, "Value test field", 0) != 0) e(0);
	if (describe_node(mib, 2, TEST_PRIVATE, "", 0) != -1) e(0);
	if (errno != EPERM) e(0);
	if (describe_node(mib, 2, TEST_SECRET, "", 0) != -1) e(0);
	if (errno != EPERM) e(0);
	if (describe_node(mib, 2, TEST_PERM, "", 0) != 0) e(0);

	mib[2] = TEST_SECRET;
	mib[3] = CTL_DESCRIBE;
	if (sysctl(mib, 3, NULL, NULL, NULL, 0) != -1) e(0);
	if (errno != EPERM) e(0);

	if (describe_node(mib, 3, SECRET_VALUE, "", 0) != -1) e(0);
	if (errno != EPERM) e(0);
}

/*
 * Test sysctl(2) node descriptions, part 1: getting descriptions.
 */
static void
test87e(void)
{
	static char buf[2048];
	char seen[32], *p;
	struct sysctldesc *scd, *endscd;
	struct sysctlnode scn;
	size_t oldlen, len, sublen;
	int mib[4];

	subtest = 4;

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_DESCRIBE;
	memset(&scn, 0, sizeof(scn));

	/* Start with tests for getting a description listing. */
	if (sysctl(mib, 3, NULL, NULL, NULL, 0) != 0) e(0);

	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);
	len = oldlen;

	memset(buf, 0, sizeof(buf));
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != len) e(0);

	scd = (struct sysctldesc *)buf;
	endscd = (struct sysctldesc *)&buf[len];
	memset(seen, 0, sizeof(seen));

	sublen = (size_t)((char *)NEXT_DESCR(scd) - buf);

	while (scd < endscd) {
		if (scd->descr_num >= __arraycount(seen)) e(0);
		if (seen[scd->descr_num]++) e(0);

		if (scd->descr_ver == 0) e(0);
		if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
		if (scd->descr_len != strlen(scd->descr_str) + 1) e(0);

		/*
		 * This is not supposed to be complete.  We test different
		 * string lengths, private fields, and empty descriptions.
		 */
		switch (scd->descr_num) {
		case TEST_INT:
			if (strcmp(scd->descr_str, "Value test field")) e(0);
			break;
		case TEST_BOOL:
			if (strcmp(scd->descr_str, "Boolean test field")) e(0);
			break;
		case TEST_QUAD:
			if (strcmp(scd->descr_str, "Quad test field")) e(0);
			break;
		case TEST_STRING:
			if (strcmp(scd->descr_str, "String test field")) e(0);
			break;
		case TEST_PRIVATE:
			if (strcmp(scd->descr_str, "Private test field")) e(0);
			break;
		case TEST_SECRET:
			if (strcmp(scd->descr_str, "Private subtree")) e(0);
			break;
		case TEST_PERM:
			if (strcmp(scd->descr_str, "")) e(0);
			break;
		}

		/*
		 * If there are padding bytes, they must be zero, whether it is
		 * because we set them or the MIB service copied out zeroes.
		 */
		p = scd->descr_str + scd->descr_len;
		while (p != (char *)NEXT_DESCR(scd))
			if (*p++ != '\0') e(0);

		scd = NEXT_DESCR(scd);
	}
	if (scd != endscd) e(0);

	if (!seen[TEST_INT]) e(0);
	if (!seen[TEST_BOOL]) e(0);
	if (!seen[TEST_QUAD]) e(0);
	if (!seen[TEST_STRING]) e(0);
	if (!seen[TEST_STRUCT]) e(0);
	if (!seen[TEST_PRIVATE]) e(0);
	if (!seen[TEST_ANYWRITE]) e(0);
	if (!seen[TEST_SECRET]) e(0);
	if (!seen[TEST_PERM]) e(0);

	memset(buf, 0, sizeof(buf));
	oldlen = sublen;
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != -1) e(0);
	if (errno != ENOMEM) e(0);

	scd = (struct sysctldesc *)buf;
	if (scd->descr_num != TEST_INT) e(0);
	if (scd->descr_ver == 0) e(0);
	if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
	if (scd->descr_len != strlen(scd->descr_str) + 1) e(0);
	if (strcmp(scd->descr_str, "Value test field")) e(0);

	/* Next up, tests for getting a particular node's description. */
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, bad_ptr, &oldlen, NULL, 0) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (sysctl(mib, 3, NULL, NULL, bad_ptr, sizeof(scn) - 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, bad_ptr, sizeof(scn) + 1) != -1) e(0);
	if (errno != EINVAL) e(0);
	if (sysctl(mib, 3, NULL, NULL, bad_ptr, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERS_0;
	scn.sysctl_num = INT_MAX;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_flags = SYSCTL_VERSION;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOENT) e(0);

	scn.sysctl_num = TEST_BOOL;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	oldlen = sizeof(buf);
	scn.sysctl_num = TEST_INT;
	if (sysctl(mib, 3, bad_ptr, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	oldlen = sublen - 1;
	scn.sysctl_num = TEST_INT;
	if (sysctl(mib, 3, buf, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (oldlen != sublen) e(0);

	if (describe_node(mib, 2, TEST_INT, "Value test field", 0) != 0) e(0);
	if (describe_node(mib, 2, TEST_QUAD, "Quad test field", 0) != 0) e(0);
	if (describe_node(mib, 2, TEST_PRIVATE, "Private test field",
	    0) != 0) e(0);
	if (describe_node(mib, 2, TEST_SECRET, "Private subtree",
	    0) != 0) e(0);
	if (describe_node(mib, 2, TEST_PERM, "", 0) != 0) e(0);

	/*
	 * Make sure that unprivileged users cannot access privileged nodes'
	 * descriptions.  It doesn't sound too bad to me if they could, but
	 * these are apparently the rules..
	 */
	(void)test_nonroot(sub87e);

	/* Do some more path-related error code tests unrelated to the rest. */
	mib[1] = INT_MAX;
	if (describe_node(mib, 2, TEST_DYNAMIC, "", 0) != -1) e(0);
	if (errno != ENOENT) e(0);

	mib[1] = MINIX_TEST;
	mib[2] = TEST_INT;
	if (describe_node(mib, 3, TEST_DYNAMIC, "", 0) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = TEST_BOOL;
	if (describe_node(mib, 3, TEST_DYNAMIC, "", 0) != -1) e(0);
	if (errno != ENOTDIR) e(0);

	mib[2] = CTL_DESCRIBE;
	if (describe_node(mib, 3, TEST_DYNAMIC, "", 0) != -1) e(0);
	if (errno != EINVAL) e(0);
}

/*
 * Test setting descriptions from an unprivileged process.
 */
static void
sub87f(void)
{
	struct sysctlnode scn;
	int mib[3];

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_DESCRIBE;

	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = TEST_DYNAMIC;
	scn.sysctl_desc = "Description.";

	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);
}

/*
 * Test sysctl(2) node descriptions, part 2: setting descriptions.
 */
static void
test87f(void)
{
	static char buf[2048];
	char seen, *p;
	struct sysctlnode scn, tmpscn, scnset[3];
	struct sysctldesc *scd, *endscd, *scdset[2];
	size_t oldlen, len;
	int i, r, mib[4], id[2];

	subtest = 5;

	/*
	 * All tests that experiment with dynamic nodes must start with trying
	 * to destroy the TEST_DYNAMIC node first, as tests may be run
	 * individually, and this node exists as a static node after booting.
	 */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	(void)destroy_node(mib, 2, TEST_DYNAMIC);

	/*
	 * First try setting and retrieving the description of a dynamic node
	 * in a directory full of static nodes.
	 */
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_IMMEDIATE |
	    CTLFLAG_READONLY | CTLTYPE_INT;
	scn.sysctl_size = sizeof(int);
	scn.sysctl_num = TEST_DYNAMIC;
	scn.sysctl_idata = 27182818;
	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	memcpy(&tmpscn, &scn, sizeof(tmpscn));

	/* We should get an empty description for the node in a listing. */
	mib[2] = CTL_DESCRIBE;
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);

	scd = (struct sysctldesc *)buf;
	endscd = (struct sysctldesc *)&buf[oldlen];
	seen = 0;

	while (scd < endscd) {
		if (scd->descr_num == TEST_DYNAMIC) {
			if (seen++) e(0);

			if (scd->descr_len != 1) e(0);
			if (scd->descr_str[0] != '\0') e(0);
		}

		if (scd->descr_ver == 0) e(0);
		if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
		if (scd->descr_len != strlen(scd->descr_str) + 1) e(0);

		p = scd->descr_str + scd->descr_len;
		while (p != (char *)NEXT_DESCR(scd))
			if (*p++ != '\0') e(0);

		scd = NEXT_DESCR(scd);
	}
	if (scd != endscd) e(0);

	if (!seen) e(0);

	/* We should get an empty description quering the node directly. */
	if (describe_node(mib, 2, TEST_DYNAMIC, "", 0) != 0) e(0);

	/* Attempt to set a description with a bad description pointer. */
	if (describe_node(mib, 2, TEST_DYNAMIC, bad_ptr, 1) != -1) e(0);
	if (errno != EFAULT) e(0);

	/* Attempt to set a description that is longer than allowed. */
	memset(buf, 'A', sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	if (describe_node(mib, 2, TEST_DYNAMIC, buf, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* Now actually set a description. */
	if (describe_node(mib, 2, TEST_DYNAMIC, "Dynamic node", 1) != 0) e(0);
	len = strlen("Dynamic node") + 1;

	/* We should get the new description for the node in a listing. */
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, NULL, 0) != 0) e(0);

	scd = (struct sysctldesc *)buf;
	endscd = (struct sysctldesc *)&buf[oldlen];
	seen = 0;

	while (scd < endscd) {
		if (scd->descr_num == TEST_DYNAMIC) {
			if (seen++) e(0);

			if (scd->descr_len != len) e(0);
			if (strcmp(scd->descr_str, "Dynamic node")) e(0);
		}

		if (scd->descr_ver == 0) e(0);
		if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
		if (scd->descr_len != strlen(scd->descr_str) + 1) e(0);

		p = scd->descr_str + scd->descr_len;
		while (p != (char *)NEXT_DESCR(scd))
			if (*p++ != '\0') e(0);

		scd = NEXT_DESCR(scd);
	}
	if (scd != endscd) e(0);

	if (!seen) e(0);

	/* We should get the new description quering the node directly. */
	if (describe_node(mib, 2, TEST_DYNAMIC, "Dynamic node", 0) != 0) e(0);

	mib[2] = CTL_DESCRIBE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERS_0;
	scn.sysctl_num = TEST_INT;
	scn.sysctl_desc = "Test description";
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* It is not possible to replace an existing static description. */
	scn.sysctl_flags = SYSCTL_VERSION;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);

	/* Nonexistent nodes cannot be given a description. */
	scn.sysctl_num = INT_MAX;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOENT) e(0);

	/* It is not possible to replace an existing dynamic description. */
	scn.sysctl_num = TEST_DYNAMIC;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);

	/* It is not possible to set a description on a permanent node. */
	scn.sysctl_num = TEST_PERM;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EPERM) e(0);

	/* Verify that TEST_DYNAMIC now has CTLFLAG_OWNDESC set. */
	if (query_node(mib, 2, TEST_DYNAMIC, &scn) != 0) e(0);
	if (!(scn.sysctl_flags & CTLFLAG_OWNDESC)) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * Set a description on a static node, ensure that CTLFLAG_OWNDESC is
	 * set, and then destroy the static node.  This should still free the
	 * memory allocated for the description.  We cannot test whether the
	 * memory is really freed, but at least we can trigger this case at
	 * all, and leave the rest up to memory checkers or whatever.  Since we
	 * destroy the static node, we can not do this more than once, and thus
	 * we skip this test if the static node does not exist.
	 */
	r = describe_node(mib, 2, TEST_DESTROY1, "Destroy me", 1);

	if (r == -1 && errno != ENOENT) e(0);
	else if (r == 0) {
		if (query_node(mib, 2, TEST_DESTROY1, &scn) != 0) e(0);
		if (!(scn.sysctl_flags & CTLFLAG_OWNDESC)) e(0);

		if (describe_node(mib, 2, TEST_DESTROY1, "Destroy me", 0) != 0)
			e(0);

		if (destroy_node(mib, 2, TEST_DESTROY1) != 0) e(0);
	}

	/*
	 * Test queries and description listings in subtrees.
	 */
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_NODE;
	scn.sysctl_num = TEST_DYNAMIC;
	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	scn.sysctl_desc = "This will not be set.";
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	/* Setting sysctl_desc should have no effect during creation. */
	if (describe_node(mib, 2, TEST_DYNAMIC, "", 0) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	id[0] = create_node(mib, 3, &tmpscn, CTL_CREATE, "NodeA", -1, NULL);
	if (id[0] < 0) e(0);
	id[1] = create_node(mib, 3, &tmpscn, CTL_CREATE, "NodeB", -1, NULL);
	if (id[1] < 0) e(0);
	if (id[0] == id[1]) e(0);

	mib[3] = CTL_QUERY;
	oldlen = sizeof(scnset);
	if (sysctl(mib, 4, scnset, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(scnset[0]) * 2) e(0);
	i = (scnset[0].sysctl_num != id[0]);
	if (scnset[i].sysctl_num != id[0]) e(0);
	if (scnset[1 - i].sysctl_num != id[1]) e(0);
	if (scnset[i].sysctl_flags & CTLFLAG_OWNDESC) e(0);
	if (scnset[1 - i].sysctl_flags & CTLFLAG_OWNDESC) e(0);

	mib[3] = CTL_DESCRIBE;
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 4, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);

	scdset[0] = (struct sysctldesc *)buf;
	scdset[1] = NEXT_DESCR(scdset[0]);
	if ((char *)NEXT_DESCR(scdset[1]) != &buf[oldlen]) e(0);
	i = (scdset[0]->descr_num != id[0]);
	if (scdset[i]->descr_num != id[0]) e(0);
	if (scdset[i]->descr_ver == 0) e(0);
	if (scdset[i]->descr_len != 1) e(0);
	if (scdset[i]->descr_str[0] != '\0') e(0);
	if (scdset[1 - i]->descr_num != id[1]) e(0);
	if (scdset[1 - i]->descr_ver == 0) e(0);
	if (scdset[1 - i]->descr_len != 1) e(0);
	if (scdset[1 - i]->descr_str[0] != '\0') e(0);

	if (describe_node(mib, 3, id[0], "Description A", 1) != 0) e(0);

	mib[3] = CTL_QUERY;
	oldlen = sizeof(scnset);
	if (sysctl(mib, 4, scnset, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(scnset[0]) * 2) e(0);
	i = (scnset[0].sysctl_num != id[0]);
	if (scnset[i].sysctl_num != id[0]) e(0);
	if (scnset[1 - i].sysctl_num != id[1]) e(0);
	if (!(scnset[i].sysctl_flags & CTLFLAG_OWNDESC)) e(0);
	if (scnset[1 - i].sysctl_flags & CTLFLAG_OWNDESC) e(0);

	mib[3] = CTL_DESCRIBE;
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 4, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);

	scdset[0] = (struct sysctldesc *)buf;
	scdset[1] = NEXT_DESCR(scdset[0]);
	if ((char *)NEXT_DESCR(scdset[1]) != &buf[oldlen]) e(0);
	i = (scdset[0]->descr_num != id[0]);
	if (scdset[i]->descr_num != id[0]) e(0);
	if (scdset[i]->descr_ver == 0) e(0);
	if (strcmp(scdset[i]->descr_str, "Description A")) e(0);
	if (scdset[i]->descr_len != strlen(scdset[i]->descr_str) + 1) e(0);
	if (scdset[1 - i]->descr_num != id[1]) e(0);
	if (scdset[1 - i]->descr_ver == 0) e(0);
	if (scdset[1 - i]->descr_len != 1) e(0);
	if (scdset[1 - i]->descr_str[0] != '\0') e(0);

	if (describe_node(mib, 3, id[1], "Description B", 1) != 0) e(0);

	mib[3] = CTL_QUERY;
	oldlen = sizeof(scnset);
	if (sysctl(mib, 4, scnset, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(scnset[0]) * 2) e(0);
	i = (scnset[0].sysctl_num != id[0]);
	if (scnset[i].sysctl_num != id[0]) e(0);
	if (scnset[1 - i].sysctl_num != id[1]) e(0);
	if (!(scnset[i].sysctl_flags & CTLFLAG_OWNDESC)) e(0);
	if (!(scnset[1 - i].sysctl_flags & CTLFLAG_OWNDESC)) e(0);

	mib[3] = CTL_DESCRIBE;
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 4, buf, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen == 0) e(0);

	scdset[0] = (struct sysctldesc *)buf;
	scdset[1] = NEXT_DESCR(scdset[0]);
	if ((char *)NEXT_DESCR(scdset[1]) != &buf[oldlen]) e(0);
	i = (scdset[0]->descr_num != id[0]);
	if (scdset[i]->descr_num != id[0]) e(0);
	if (scdset[i]->descr_ver == 0) e(0);
	if (strcmp(scdset[i]->descr_str, "Description A")) e(0);
	if (scdset[i]->descr_len != strlen(scdset[i]->descr_str) + 1) e(0);
	if (scdset[1 - i]->descr_num != id[1]) e(0);
	if (scdset[1 - i]->descr_ver == 0) e(0);
	if (strcmp(scdset[1 - i]->descr_str, "Description B")) e(0);
	if (scdset[1 - i]->descr_len != strlen(scdset[1 - i]->descr_str) + 1)
		e(0);

	if (destroy_node(mib, 3, id[0]) != 0) e(0);
	if (destroy_node(mib, 3, id[1]) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * Test that the resulting description is copied out after setting it,
	 * and that copy failures do not undo the description getting set.
	 */
	if (create_node(mib, 2, &tmpscn, TEST_DYNAMIC, "dynamic", -1,
	    NULL) == -1) e(0);

	mib[2] = CTL_DESCRIBE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = TEST_DYNAMIC;
	scn.sysctl_desc = "Testing..";
	memset(buf, 0, sizeof(buf));
	oldlen = sizeof(buf);
	if (sysctl(mib, 3, buf, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen == 0) e(0);
	len = oldlen;

	scd = (struct sysctldesc *)buf;
	if (scd->descr_str[scd->descr_len - 1] != '\0') e(0);
	if (scd->descr_len != strlen(scn.sysctl_desc) + 1) e(0);
	if (strcmp(scd->descr_str, scn.sysctl_desc)) e(0);
	if (oldlen != (size_t)((char *)NEXT_DESCR(scd) - buf)) e(0);
	p = scd->descr_str + scd->descr_len;
	while (p != (char *)NEXT_DESCR(scd))
		if (*p++ != '\0') e(0);

	if (describe_node(mib, 2, TEST_DYNAMIC, "Testing..", 0) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	if (create_node(mib, 2, &tmpscn, TEST_DYNAMIC, "dynamic", -1,
	    NULL) == -1) e(0);

	memset(buf, 0, sizeof(buf));
	oldlen = len - 1;
	if (sysctl(mib, 3, buf, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != ENOMEM) e(0);
	if (oldlen != len) e(0);

	if (describe_node(mib, 2, TEST_DYNAMIC, "Testing..", 0) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	if (create_node(mib, 2, &tmpscn, TEST_DYNAMIC, "dynamic", -1,
	    NULL) == -1) e(0);

	memset(buf, 0, sizeof(buf));
	oldlen = len;
	if (sysctl(mib, 3, bad_ptr, &oldlen, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (describe_node(mib, 2, TEST_DYNAMIC, "Testing..", 0) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Finally, ensure that unprivileged users cannot set descriptions. */
	memcpy(&scn, &tmpscn, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_IMMEDIATE |
	    CTLFLAG_READWRITE | CTLFLAG_ANYWRITE | CTLTYPE_INT;
	if (create_node(mib, 2, &scn, TEST_DYNAMIC, "dynamic", -1,
	    NULL) == -1) e(0);

	(void)test_nonroot(sub87f);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);
}

/*
 * Set or test buffer contents.  When setting, the buffer is filled with a
 * sequence of bytes that is a) free of null characters and b) likely to cause
 * detection of wrongly copied subsequences.  When testing, for any size up to
 * the size used to set the buffer contents, 0 is returned if the buffer
 * contents match expectations, or -1 if they do not.
 */
static int
test_buf(char * buf, unsigned char c, size_t size, int set)
{
	unsigned char *ptr;
	int step;

	ptr = (unsigned char *)buf;

	for (step = 1; size > 0; size--) {
		if (set)
			*ptr++ = c;
		else if (*ptr++ != c)
			return -1;

		c += step;
		if (c == 0) {
			if (++step == 256)
				step = 1;
			c += step;
		}
	}

	return 0;
}

/*
 * Test large data sizes from an unprivileged process.
 */
static void
sub87g(void)
{
	char *ptr;
	size_t size, oldlen;
	int id, mib[3];

	size = getpagesize() * 3;

	if ((ptr = mmap(NULL, size, PROT_READ, MAP_ANON | MAP_PRIVATE, -1,
	    0)) == MAP_FAILED) e(0);
	memset(ptr, 0x2f, size);

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = TEST_DYNAMIC;
	oldlen = size - 2;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size - 2) e(0);
	if (test_buf(ptr, 'D', size - 2, 0) != 0) e(0);

	/*
	 * Given the large data size, we currently expect this attempt to
	 * write to the structure to be blocked by the MIB service.
	 */
	if (sysctl(mib, 3, NULL, NULL, ptr, oldlen) != -1) e(0);
	if (errno != EPERM) e(0);

	/* Get the ID of the second dynamic node. */
	mib[2] = TEST_ANYWRITE;
	oldlen = sizeof(id);
	if (sysctl(mib, 3, &id, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(id)) e(0);
	if (id < 0) e(0);

	/*
	 * Test data size limits for strings as well, although here we can also
	 * ensure that we hit the right check by testing with a shorter string.
	 */
	mib[2] = id;
	oldlen = size;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);
	if (test_buf(ptr, 'f', size - 1, 0) != 0) e(0);
	if (ptr[size - 1] != '\0') e(0);

	test_buf(ptr, 'h', size - 1, 1);
	if (sysctl(mib, 3, NULL, NULL, ptr, size) != -1) e(0);
	if (errno != EPERM) e(0);

	if (sysctl(mib, 3, NULL, NULL, ptr, getpagesize() - 1) != 0) e(0);

	if (munmap(ptr, size) != 0) e(0);
}

/*
 * Test large data sizes and mid-data page faults.
 */
static void
test87g(void)
{
	struct sysctlnode scn, newscn;
	char *ptr;
	size_t pgsz, size, oldlen;
	int id, mib[3];

	subtest = 6;

	/*
	 * No need to go overboard with sizes here; it will just cause the MIB
	 * service's memory usage to grow - permanently.  Three pages followed
	 * by an unmapped page is plenty for this test.
	 */
	pgsz = getpagesize();
	size = pgsz * 3;

	if ((ptr = mmap(NULL, size + pgsz, PROT_READ, MAP_ANON | MAP_PRIVATE,
	    -1, 0)) == MAP_FAILED) e(0);
	if (munmap(ptr + size, pgsz) != 0) e(0);

	(void)destroy_node(mib, 2, TEST_DYNAMIC);

	/* Test string creation initializers with an accurate length. */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_OWNDATA |
	    CTLFLAG_READWRITE | CTLTYPE_STRING;
	scn.sysctl_num = TEST_DYNAMIC;
	scn.sysctl_data = ptr;
	scn.sysctl_size = size;
	strlcpy(scn.sysctl_name, "dynamic", sizeof(scn.sysctl_name));
	test_buf(ptr, 'a', size, 1);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0); /* no null terminator */

	scn.sysctl_size++;
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	scn.sysctl_size--;
	ptr[size - 1] = '\0';
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);

	memset(ptr, 0, size);
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);
	if (ptr[size - 1] != '\0') e(0);
	if (test_buf(ptr, 'a', size - 1, 0) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test string creation initializers with no length. */
	mib[2] = CTL_CREATE;
	scn.sysctl_size = 0;
	test_buf(ptr, 'b', size, 1);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	test_buf(ptr, 'b', size - 1, 1);
	ptr[size - 1] = '\0';
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (query_node(mib, 2, TEST_DYNAMIC, &newscn) != 0) e(0);
	if (newscn.sysctl_size != size) e(0);

	mib[2] = TEST_DYNAMIC;
	if (sysctl(mib, 3, NULL, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);

	memset(ptr, 0x7e, size);
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);
	if (ptr[size - 1] != '\0') e(0);
	if (test_buf(ptr, 'b', size - 1, 0) != 0) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/*
	 * Test string creation initializers with a length exceeding the string
	 * length.  If the string is properly null terminated, this should not
	 * result in a fault.
	 */
	mib[2] = CTL_CREATE;
	scn.sysctl_size = size;
	scn.sysctl_data = &ptr[size - pgsz - 5];
	test_buf(&ptr[size - pgsz - 5], 'c', pgsz + 5, 1);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	ptr[size - 1] = '\0';
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	if (query_node(mib, 2, TEST_DYNAMIC, &newscn) != 0) e(0);
	if (newscn.sysctl_size != size) e(0);

	mib[2] = TEST_DYNAMIC;
	oldlen = size - pgsz - 6;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != pgsz + 5) e(0);
	/* We rely on only the actual string getting copied out here. */
	if (memcmp(ptr, &ptr[size - pgsz - 5], pgsz + 5)) e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);

	/* Test structure creation initializers. */
	mib[2] = CTL_CREATE;
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_OWNDATA |
	    CTLFLAG_ANYWRITE | CTLFLAG_READWRITE | CTLTYPE_STRUCT;
	scn.sysctl_size = size - 2;
	scn.sysctl_data = &ptr[3];
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EFAULT) e(0);

	scn.sysctl_data = &ptr[2];
	test_buf(&ptr[2], 'd', size - 2, 1);
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	mib[2] = TEST_DYNAMIC;
	memset(ptr, 0x3b, size);
	oldlen = size - 2;
	if (sysctl(mib, 3, &ptr[3], &oldlen, NULL, 0) != -1) e(0);
	if (errno != EFAULT) e(0);
	oldlen = size - 2;
	if (sysctl(mib, 3, &ptr[2], &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size - 2) e(0);
	if (test_buf(&ptr[2], 'd', size - 2, 0) != 0) e(0);

	/*
	 * Test setting new values.  We already have a structure node, so let's
	 * start there.
	 */
	test_buf(&ptr[2], 'D', size - 2, 1);
	if (sysctl(mib, 3, NULL, NULL, &ptr[3], size - 2) != -1) e(0);
	if (errno != EFAULT) e(0);

	/* Did the mid-data fault cause a partial update?  It better not. */
	memset(ptr, 0x4c, size);
	oldlen = size - 2;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size - 2) e(0);
	if (test_buf(ptr, 'd', size - 2, 0) != 0) e(0);

	test_buf(&ptr[2], 'D', size - 2, 1);
	if (sysctl(mib, 3, NULL, NULL, &ptr[2], size - 2) != 0) e(0);

	memset(ptr, 0x5d, size);
	oldlen = size - 2;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size - 2) e(0);
	if (test_buf(ptr, 'D', size - 2, 0) != 0) e(0);

	/*
	 * We are going to reuse TEST_DYNAMIC for the non-root test later, so
	 * create a new node for string tests.
	 */
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_OWNDATA |
	    CTLFLAG_ANYWRITE | CTLFLAG_READWRITE | CTLTYPE_STRING;
	scn.sysctl_num = CTL_CREATE;
	scn.sysctl_size = size;
	scn.sysctl_data = ptr;
	test_buf(ptr, 'e', size - 1, 1);
	ptr[size - 1] = '\0';
	strlcpy(scn.sysctl_name, "dynamic2", sizeof(scn.sysctl_name));
	oldlen = sizeof(newscn);
	if (sysctl(mib, 3, &newscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(newscn)) e(0);
	id = newscn.sysctl_num;
	if (id < 0) e(0);

	/*
	 * Test setting a short but faulty string, ensuring that no partial
	 * update on the field contents takes place.
	 */
	mib[2] = id;
	memcpy(&ptr[size - 3], "XYZ", 3);
	if (sysctl(mib, 3, NULL, NULL, &ptr[size - 3], 4) != -1) e(0);
	if (errno != EFAULT) e(0);

	oldlen = size;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);
	if (test_buf(ptr, 'e', size - 1, 0) != 0) e(0);
	if (ptr[size - 1] != '\0') e(0);

	memcpy(&ptr[size - 3], "XYZ", 3);
	if (sysctl(mib, 3, NULL, NULL, &ptr[size - 3], 3) != 0) e(0);

	oldlen = size;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != 4) e(0);
	if (strcmp(ptr, "XYZ")) e(0);

	test_buf(&ptr[1], 'f', size - 1, 1);
	if (sysctl(mib, 3, NULL, NULL, &ptr[1], size - 1) != 0) e(0);

	test_buf(&ptr[1], 'G', size - 1, 1);
	if (sysctl(mib, 3, NULL, NULL, &ptr[1], size) != -1) e(0);
	if (errno != EFAULT) e(0);

	oldlen = size;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != size) e(0);
	if (test_buf(ptr, 'f', size - 1, 0) != 0) e(0);
	if (ptr[size - 1] != '\0') e(0);

	/*
	 * Test descriptions as well.  First, the MIB service does not allow
	 * for overly long descriptions, although the limit is not exposed.
	 * Three memory pages worth of text is way too long though.
	 */
	memset(ptr, 'A', size);
	if (describe_node(mib, 2, id, ptr, 1) != -1) e(0);
	if (errno != EINVAL) e(0); /* not EFAULT, should never get that far */

	ptr[size - 1] = '\0';
	if (describe_node(mib, 2, id, ptr, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (describe_node(mib, 2, id, "", 0) != 0) e(0);

	/*
	 * Second, the description routine must deal with faults occurring
	 * while it is trying to find the string end.
	 */
	ptr[size - 2] = 'B';
	ptr[size - 1] = 'C';
	if (describe_node(mib, 2, id, &ptr[size - 3], 1) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (describe_node(mib, 2, id, "", 0) != 0) e(0);

	ptr[size - 1] = '\0';
	if (describe_node(mib, 2, id, &ptr[size - 3], 1) != 0) e(0);

	if (describe_node(mib, 2, id, "AB", 0) != 0) e(0);

	/* Pass the second dynamic node ID to the unprivileged child. */
	mib[2] = TEST_ANYWRITE;
	if (sysctl(mib, 3, NULL, NULL, &id, sizeof(id)) != 0) e(0);

	(void)test_nonroot(sub87g);

	mib[2] = id;
	oldlen = size;
	if (sysctl(mib, 3, ptr, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != pgsz) e(0);
	if (test_buf(ptr, 'h', pgsz - 1, 1) != 0) e(0);
	if (ptr[pgsz - 1] != '\0') e(0);

	if (destroy_node(mib, 2, TEST_DYNAMIC) != 0) e(0);
	if (destroy_node(mib, 2, id) != 0) e(0);

	munmap(ptr, size);
}

/*
 * Verify whether the given node on the given path has the given node version.
 * Return 0 if the version matches, or -1 if it does not or a failure occurred.
 */
static int
check_version(const int * path, unsigned int pathlen, int id, uint32_t ver)
{
	struct sysctlnode scn;
	struct sysctldesc scd;
	size_t oldlen;
	int r, mib[CTL_MAXNAME];

	assert(pathlen < CTL_MAXNAME);
	memcpy(mib, path, sizeof(mib[0]) * pathlen);
	mib[pathlen] = CTL_DESCRIBE;

	/*
	 * For some reason, when retrieving a particular description (as
	 * opposed to setting one), the node version number is not checked.
	 * In order to test this, we deliberately pass in a node version number
	 * that, if checked, would eventually cause failures.
	 */
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = id;
	scn.sysctl_ver = 1;
	oldlen = sizeof(scd);
	r = sysctl(mib, pathlen + 1, &scd, &oldlen, &scn, sizeof(scn));
	if (r == -1 && errno != ENOMEM) e(0);

	return (scd.descr_ver == ver) ? 0 : -1;
}

/*
 * Test sysctl(2) node versioning.
 */
static void
test87h(void)
{
	struct sysctlnode scn, oldscn;
	size_t oldlen;
	uint32_t ver[4];
	int mib[4], id[4];

	/*
	 * The other tests have already tested sufficiently that a zero version
	 * is always accepted in calls.  Here, we test that node versions
	 * actually change when creating and destroying nodes, and that the
	 * right version test is implemented for all of the four node meta-
	 * operations (query, create, destroy, describe).  Why did we not do
	 * this earlier, you ask?  Well, versioning was implemented later on.
	 */
	subtest = 7;

	/*
	 * Test versioning with node creation.
	 */
	mib[0] = CTL_MINIX;
	mib[1] = MINIX_TEST;
	mib[2] = CTL_CREATE;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_READWRITE | CTLTYPE_NODE;
	scn.sysctl_num = CTL_CREATE;
	strlcpy(scn.sysctl_name, "NodeA", sizeof(scn.sysctl_name));
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	id[0] = oldscn.sysctl_num;
	ver[0] = oldscn.sysctl_ver;
	if (ver[0] == 0) e(0);

	if (check_version(mib, 0, CTL_MINIX, ver[0]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[0]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[0]) != 0) e(0);

	strlcpy(scn.sysctl_name, "NodeB", sizeof(scn.sysctl_name));
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	id[1] = oldscn.sysctl_num;
	ver[1] = oldscn.sysctl_ver;
	if (ver[1] == 0) e(0);
	if (ver[1] != NEXT_VER(ver[0])) e(0);

	if (check_version(mib, 0, CTL_MINIX, ver[1]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[1]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[0]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[1]) != 0) e(0);

	/* A version that is too high should be rejected. */
	mib[2] = id[0];
	mib[3] = CTL_CREATE;
	scn.sysctl_flags = SYSCTL_VERSION | CTLFLAG_IMMEDIATE |
	    CTLFLAG_READWRITE | CTLTYPE_INT;
	scn.sysctl_size = sizeof(int);
	scn.sysctl_ver = NEXT_VER(ver[1]);
	strlcpy(scn.sysctl_name, "ValueA", sizeof(scn.sysctl_name));
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* The version of the parent node should be accepted. */
	scn.sysctl_ver = ver[0]; /* different from the root node version */
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 4, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	id[2] = oldscn.sysctl_num;
	ver[2] = oldscn.sysctl_ver;
	if (ver[2] == 0) e(0);
	if (ver[2] != NEXT_VER(ver[1])) e(0);

	if (check_version(mib, 0, CTL_MINIX, ver[2]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[2]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[2]) != 0) e(0);
	if (check_version(mib, 3, id[2], ver[2]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[1]) != 0) e(0);

	/* A version that is too low (old) should be rejected. */
	mib[2] = id[1];

	scn.sysctl_ver = ver[0];
	strlcpy(scn.sysctl_name, "ValueB", sizeof(scn.sysctl_name));
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/* The version of the root node should be accepted. */
	scn.sysctl_ver = ver[2]; /* different from the parent node version */
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 4, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	id[3] = oldscn.sysctl_num;
	ver[3] = oldscn.sysctl_ver;
	if (ver[3] == 0) e(0);
	if (ver[3] != NEXT_VER(ver[2])) e(0);

	if (check_version(mib, 0, CTL_MINIX, ver[3]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[3]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[2]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[3]) != 0) e(0);
	if (check_version(mib, 3, id[3], ver[3]) != 0) e(0);
	mib[2] = id[0];
	if (check_version(mib, 3, id[2], ver[2]) != 0) e(0);

	/*
	 * Test versioning with node queries.
	 */
	mib[3] = CTL_QUERY;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_ver = ver[0]; /* previous parent version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_ver = ver[2]; /* parent version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	scn.sysctl_ver = ver[2]; /* root version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	scn.sysctl_ver = NEXT_VER(ver[3]); /* nonexistent version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * Test versioning with node description.
	 */
	mib[2] = CTL_DESCRIBE;
	scn.sysctl_num = id[0];
	scn.sysctl_ver = ver[3]; /* root and parent, but not target version */
	scn.sysctl_desc = "Parent A";
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_ver = ver[1]; /* another bad version */
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_ver = ver[2]; /* target version */
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	/* Neither querying nor description should have changed versions. */
	if (check_version(mib, 0, CTL_MINIX, ver[3]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[3]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[2]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[3]) != 0) e(0);
	mib[2] = id[1];
	if (check_version(mib, 3, id[3], ver[3]) != 0) e(0);
	mib[2] = id[0];
	if (check_version(mib, 3, id[2], ver[2]) != 0) e(0);

	/*
	 * Test versioning with node destruction.
	 */
	mib[3] = CTL_DESTROY;
	memset(&scn, 0, sizeof(scn));
	scn.sysctl_flags = SYSCTL_VERSION;
	scn.sysctl_num = id[2];
	scn.sysctl_ver = ver[3]; /* root but not target version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_ver = ver[2]; /* target (and parent) version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	/* Fortunately, versions are predictable. */
	ver[0] = NEXT_VER(ver[3]);

	if (check_version(mib, 0, CTL_MINIX, ver[0]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[0]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[0]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[3]) != 0) e(0);

	mib[2] = id[1];
	scn.sysctl_num = id[3];
	scn.sysctl_ver = ver[0]; /* root but not target version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_ver = ver[3]; /* target (and parent) version */
	if (sysctl(mib, 4, NULL, NULL, &scn, sizeof(scn)) != 0) e(0);

	ver[1] = NEXT_VER(ver[0]);

	if (check_version(mib, 0, CTL_MINIX, ver[1]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[1]) != 0) e(0);
	if (check_version(mib, 2, id[0], ver[0]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[1]) != 0) e(0);

	mib[2] = CTL_DESTROY;
	scn.sysctl_num = id[0];
	scn.sysctl_ver = ver[1]; /* root and parent, but not target version */
	if (sysctl(mib, 3, NULL, NULL, &scn, sizeof(scn)) != -1) e(0);
	if (errno != EINVAL) e(0);

	scn.sysctl_ver = ver[0]; /* target version */
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	if (oldscn.sysctl_num != id[0]) e(0);
	if (oldscn.sysctl_ver != ver[0]) e(0);

	ver[2] = NEXT_VER(ver[1]);

	if (check_version(mib, 0, CTL_MINIX, ver[2]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[2]) != 0) e(0);
	if (check_version(mib, 2, id[1], ver[1]) != 0) e(0);

	/* For the last destruction, just see if we get the old version. */
	scn.sysctl_num = id[1];
	scn.sysctl_ver = 0;
	oldlen = sizeof(oldscn);
	if (sysctl(mib, 3, &oldscn, &oldlen, &scn, sizeof(scn)) != 0) e(0);
	if (oldlen != sizeof(oldscn)) e(0);
	if (oldscn.sysctl_num != id[1]) e(0);
	if (oldscn.sysctl_ver != ver[1]) e(0);

	ver[3] = NEXT_VER(ver[2]);

	if (check_version(mib, 0, CTL_MINIX, ver[3]) != 0) e(0);
	if (check_version(mib, 1, MINIX_TEST, ver[3]) != 0) e(0);
}

/*
 * Perform pre-test initialization.
 */
static void
test87_init(void)
{
	size_t oldlen;
	int mib[3];

	subtest = 99;

	if ((bad_ptr = mmap(NULL, getpagesize(), PROT_READ,
	    MAP_ANON | MAP_PRIVATE, -1, 0)) == MAP_FAILED) e(0);
	if (munmap(bad_ptr, getpagesize()) != 0) e(0);

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_MIB;
	mib[2] = MIB_NODES;
	oldlen = sizeof(nodes);
	if (sysctl(mib, 3, &nodes, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(nodes)) e(0);

	mib[2] = MIB_OBJECTS;
	oldlen = sizeof(objects);
	if (sysctl(mib, 3, &objects, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(objects)) e(0);
}

/*
 * Perform post-test checks.
 */
static void
test87_check(void)
{
	unsigned int newnodes, newobjects;
	size_t oldlen;
	int mib[3];

	subtest = 99;

	mib[0] = CTL_MINIX;
	mib[1] = MINIX_MIB;
	mib[2] = MIB_NODES;
	oldlen = sizeof(newnodes);
	if (sysctl(mib, 3, &newnodes, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(newnodes)) e(0);

	/*
	 * Upon the first run, the total number of nodes must actually go down,
	 * as we destroy number of static nodes.  Upon subsequent runs, the
	 * number of nodes should remain stable.  Thus, we can safely test that
	 * the number of nodes has not gone up as a result of the test.
	 */
	if (newnodes > nodes) e(0);

	mib[2] = MIB_OBJECTS;
	oldlen = sizeof(newobjects);
	if (sysctl(mib, 3, &newobjects, &oldlen, NULL, 0) != 0) e(0);
	if (oldlen != sizeof(newobjects)) e(0);

	/*
	 * The number of dynamically allocated objects should remain the same
	 * across the test.
	 */
	if (newobjects != objects) e(0);
}

/*
 * Test program for sysctl(2).
 */
int
main(int argc, char ** argv)
{
	int i, m;

	start(87);

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	test87_init();

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x001) test87a();
		if (m & 0x002) test87b();
		if (m & 0x004) test87c();
		if (m & 0x008) test87d();
		if (m & 0x010) test87e();
		if (m & 0x020) test87f();
		if (m & 0x040) test87g();
		if (m & 0x080) test87h();
	}

	test87_check();

	quit();
}
