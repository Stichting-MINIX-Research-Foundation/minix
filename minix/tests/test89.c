/* Tests for set[ug]id, sete[ug]id, and saved IDs - by D.C. van Moolenbroek */
/* This test must be run as root, as it tests privileged operations. */
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include "common.h"

#define ITERATIONS	2

/* These are in a specific order. */
enum {
	SUB_REAL,	/* test set[ug]id(2) */
	SUB_EFF,	/* test sete[ug]id(2) */
	SUB_REAL_E0,	/* test setgid(2) with euid=0 */
	SUB_EFF_E0,	/* test setegid(2) with euid=0 */
	SUB_RETAIN,	/* test r/e/s preservation across fork(2), exec(2) */
};

static const char *executable;

/*
 * The table below is exhaustive in terms of different combinations of real,
 * effective, and saved user IDs (with 0 being a special value, but 1 and 2
 * being interchangeable), but not all these combinations can actually be
 * established in practice.  The results for which there is no way to create
 * the initial condition are set to -1.  If we ever implement setresuid(2),
 * these results can be filled in and tested as well.
 */
static const struct uid_set {
	uid_t ruid;
	uid_t euid;
	uid_t suid;
	uid_t uid;
	int res;
	int eres;
} uid_sets[] = {
	{ 0, 0, 0, 0,  1,  1 },
	{ 0, 0, 0, 1,  1,  1 },
	{ 0, 0, 1, 0,  1,  1 },
	{ 0, 0, 1, 1,  1,  1 },
	{ 0, 0, 1, 2,  1,  1 },
	{ 0, 1, 0, 0,  1,  1 },
	{ 0, 1, 0, 1,  0,  0 },
	{ 0, 1, 0, 2,  0,  0 },
	{ 0, 1, 1, 0,  1,  1 },
	{ 0, 1, 1, 1,  0,  1 },
	{ 0, 1, 1, 2,  0,  0 },
	{ 0, 1, 2, 0, -1, -1 },
	{ 0, 1, 2, 1, -1, -1 },
	{ 0, 1, 2, 2, -1, -1 },
	{ 1, 0, 0, 0,  1,  1 },
	{ 1, 0, 0, 1,  1,  1 },
	{ 1, 0, 0, 2,  1,  1 },
	{ 1, 0, 1, 0, -1, -1 },
	{ 1, 0, 1, 1, -1, -1 },
	{ 1, 0, 1, 2, -1, -1 },
	{ 1, 0, 2, 0, -1, -1 },
	{ 1, 0, 2, 1, -1, -1 },
	{ 1, 0, 2, 2, -1, -1 },
	{ 1, 1, 0, 0,  0,  1 },
	{ 1, 1, 0, 1,  1,  1 },
	{ 1, 1, 0, 2,  0,  0 },
	{ 1, 1, 1, 0,  0,  0 },
	{ 1, 1, 1, 1,  1,  1 },
	{ 1, 1, 1, 2,  0,  0 },
	{ 1, 1, 2, 0,  0,  0 },
	{ 1, 1, 2, 1,  1,  1 },
	{ 1, 1, 2, 2,  0,  1 },
	{ 1, 2, 0, 0,  0,  1 },
	{ 1, 2, 0, 1,  1,  1 },
	{ 1, 2, 0, 2,  0,  0 },
	{ 1, 2, 1, 0, -1, -1 },
	{ 1, 2, 1, 1, -1, -1 },
	{ 1, 2, 1, 2, -1, -1 },
	{ 1, 2, 2, 0,  0,  0 },
	{ 1, 2, 2, 1,  1,  1 },
	{ 1, 2, 2, 2,  0,  1 },
};

/*
 * The same type of table but now for group identifiers.  In this case, all
 * combinations are possible to establish in practice, because the effective
 * UID, not the GID, is used for the privilege check.  GID 0 does not have any
 * special meaning, but we still test it as though it does, in order to ensure
 * that it in fact does not.
 */
static const struct gid_set {
	gid_t rgid;
	gid_t egid;
	gid_t sgid;
	gid_t gid;
	int res;
	int eres;
} gid_sets[] = {
	{ 0, 0, 0, 0, 1, 1 },
	{ 0, 0, 0, 1, 0, 0 },
	{ 0, 0, 1, 0, 1, 1 },
	{ 0, 0, 1, 1, 0, 1 },
	{ 0, 0, 1, 2, 0, 0 },
	{ 0, 1, 0, 0, 1, 1 },
	{ 0, 1, 0, 1, 0, 0 },
	{ 0, 1, 0, 2, 0, 0 },
	{ 0, 1, 1, 0, 1, 1 },
	{ 0, 1, 1, 1, 0, 1 },
	{ 0, 1, 1, 2, 0, 0 },
	{ 0, 1, 2, 0, 1, 1 },
	{ 0, 1, 2, 1, 0, 0 },
	{ 0, 1, 2, 2, 0, 1 },
	{ 1, 0, 0, 0, 0, 1 },
	{ 1, 0, 0, 1, 1, 1 },
	{ 1, 0, 0, 2, 0, 0 },
	{ 1, 0, 1, 0, 0, 0 },
	{ 1, 0, 1, 1, 1, 1 },
	{ 1, 0, 1, 2, 0, 0 },
	{ 1, 0, 2, 0, 0, 0 },
	{ 1, 0, 2, 1, 1, 1 },
	{ 1, 0, 2, 2, 0, 1 },
	{ 1, 1, 0, 0, 0, 1 },
	{ 1, 1, 0, 1, 1, 1 },
	{ 1, 1, 0, 2, 0, 0 },
	{ 1, 1, 1, 0, 0, 0 },
	{ 1, 1, 1, 1, 1, 1 },
	{ 1, 1, 1, 2, 0, 0 },
	{ 1, 1, 2, 0, 0, 0 },
	{ 1, 1, 2, 1, 1, 1 },
	{ 1, 1, 2, 2, 0, 1 },
	{ 1, 2, 0, 0, 0, 1 },
	{ 1, 2, 0, 1, 1, 1 },
	{ 1, 2, 0, 2, 0, 0 },
	{ 1, 2, 1, 0, 0, 0 },
	{ 1, 2, 1, 1, 1, 1 },
	{ 1, 2, 1, 2, 0, 0 },
	{ 1, 2, 2, 0, 0, 0 },
	{ 1, 2, 2, 1, 1, 1 },
	{ 1, 2, 2, 2, 0, 1 },
};

/*
 * Obtain the kinfo_proc2 data for the given process ID.  Return 0 on success,
 * or -1 with errno set appropriately on failure.
 */
static int
get_proc2(pid_t pid, struct kinfo_proc2 * proc2)
{
	int mib[6];
	size_t oldlen;

	/*
	 * FIXME: for performance reasons, the MIB service updates it process
	 * tables only every clock tick.  As a result, we may not be able to
	 * obtain accurate process details right away, and we need to wait.
	 * Eventually, the MIB service should retrieve more targeted subsets of
	 * the process tables, and this problem should go away at least for
	 * specific queries such as this one, which queries only a single PID.
	 */
	usleep((2000000 + sysconf(_SC_CLK_TCK)) / sysconf(_SC_CLK_TCK));

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC2;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;
	mib[4] = sizeof(*proc2);
	mib[5] = 1;

	oldlen = sizeof(*proc2);
	if (sysctl(mib, __arraycount(mib), proc2, &oldlen, NULL, 0) == -1)
		return -1;
	if (oldlen != sizeof(*proc2)) {
		errno = ESRCH;
		return -1;
	}
	return 0;
}

/*
 * Verify that the current process's real, effective, and saved user IDs are
 * set to the given respective value.
 */
static void
test_uids(uid_t ruid, uid_t euid, uid_t suid)
{
	struct kinfo_proc2 proc2;

	if (getuid() != ruid) e(0);
	if (geteuid() != euid) e(0);

	/*
	 * There is no system call specifically to retrieve the saved user ID,
	 * so we use sysctl(2) to obtain process information.  This allows us
	 * to verify the real and effective user IDs once more, too.
	 */
	if (get_proc2(getpid(), &proc2) != 0) e(0);

	if (proc2.p_ruid != ruid) e(0);
	if (proc2.p_uid != euid) e(0);
	if (proc2.p_svuid != suid) e(0);
}

/*
 * Verify that the real and effective user IDs are kept as is after an exec(2)
 * call on a non-setuid binary, and that the saved user ID is set to the
 * effective user ID.
 */
static void
exec89b(const char * param1, const char * param2 __unused)
{
	const struct uid_set *set;
	int setnum;

	setnum = atoi(param1);
	if (setnum < 0 || setnum >= __arraycount(uid_sets)) {
		e(setnum);
		return;
	}
	set = &uid_sets[setnum];

	test_uids(set->ruid, set->euid, set->euid);
}

/*
 * The real, effective, and saved user IDs have been set up as indicated by the
 * current set.  Verify that fork(2) and exec(2) do not change the real and
 * effective UIDs, and that only exec(2) sets the saved UID to the effective
 * UID.
 */
static void
sub89b(int setnum)
{
	const struct uid_set *set;
	char param1[32];
	pid_t pid;
	int status;

	set = &uid_sets[setnum];

	pid = fork();

	switch (pid) {
	case -1:
		e(setnum);
		break;

	case 0:
		/*
		 * Verify that all the UIDs were retained across the fork(2)
		 * call.
		 */
		test_uids(set->ruid, set->euid, set->suid);

		snprintf(param1, sizeof(param1), "%d", setnum);

		(void)execl(executable, executable, "DO CHECK", "b", param1,
		    "", NULL);

		e(setnum);
		break;

	default:
		if (waitpid(pid, &status, 0) != pid) e(setnum);
		if (!WIFEXITED(status)) e(setnum);
		if (WEXITSTATUS(status) != 0) e(setnum);
	}
}

/*
 * The real, effective, and saved user IDs have been set up as indicated by the
 * current set.  Test one particular case for test A or B, and verify the
 * result.
 */
static void
test_one_uid(int setnum, int sub)
{
	const struct uid_set *set;
	int res, exp;

	set = &uid_sets[setnum];

	/* Verify that the pre-call process state is as expected. */
	test_uids(set->ruid, set->euid, set->suid);

	/* Perform the call, and check whether the result is as expected. */
	switch (sub) {
	case SUB_REAL:
		res = setuid(set->uid);
		exp = set->res - 1;
		break;

	case SUB_EFF:
		res = seteuid(set->uid);
		exp = set->eres - 1;
		break;

	case SUB_RETAIN:
		sub89b(setnum);

		return;

	default:
		abort();
	}

	if (res != 0 && (res != -1 || errno != EPERM)) e(setnum);

	if (res != exp) e(setnum);

	/* Verify that the post-call process state is as expected as well. */
	if (res == 0) {
		if (sub == SUB_EFF)
			test_uids(set->ruid, set->uid, set->suid);
		else
			test_uids(set->uid, set->uid, set->uid);
	} else
		test_uids(set->ruid, set->euid, set->suid);
}

/*
 * Test setuid(2) or seteuid(2) after a successful execve(2) call, which should
 * have set the process's effective and saved user ID.
 */
static void
exec89a(const char * param1, const char * param2)
{
	const struct uid_set *set;
	int setnum, sub;

	setnum = atoi(param1);
	if (setnum < 0 || setnum >= __arraycount(uid_sets)) {
		e(setnum);
		return;
	}
	set = &uid_sets[setnum];

	sub = atoi(param2);

	if (sub == SUB_RETAIN) {
		/* Clear the set-uid bit before dropping more privileges. */
		if (chmod(executable, S_IXUSR | S_IXGRP | S_IXOTH) != 0)
			e(setnum);
	}

	/* Finish setting up the initial condition. */
	if (set->euid != set->suid) {
		if (set->euid != set->ruid && set->suid != 0) {
			test_uids(set->ruid, set->suid, set->suid);

			return; /* skip test */
		}

		if (seteuid(set->euid) != 0) e(setnum);
	}

	/* Perform the actual test. */
	test_one_uid(setnum, sub);
}

/*
 * Test setuid(2) or seteuid(2) with a certain value starting from a certain
 * initial condition, as identified by the given uid_sets[] array element.  As
 * a side effect, test that in particular exec(2) properly sets the effective
 * and saved user ID.
 */
static void
sub89a(int setnum, int sub)
{
	const struct uid_set *set;
	char param1[32], param2[32];

	set = &uid_sets[setnum];

	/*
	 * Figure out how to set the real, effective, and saved UIDs to those
	 * of the set structure.  Without setresuid(2), not all combinations
	 * are possible to achieve.  We silently skip the tests for which we
	 * cannot create the requested initial condition.
	 */
	if (set->ruid != set->suid) {
		/*
		 * In order to set the saved UID to something other than the
		 * real UID, we must exec(2) a set-uid binary.
		 */
		if (chown(executable, set->suid, 0 /*anything*/) != 0) e(0);
		if (chmod(executable,
		    S_ISUID | S_IXUSR | S_IXGRP | S_IXOTH) != 0) e(0);

		if (setuid(set->ruid) != 0) e(setnum);

		snprintf(param1, sizeof(param1), "%d", setnum);
		snprintf(param2, sizeof(param2), "%d", sub);

		(void)execl(executable, executable, "DO CHECK", "a", param1,
		    param2, NULL);

		e(0);
	} else {
		/*
		 * If the real and saved user ID are to be set to the same
		 * value, we need not use exec(2).  Still, we cannot achieve
		 * all combinations here either.
		 */
		if (set->ruid != 0 && set->ruid != set->euid)
			return; /* skip test */

		if (sub == SUB_RETAIN) {
			/* Clear the set-uid bit before dropping privileges. */
			if (chmod(executable,
			    S_IXUSR | S_IXGRP | S_IXOTH) != 0) e(setnum);
		}

		if (setuid(set->ruid) != 0) e(setnum);
		if (seteuid(set->euid) != 0) e(setnum);

		/* Perform the actual test. */
		test_one_uid(setnum, sub);
	}
}

/*
 * Test setuid(2) and seteuid(2) calls with various initial conditions, by
 * setting the real, effective, and saved UIDs to different values before
 * performing the setuid(2) or seteuid(2) call.
 */
static void
test89a(void)
{
	unsigned int setnum;
	int sub, status;
	pid_t pid;

	subtest = 1;

	for (setnum = 0; setnum < __arraycount(uid_sets); setnum++) {
		for (sub = SUB_REAL; sub <= SUB_EFF; sub++) {
			pid = fork();

			switch (pid) {
			case -1:
				e(setnum);

				break;

			case 0:
				errct = 0;

				sub89a((int)setnum, sub);

				exit(errct);
				/* NOTREACHED */

			default:
				if (waitpid(pid, &status, 0) != pid) e(setnum);
				if (!WIFEXITED(status)) e(setnum);
				if (WEXITSTATUS(status) != 0) e(setnum);
			}
		}
	}
}

/*
 * Ensure that the real, effective, and saved UIDs are fully preserved across
 * fork(2) and non-setuid-binary exec(2) calls.
 */
static void
test89b(void)
{
	unsigned int setnum;
	int status;
	pid_t pid;

	subtest = 2;

	for (setnum = 0; setnum < __arraycount(uid_sets); setnum++) {
		if (uid_sets[setnum].uid != 0)
			continue; /* no need to do the same test >1 times */

		pid = fork();

		switch (pid) {
		case -1:
			e(setnum);

			break;

		case 0:
			errct = 0;

			/*
			 * Test B uses some of the A-test code.  While rather
			 * ugly, this avoids duplication of some of test A's
			 * important UID logic.
			 */
			sub89a((int)setnum, SUB_RETAIN);

			exit(errct);
			/* NOTREACHED */

		default:
			if (waitpid(pid, &status, 0) != pid) e(setnum);
			if (!WIFEXITED(status)) e(setnum);
			if (WEXITSTATUS(status) != 0) e(setnum);
		}
	}
}

/*
 * Verify that the current process's real, effective, and saved group IDs are
 * set to the given respective value.
 */
static void
test_gids(gid_t rgid, gid_t egid, gid_t sgid)
{
	struct kinfo_proc2 proc2;

	if (getgid() != rgid) e(0);
	if (getegid() != egid) e(0);

	/* As above. */
	if (get_proc2(getpid(), &proc2) != 0) e(0);

	if (proc2.p_rgid != rgid) e(0);
	if (proc2.p_gid != egid) e(0);
	if (proc2.p_svgid != sgid) e(0);
}

/*
 * Verify that the real and effective group IDs are kept as is after an exec(2)
 * call on a non-setgid binary, and that the saved group ID is set to the
 * effective group ID.
 */
static void
exec89d(const char * param1, const char * param2 __unused)
{
	const struct gid_set *set;
	int setnum;

	setnum = atoi(param1);
	if (setnum < 0 || setnum >= __arraycount(gid_sets)) {
		e(setnum);
		return;
	}
	set = &gid_sets[setnum];

	test_gids(set->rgid, set->egid, set->egid);
}

/*
 * The real, effective, and saved group IDs have been set up as indicated by
 * the current set.  Verify that fork(2) and exec(2) do not change the real and
 * effective GID, and that only exec(2) sets the saved GID to the effective
 * GID.
 */
static void
sub89d(int setnum)
{
	const struct gid_set *set;
	char param1[32];
	pid_t pid;
	int status;

	set = &gid_sets[setnum];

	pid = fork();

	switch (pid) {
	case -1:
		e(setnum);
		break;

	case 0:
		/*
		 * Verify that all the GIDs were retained across the fork(2)
		 * call.
		 */
		test_gids(set->rgid, set->egid, set->sgid);

		/* Clear the set-gid bit. */
		if (chmod(executable, S_IXUSR | S_IXGRP | S_IXOTH) != 0)
			e(setnum);

		/* Alternate between preserving and dropping user IDs. */
		if (set->gid != 0) {
			if (setuid(3) != 0) e(setnum);
		}

		snprintf(param1, sizeof(param1), "%d", setnum);

		(void)execl(executable, executable, "DO CHECK", "d", param1,
		    "", NULL);

		e(setnum);
		break;

	default:
		if (waitpid(pid, &status, 0) != pid) e(setnum);
		if (!WIFEXITED(status)) e(setnum);
		if (WEXITSTATUS(status) != 0) e(setnum);
	}
}

/*
 * The real, effective, and saved group IDs have been set up as indicated by
 * the current set.  Test one particular case for test C or D, and verify the
 * result.
 */
static void
test_one_gid(int setnum, int sub)
{
	const struct gid_set *set;
	int res, exp;

	set = &gid_sets[setnum];

	/* Verify that the pre-call process state is as expected. */
	test_gids(set->rgid, set->egid, set->sgid);

	/* Perform the call, and check whether the result is as expected. */
	switch (sub) {
	case SUB_REAL:
	case SUB_REAL_E0:
		if (sub != SUB_REAL_E0 && seteuid(1) != 0) e(0);

		res = setgid(set->gid);
		exp = (sub != SUB_REAL_E0) ? (set->res - 1) : 0;
		break;

	case SUB_EFF:
	case SUB_EFF_E0:
		if (sub != SUB_EFF_E0 && seteuid(1) != 0) e(0);

		res = setegid(set->gid);
		exp = (sub != SUB_EFF_E0) ? (set->eres - 1) : 0;
		break;

	case SUB_RETAIN:
		sub89d(setnum);

		return;

	default:
		abort();
	}

	if (res != 0 && (res != -1 || errno != EPERM)) e(setnum);

	if (res != exp) e(setnum);

	/* Verify that the post-call process state is as expected as well. */
	if (res == 0) {
		if (sub == SUB_EFF || sub == SUB_EFF_E0)
			test_gids(set->rgid, set->gid, set->sgid);
		else
			test_gids(set->gid, set->gid, set->gid);
	} else
		test_gids(set->rgid, set->egid, set->sgid);
}

/*
 * Test setgid(2) or setegid(2) after a successful execve(2) call, which should
 * have set the process's effective and saved group ID.
 */
static void
exec89c(const char * param1, const char * param2)
{
	const struct gid_set *set;
	int setnum, sub;

	setnum = atoi(param1);
	if (setnum < 0 || setnum >= __arraycount(gid_sets)) {
		e(setnum);
		return;
	}
	set = &gid_sets[setnum];

	sub = atoi(param2);

	/* Finish setting up the initial condition. */
	if (set->egid != set->sgid && setegid(set->egid) != 0) e(setnum);

	/* Perform the actual test. */
	test_one_gid(setnum, sub);
}

/*
 * Test setgid(2) or setegid(2) with a certain value starting from a certain
 * initial condition, as identified by the given gid_sets[] array element.  As
 * a side effect, test that in particular exec(2) properly sets the effective
 * and saved group ID.
 */
static void
sub89c(int setnum, int sub)
{
	const struct gid_set *set;
	char param1[32], param2[32];

	set = &gid_sets[setnum];

	/*
	 * Figure out how to set the real, effective, and saved GIDs to those
	 * of the set structure.  In this case, all combinations are possible.
	 */
	if (set->rgid != set->sgid) {
		/*
		 * In order to set the saved GID to something other than the
		 * real GID, we must exec(2) a set-gid binary.
		 */
		if (chown(executable, 0 /*anything*/, set->sgid) != 0) e(0);
		if (chmod(executable,
		    S_ISGID | S_IXUSR | S_IXGRP | S_IXOTH) != 0) e(0);

		if (setgid(set->rgid) != 0) e(setnum);

		snprintf(param1, sizeof(param1), "%d", setnum);
		snprintf(param2, sizeof(param2), "%d", sub);

		(void)execl(executable, executable, "DO CHECK", "c", param1,
		    param2, NULL);

		e(0);
	} else {
		/*
		 * If the real and saved group ID are to be set to the same
		 * value, we need not use exec(2).
		 */
		if (setgid(set->rgid) != 0) e(setnum);
		if (setegid(set->egid) != 0) e(setnum);

		/* Perform the actual test. */
		test_one_gid(setnum, sub);
	}
}

/*
 * Test setgid(2) and setegid(2) calls with various initial conditions, by
 * setting the real, effective, and saved GIDs to different values before
 * performing the setgid(2) or setegid(2) call.  At the same time, verify that
 * if the caller has an effective UID of 0, all set(e)gid calls are allowed.
 */
static void
test89c(void)
{
	unsigned int setnum;
	int sub, status;
	pid_t pid;

	subtest = 3;

	for (setnum = 0; setnum < __arraycount(gid_sets); setnum++) {
		for (sub = SUB_REAL; sub <= SUB_EFF_E0; sub++) {
			pid = fork();

			switch (pid) {
			case -1:
				e(setnum);

				break;

			case 0:
				errct = 0;

				sub89c((int)setnum, sub);

				exit(errct);
				/* NOTREACHED */

			default:
				if (waitpid(pid, &status, 0) != pid) e(setnum);
				if (!WIFEXITED(status)) e(setnum);
				if (WEXITSTATUS(status) != 0) e(setnum);
			}
		}
	}
}

/*
 * Ensure that the real, effective, and saved GIDs are fully preserved across
 * fork(2) and non-setgid-binary exec(2) calls.
 */
static void
test89d(void)
{
	unsigned int setnum;
	int status;
	pid_t pid;

	subtest = 4;

	for (setnum = 0; setnum < __arraycount(gid_sets); setnum++) {
		if (gid_sets[setnum].gid == 2)
			continue; /* no need to do the same test >1 times */

		pid = fork();

		switch (pid) {
		case -1:
			e(setnum);

			break;

		case 0:
			errct = 0;

			/* Similarly, test D uses some of the C-test code. */
			sub89c((int)setnum, SUB_RETAIN);

			exit(errct);
			/* NOTREACHED */

		default:
			if (waitpid(pid, &status, 0) != pid) e(setnum);
			if (!WIFEXITED(status)) e(setnum);
			if (WEXITSTATUS(status) != 0) e(setnum);
		}
	}
}

/*
 * Either perform the second step of setting up user and group IDs, or check
 * whether the user and/or group IDs have indeed been changed appropriately as
 * the result of the second exec(2).
 */
static void
exec89e(const char * param1, const char * param2)
{
	int mask, step;
	mode_t mode;

	mask = atoi(param1);
	step = atoi(param2);

	if (step == 0) {
		mode = S_IXUSR | S_IXGRP | S_IXOTH;
		if (mask & 1) mode |= S_ISUID;
		if (mask & 2) mode |= S_ISGID;

		if (chown(executable, 6, 7) != 0) e(0);
		if (chmod(executable, mode) != 0) e(0);

		if (setegid(4) != 0) e(0);
		if (seteuid(2) != 0) e(0);

		test_uids(1, 2, 0);
		test_gids(3, 4, 5);

		(void)execl(executable, executable, "DO CHECK", "e", param1,
		    "1", NULL);

		e(0);
	} else {
		if (mask & 1)
			test_uids(1, 6, 6);
		else
			test_uids(1, 2, 2);

		if (mask & 2)
			test_gids(3, 7, 7);
		else
			test_gids(3, 4, 4);
	}
}

/*
 * Set up for the set-uid/set-gid execution test by initializing to different
 * real and effective user IDs.
 */
static void
sub89e(int mask)
{
	char param1[32];

	if (chown(executable, 0, 5) != 0) e(0);
	if (chmod(executable,
	    S_ISUID | S_ISGID | S_IXUSR | S_IXGRP | S_IXOTH) != 0) e(0);

	if (setgid(3) != 0) e(0);
	if (setuid(1) != 0) e(0);

	snprintf(param1, sizeof(param1), "%d", mask);
	(void)execl(executable, executable, "DO CHECK", "e", param1, "0",
	    NULL);
}

/*
 * Perform basic verification that the set-uid and set-gid bits on binaries are
 * fully independent from each other.
 */
static void
test89e(void)
{
	int mask, status;
	pid_t pid;

	subtest = 5;

	for (mask = 0; mask <= 3; mask++) {
		pid = fork();

		switch (pid) {
		case -1:
			e(0);

			break;

		case 0:
			errct = 0;

			sub89e(mask);

			exit(errct);
			/* NOTREACHED */

		default:
			if (waitpid(pid, &status, 0) != pid) e(mask);
			if (!WIFEXITED(status)) e(mask);
			if (WEXITSTATUS(status) != 0) e(mask);
		}
	}
}

/*
 * Call the right function after having executed myself.
 */
static void
exec89(const char * param0, const char * param1, const char * param2)
{

	switch (param0[0]) {
	case 'a':
		exec89a(param1, param2);
		break;

	case 'b':
		exec89b(param1, param2);
		break;

	case 'c':
		exec89c(param1, param2);
		break;

	case 'd':
		exec89d(param1, param2);
		break;

	case 'e':
		exec89e(param1, param2);
		break;

	default:
		e(0);
	}

	exit(errct);
}

/*
 * Initialize the test.
 */
static void
test89_init(void)
{
	char cp_cmd[PATH_MAX + 9];
	int status;

	subtest = 0;

	/* Reset all user and group IDs to known values. */
	if (setuid(0) != 0) e(0);
	if (setgid(0) != 0) e(0);
	if (setgroups(0, NULL) != 0) e(0);

	test_uids(0, 0, 0);
	test_gids(0, 0, 0);

	/* Make a copy of the binary, which as of start() is one level up. */
	snprintf(cp_cmd, sizeof(cp_cmd), "cp ../%s .", executable);

	status = system(cp_cmd);
	if (status < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS) e(0);
}

/*
 * Test program for set[ug]id, sete[ug]id, and saved IDs.
 */
int
main(int argc, char ** argv)
{
	int i, m;

	executable = argv[0];

	/* This test executes itself.  Handle that case first. */
	if (argc == 5 && !strcmp(argv[1], "DO CHECK"))
		exec89(argv[2], argv[3], argv[4]);

	start(89);

	test89_init();

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x01) test89a();
		if (m & 0x02) test89b();
		if (m & 0x04) test89c();
		if (m & 0x08) test89d();
		if (m & 0x10) test89e();
	}

	quit();
	/* NOTREACHED */
}
