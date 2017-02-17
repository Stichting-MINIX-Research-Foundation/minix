/* Tests for System V IPC semaphores - by D.C. van Moolenbroek */
/* This test must be run as root, as it includes permission checking tests. */
#include <stdlib.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <signal.h>

#include "common.h"

#define ITERATIONS	3

#define WAIT_USECS	100000		/* time for processes to get ready */

#define KEY_A		0x73570001
#define KEY_B		(KEY_A + 1)
#define KEY_C		(KEY_A + 2)

#define ROOT_USER	"root"		/* name of root */
#define ROOT_GROUP	"wheel"		/* name of root's group */
#define NONROOT_USER	"bin"		/* name of any unprivileged user */
#define NONROOT_GROUP	"bin"		/* name of any unprivileged group */

enum {
	DROP_NONE,
	DROP_USER,
	DROP_ALL,
};

enum {
	SUGID_NONE,
	SUGID_ROOT_USER,
	SUGID_NONROOT_USER,
	SUGID_ROOT_GROUP,
	SUGID_NONROOT_GROUP,
};

struct link {
	pid_t pid;
	int sndfd;
	int rcvfd;
};

/*
 * Test semaphore properties.  This is a macro, so that it prints useful line
 * information if an error occurs.
 */
#define TEST_SEM(id, num, val, pid, ncnt, zcnt) do { \
	if (semctl(id, num, GETVAL) != val) e(0); \
	if (pid != -1 && semctl(id, num, GETPID) != pid) e(1); \
	if (ncnt != -1 && semctl(id, num, GETNCNT) != ncnt) e(2); \
	if (zcnt != -1 && semctl(id, num, GETZCNT) != zcnt) e(3); \
} while (0);

static int nr_signals = 0;

static size_t page_size;
static char *page_ptr;
static void *bad_ptr;

/*
 * Spawn a child process, with a pair of pipes to talk to it bidirectionally.
 * Drop user and group privileges in the child process if requested.
 */
static void
spawn(struct link * link, void (* proc)(struct link *), int drop)
{
	struct passwd *pw;
	struct group *gr;
	int up[2], dn[2];

	fflush(stdout);
	fflush(stderr);

	if (pipe(up) != 0) e(0);
	if (pipe(dn) != 0) e(0);

	link->pid = fork();

	switch (link->pid) {
	case 0:
		close(up[1]);
		close(dn[0]);

		link->pid = getppid();
		link->rcvfd = up[0];
		link->sndfd = dn[1];

		errct = 0;

		switch (drop) {
		case DROP_ALL:
			if (setgroups(0, NULL) != 0) e(0);

			if ((gr = getgrnam(NONROOT_GROUP)) == NULL) e(0);

			if (setgid(gr->gr_gid) != 0) e(0);
			if (setegid(gr->gr_gid) != 0) e(0);

			/* FALLTHROUGH */
		case DROP_USER:
			if ((pw = getpwnam(NONROOT_USER)) == NULL) e(0);

			if (setuid(pw->pw_uid) != 0) e(0);
		}

		proc(link);

		/* Close our pipe FDs on exit, so that we can make zombies. */
		exit(errct);
	case -1:
		e(0);
		break;
	}

	close(up[0]);
	close(dn[1]);

	link->sndfd = up[1];
	link->rcvfd = dn[0];
}

/*
 * Wait for a child process to terminate, and clean up.
 */
static void
collect(struct link * link)
{
	int status;

	close(link->sndfd);
	close(link->rcvfd);

	if (waitpid(link->pid, &status, 0) != link->pid) e(0);

	if (!WIFEXITED(status)) e(0);
	else errct += WEXITSTATUS(status);
}

/*
 * Forcibly terminate a child process, and clean up.
 */
static void
terminate(struct link * link)
{
	int status;

	if (kill(link->pid, SIGKILL) != 0) e(0);

	close(link->sndfd);
	close(link->rcvfd);

	if (waitpid(link->pid, &status, 0) <= 0) e(0);

	if (WIFSIGNALED(status)) {
		if (WTERMSIG(status) != SIGKILL) e(0);
	} else {
		if (!WIFEXITED(status)) e(0);
		else errct += WEXITSTATUS(status);
	}
}

/*
 * Send an integer value to the child or parent.
 */
static void
snd(struct link * link, int val)
{

	if (write(link->sndfd, (void *)&val, sizeof(val)) != sizeof(val)) e(0);
}

/*
 * Receive an integer value from the child or parent, or -1 on EOF.
 */
static int
rcv(struct link * link)
{
	int r, val;

	if ((r = read(link->rcvfd, (void *)&val, sizeof(val))) == 0)
		return -1;

	if (r != sizeof(val)) e(0);

	return val;
}

/*
 * Child procedure that creates semaphore sets.
 */
static void
test_perm_child(struct link * parent)
{
	struct passwd *pw;
	struct group *gr;
	struct semid_ds semds;
	uid_t uid;
	gid_t gid;
	int mask, rmask, sugid, id[3];

	/*
	 * Repeatedly create a number of semaphores with the masks provided by
	 * the parent process.
	 */
	while ((mask = rcv(parent)) != -1) {
		rmask = rcv(parent);
		sugid = rcv(parent);

		/*
		 * Create the semaphores.  For KEY_A, if we are going to set
		 * the mode through IPC_SET anyway, start with a zero mask to
		 * check that the replaced mode is used (thus testing IPC_SET).
		 */
		if ((id[0] = semget(KEY_A, 3,
		    IPC_CREAT | IPC_EXCL |
		    ((sugid == SUGID_NONE) ? mask : 0))) == -1) e(0);
		if ((id[1] = semget(KEY_B, 3,
		    IPC_CREAT | IPC_EXCL | mask | rmask)) == -1) e(0);
		if ((id[2] = semget(KEY_C, 3,
		    IPC_CREAT | IPC_EXCL | rmask)) == -1) e(0);

		uid = geteuid();
		gid = getegid();
		if (sugid != SUGID_NONE) {
			switch (sugid) {
			case SUGID_ROOT_USER:
				if ((pw = getpwnam(ROOT_USER)) == NULL) e(0);
				uid = pw->pw_uid;
				break;
			case SUGID_NONROOT_USER:
				if ((pw = getpwnam(NONROOT_USER)) == NULL)
					e(0);
				uid = pw->pw_uid;
				break;
			case SUGID_ROOT_GROUP:
				if ((gr = getgrnam(ROOT_GROUP)) == NULL) e(0);
				gid = gr->gr_gid;
				break;
			case SUGID_NONROOT_GROUP:
				if ((gr = getgrnam(NONROOT_GROUP)) == NULL)
					e(0);
				gid = gr->gr_gid;
				break;
			}

			semds.sem_perm.uid = uid;
			semds.sem_perm.gid = gid;
			semds.sem_perm.mode = mask;
			if (semctl(id[0], 0, IPC_SET, &semds) != 0) e(0);
			semds.sem_perm.mode = mask | rmask;
			if (semctl(id[1], 0, IPC_SET, &semds) != 0) e(0);
			semds.sem_perm.mode = rmask;
			if (semctl(id[2], 0, IPC_SET, &semds) != 0) e(0);
		}

		/* Do a quick test to confirm the right privileges. */
		if (mask & IPC_R) {
			if (semctl(id[0], 0, IPC_STAT, &semds) != 0) e(0);
			if (semds.sem_perm.mode != (SEM_ALLOC | mask)) e(0);
			if (semds.sem_perm.uid != uid) e(0);
			if (semds.sem_perm.gid != gid) e(0);
			if (semds.sem_perm.cuid != geteuid()) e(0);
			if (semds.sem_perm.cgid != getegid()) e(0);
		}

		snd(parent, id[0]);
		snd(parent, id[1]);
		snd(parent, id[2]);

		/* The other child process runs here. */

		if (rcv(parent) != 0) e(0);

		/*
		 * For owner tests, the other child may already have removed
		 * the semaphore sets, so ignore return values here.
		 */
		(void)semctl(id[0], 0, IPC_RMID);
		(void)semctl(id[1], 0, IPC_RMID);
		(void)semctl(id[2], 0, IPC_RMID);
	}
}

/*
 * Perform a permission test.  The given procedure will be called for various
 * access masks, which it can use to determine whether operations on three
 * created semaphore sets should succeed or fail.  The first two semaphore sets
 * are created with appropriate privileges, the third one is not.  If the
 * 'owner_test' variable is set, the test will change slightly so as to allow
 * testing of operations that require a matching uid/cuid.
 */
static void
test_perm(void (* proc)(struct link *), int owner_test)
{
	struct link child1, child2;
	int n, shift, bit, mask, rmask, drop1, drop2, sugid, id[3];

	for (n = 0; n < 7; n++) {
		/*
		 * Child 1 creates the semaphores, and child 2 opens them.
		 * For shift 6 (0700), child 1 drops its privileges to match
		 * child 2's (n=0).  For shift 3 (0070), child 2 drops its user
		 * privileges (n=3).  For shift 0 (0007), child 2 drops its
		 * group in addition to its user privileges (n=6).  Also try
		 * with differing uid/cuid (n=1,2) and gid/cgid (n=4,5), where
		 * the current ownership (n=1,4) or the creator's ownership
		 * (n=2,5) is tested.
		 */
		switch (n) {
		case 0:
			shift = 6;
			drop1 = DROP_ALL;
			drop2 = DROP_ALL;
			sugid = SUGID_NONE;
			break;
		case 1:
			shift = 6;
			drop1 = DROP_NONE;
			drop2 = DROP_ALL;
			sugid = SUGID_NONROOT_USER;
			break;
		case 2:
			shift = 6;
			drop1 = DROP_USER;
			drop2 = DROP_ALL;
			sugid = SUGID_ROOT_USER;
			break;
		case 3:
			shift = 3;
			drop1 = DROP_NONE;
			drop2 = DROP_USER;
			sugid = SUGID_NONE;
			break;
		case 4:
			shift = 3;
			drop1 = DROP_NONE;
			drop2 = DROP_ALL;
			sugid = SUGID_NONROOT_GROUP;
			break;
		case 5:
			/* The root group has no special privileges. */
			shift = 3;
			drop1 = DROP_NONE;
			drop2 = DROP_USER;
			sugid = SUGID_NONROOT_GROUP;
			break;
		case 6:
			shift = 0;
			drop1 = DROP_NONE;
			drop2 = DROP_ALL;
			sugid = SUGID_NONE;
			break;
		}

		spawn(&child1, test_perm_child, drop1);
		spawn(&child2, proc, drop2);

		for (bit = 0; bit <= 7; bit++) {
			mask = bit << shift;
			rmask = 0777 & ~(7 << shift);

			snd(&child1, mask);
			snd(&child1, rmask);
			snd(&child1, sugid);
			id[0] = rcv(&child1);
			id[1] = rcv(&child1);
			id[2] = rcv(&child1);

			snd(&child2, (owner_test) ? shift : bit);
			snd(&child2, id[0]);
			snd(&child2, id[1]);
			snd(&child2, id[2]);
			if (rcv(&child2) != 0) e(0);

			snd(&child1, 0);
		}

		/* We use a bitmask of -1 to terminate the children. */
		snd(&child1, -1);
		snd(&child2, -1);

		collect(&child1);
		collect(&child2);
	}
}

/*
 * Test semget(2) permission checks.  Please note that the checks are advisory:
 * nothing keeps a process from opening a semaphore set with fewer privileges
 * than required by the operations the process subsequently issues on the set.
 */
static void
test88a_perm(struct link * parent)
{
	int r, tbit, bit, mask, id[3];

	while ((tbit = rcv(parent)) != -1) {
		id[0] = rcv(parent);
		id[1] = rcv(parent);
		id[2] = rcv(parent);

		/*
		 * We skip setting lower bits, as it is not clear what effect
		 * that should have.  We assume that zero bits should result in
		 * failure.
		 */
		for (bit = 0; bit <= 7; bit++) {
			mask = bit << 6;

			/*
			 * Opening semaphore set A must succeed iff the given
			 * bits are all set in the relevant three-bit section
			 * of the creation mask.
			 */
			r = semget(KEY_A, 0, mask);
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if ((bit != 0 && (bit & tbit) == bit) != (r != -1))
				e(0);
			if (r != -1 && r != id[0]) e(0);

			/*
			 * Same for semaphore set B, which was created with all
			 * irrelevant mode bits inverted.
			 */
			r = semget(KEY_B, 0, mask);
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if ((bit != 0 && (bit & tbit) == bit) != (r != -1))
				e(0);
			if (r != -1 && r != id[1]) e(0);

			/*
			 * Semaphore set C was created with only irrelevant
			 * mode bits set, so opening it must always fail.
			 */
			if (semget(KEY_C, 0, mask) != -1) e(0);
			if (errno != EACCES) e(0);
		}

		snd(parent, 0);
	}
}

/*
 * Test the basic semget(2) functionality.
 */
static void
test88a(void)
{
	struct seminfo seminfo;
	struct semid_ds semds;
	time_t now;
	unsigned int i, j;
	int id[3], *idp;

	subtest = 0;

	/*
	 * The key IPC_PRIVATE must always yield a new semaphore set identifier
	 * regardless of whether IPC_CREAT and IPC_EXCL are supplied.
	 */
	if ((id[0] = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600)) < 0) e(0);

	if ((id[1] = semget(IPC_PRIVATE, 1, IPC_CREAT | IPC_EXCL | 0600)) < 0)
		e(0);

	if ((id[2] = semget(IPC_PRIVATE, 1, 0600)) < 0) e(0);

	if (id[0] == id[1]) e(0);
	if (id[1] == id[2]) e(0);
	if (id[0] == id[2]) e(0);

	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);
	if (semctl(id[1], 0, IPC_RMID) != 0) e(0);
	if (semctl(id[2], 0, IPC_RMID) != 0) e(0);

	/* Remove any leftovers from previous test runs. */
	if ((id[0] = semget(KEY_A, 0, 0600)) >= 0 &&
	    semctl(id[0], 0, IPC_RMID) == -1) e(0);
	if ((id[0] = semget(KEY_B, 0, 0600)) >= 0 &&
	    semctl(id[0], 0, IPC_RMID) == -1) e(0);

	/*
	 * For non-IPC_PRIVATE keys, open(2)-like semantics apply with respect
	 * to IPC_CREAT and IPC_EXCL flags.  The behavior of supplying IPC_EXCL
	 * without IPC_CREAT is undefined, so we do not test for that here.
	 */
	if (semget(KEY_A, 1, 0600) != -1) e(0);
	if (errno != ENOENT);

	if ((id[0] = semget(KEY_A, 1, IPC_CREAT | IPC_EXCL | 0600)) < 0) e(0);

	if (semget(KEY_B, 1, 0600) != -1) e(0);
	if (errno != ENOENT);

	if ((id[1] = semget(KEY_B, 1, IPC_CREAT | 0600)) < 0) e(0);

	if (id[0] == id[1]) e(0);

	if ((id[2] = semget(KEY_A, 1, 0600)) < 0) e(0);
	if (id[2] != id[0]) e(0);

	if ((id[2] = semget(KEY_B, 1, IPC_CREAT | 0600)) < 0) e(0);
	if (id[2] != id[2]) e(0);

	if (semget(KEY_A, 1, IPC_CREAT | IPC_EXCL | 0600) != -1) e(0);
	if (errno != EEXIST) e(0);

	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);
	if (semctl(id[1], 0, IPC_RMID) != 0) e(0);

	/*
	 * Check that we get the right error when we run out of semaphore sets.
	 * It is possible that other processes in the system are using sets
	 * right now, so see if we can anywhere from three (the number we had
	 * already) to SEMMNI semaphore sets, and check for ENOSPC after that.
	 */
	if (semctl(0, 0, IPC_INFO, &seminfo) == -1) e(0);
	if (seminfo.semmni < 3 || seminfo.semmni > USHRT_MAX) e(0);

	if ((idp = malloc(sizeof(int) * (seminfo.semmni + 1))) == NULL) e(0);

	for (i = 0; i < seminfo.semmni + 1; i++) {
		if ((idp[i] = semget(KEY_A + i, 1, IPC_CREAT | 0600)) < 0)
			break;

		/* Ensure that there are no ID collisions.  O(n**2). */
		for (j = 0; j < i; j++)
			if (idp[i] == idp[j]) e(0);
	}

	if (errno != ENOSPC) e(0);
	if (i < 3) e(0);
	if (i == seminfo.semmni + 1) e(0);

	while (i-- > 0)
		if (semctl(idp[i], 0, IPC_RMID) != 0) e(0);

	free(idp);

	/*
	 * The given number of semaphores must be within bounds.
	 */
	if (semget(KEY_A, -1, IPC_CREAT | 0600) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semget(KEY_A, 0, IPC_CREAT | 0600) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (seminfo.semmsl < 3 || seminfo.semmsl > USHRT_MAX) e(0);
	if (semget(KEY_A, seminfo.semmsl + 1, IPC_CREAT | 0600) != -1) e(0);
	if (errno != EINVAL) e(0);

	if ((id[0] = semget(KEY_A, seminfo.semmsl, IPC_CREAT | 0600)) < 0)
		e(0);
	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);

	if ((id[0] = semget(KEY_A, 2, IPC_CREAT | 0600)) < 0) e(0);

	if ((id[1] = semget(KEY_A, 0, 0600)) < 0) e(0);
	if (id[0] != id[1]) e(0);

	if ((id[1] = semget(KEY_A, 1, 0600)) < 0) e(0);
	if (id[0] != id[1]) e(0);

	if ((id[1] = semget(KEY_A, 2, 0600)) < 0) e(0);
	if (id[0] != id[1]) e(0);

	if ((id[1] = semget(KEY_A, 3, 0600)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if ((id[1] = semget(KEY_A, seminfo.semmsl + 1, 0600)) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);

	/*
	 * Verify that the initial values for the semaphore set are as
	 * expected.
	 */
	time(&now);
	if (seminfo.semmns < 3 + seminfo.semmsl) e(0);
	if ((id[0] = semget(IPC_PRIVATE, 3, IPC_CREAT | IPC_EXCL | 0642)) < 0)
		e(0);
	if ((id[1] = semget(KEY_A, seminfo.semmsl, IPC_CREAT | 0613)) < 0)
		e(0);

	if (semctl(id[0], 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_perm.uid != geteuid()) e(0);
	if (semds.sem_perm.gid != getegid()) e(0);
	if (semds.sem_perm.cuid != geteuid()) e(0);
	if (semds.sem_perm.cgid != getegid()) e(0);
	if (semds.sem_perm.mode != (SEM_ALLOC | 0642)) e(0);
	if (semds.sem_perm._key != IPC_PRIVATE) e(0);
	if (semds.sem_nsems != 3) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime < now || semds.sem_ctime >= now + 10) e(0);

	for (i = 0; i < semds.sem_nsems; i++)
		TEST_SEM(id[0], i, 0, 0, 0, 0);

	if (semctl(id[1], 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_perm.uid != geteuid()) e(0);
	if (semds.sem_perm.gid != getegid()) e(0);
	if (semds.sem_perm.cuid != geteuid()) e(0);
	if (semds.sem_perm.cgid != getegid()) e(0);
	if (semds.sem_perm.mode != (SEM_ALLOC | 0613)) e(0);
	if (semds.sem_perm._key != KEY_A) e(0);
	if (semds.sem_nsems != seminfo.semmsl) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime < now || semds.sem_ctime >= now + 10) e(0);

	for (i = 0; i < semds.sem_nsems; i++)
		TEST_SEM(id[1], i, 0, 0, 0, 0);

	if (semctl(id[1], 0, IPC_RMID) != 0) e(0);
	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);

	/*
	 * Finally, perform a number of permission-related checks.  Since the
	 * main test program is running with superuser privileges, most of the
	 * permission tests use an unprivileged child process.
	 */
	/* The superuser can always open and destroy a semaphore set. */
	if ((id[0] = semget(KEY_A, 1, IPC_CREAT | IPC_EXCL | 0000)) < 0) e(0);

	if ((id[1] = semget(KEY_A, 0, 0600)) < 0) e(0);
	if (id[0] != id[1]) e(0);

	if ((id[1] = semget(KEY_A, 0, 0000)) < 0) e(0);
	if (id[0] != id[1]) e(0);

	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);

	/*
	 * When an unprivileged process tries to open a semaphore set, the
	 * given upper three permission bits from the mode (0700) are tested
	 * against the appropriate permission bits from the semaphore set.
	 */
	test_perm(test88a_perm, 0 /*owner_test*/);
}

/*
 * Test semop(2) permission checks.
 */
static void
test88b_perm(struct link * parent)
{
	struct sembuf sops[2];
	size_t nsops;
	int i, r, tbit, bit, id[3];

	while ((tbit = rcv(parent)) != -1) {
		id[0] = rcv(parent);
		id[1] = rcv(parent);
		id[2] = rcv(parent);

		/*
		 * This loop is designed such that failure of any bit-based
		 * subset will not result in subsequent operations blocking.
		 */
		for (i = 0; i < 8; i++) {
			memset(sops, 0, sizeof(sops));

			switch (i) {
			case 0:
				nsops = 1;
				bit = 4;
				break;
			case 1:
				sops[0].sem_op = 1;
				nsops = 1;
				bit = 2;
				break;
			case 2:
				sops[0].sem_op = -1;
				nsops = 1;
				bit = 2;
				break;
			case 3:
				sops[1].sem_op = 1;
				nsops = 2;
				bit = 6;
				break;
			case 4:
				sops[0].sem_num = 1;
				sops[1].sem_op = -1;
				nsops = 2;
				bit = 6;
				break;
			case 5:
				sops[1].sem_num = 1;
				nsops = 2;
				bit = 4;
				break;
			case 6:
				/*
				 * Two operations on the same semaphore.  As
				 * such, this verifies that operations are
				 * processed in array order.
				 */
				sops[0].sem_op = 1;
				sops[1].sem_op = -1;
				nsops = 2;
				bit = 2;
				break;
			case 7:
				/*
				 * Test the order of checks.  Since IPC_STAT
				 * requires read permission, it is reasonable
				 * that the check against sem_nsems be done
				 * only after the permission check as well.
				 * For this test we rewrite EFBIG to OK below.
				 */
				sops[0].sem_num = USHRT_MAX;
				nsops = 2;
				bit = 4;
				break;
			}

			r = semop(id[0], sops, nsops);
			if (i == 7 && r == -1 && errno == EFBIG) r = 0;
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if (((bit & tbit) == bit) != (r != -1)) e(0);

			r = semop(id[1], sops, nsops);
			if (i == 7 && r == -1 && errno == EFBIG) r = 0;
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if (((bit & tbit) == bit) != (r != -1)) e(0);

			if (semop(id[2], sops, nsops) != -1) e(0);
			if (errno != EACCES) e(0);
		}

		snd(parent, 0);
	}
}

/*
 * Signal handler.
 */
static void
got_signal(int sig)
{

	if (sig != SIGHUP) e(0);
	if (nr_signals != 0) e(0);
	nr_signals++;
}

/*
 * Child process for semop(2) tests, mainly testing blocking operations.
 */
static void
test88b_child(struct link * parent)
{
	struct sembuf sops[5];
	struct sigaction act;
	int id;

	id = rcv(parent);

	memset(sops, 0, sizeof(sops));
	if (semop(id, sops, 1) != 0) e(0);

	if (rcv(parent) != 1) e(0);

	sops[0].sem_op = -3;
	if (semop(id, sops, 1) != 0) e(0);

	if (rcv(parent) != 2) e(0);

	sops[0].sem_num = 2;
	sops[0].sem_op = 2;
	sops[1].sem_num = 1;
	sops[1].sem_op = -1;
	sops[2].sem_num = 0;
	sops[2].sem_op = 1;
	if (semop(id, sops, 3) != 0) e(0);

	if (rcv(parent) != 3) e(0);

	sops[0].sem_num = 1;
	sops[0].sem_op = 0;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	sops[2].sem_num = 0;
	sops[2].sem_op = 0;
	sops[3].sem_num = 2;
	sops[3].sem_op = 0;
	sops[4].sem_num = 2;
	sops[4].sem_op = 1;
	if (semop(id, sops, 5) != 0) e(0);

	if (rcv(parent) != 4) e(0);

	sops[0].sem_num = 1;
	sops[0].sem_op = -2;
	sops[1].sem_num = 2;
	sops[1].sem_op = 0;
	if (semop(id, sops, 2) != 0) e(0);

	if (rcv(parent) != 5) e(0);

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 1;
	sops[1].sem_op = -1;
	sops[1].sem_flg = IPC_NOWAIT;
	if (semop(id, sops, 2) != 0) e(0);

	if (rcv(parent) != 6) e(0);

	sops[0].sem_num = 1;
	sops[0].sem_op = 0;
	sops[1].sem_num = 0;
	sops[1].sem_op = 0;
	sops[1].sem_flg = IPC_NOWAIT;
	if (semop(id, sops, 2) != -1) e(0);
	if (errno != EAGAIN) e(0);

	if (rcv(parent) != 7) e(0);

	sops[0].sem_num = 0;
	sops[0].sem_op = 0;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	sops[1].sem_flg = 0;
	if (semop(id, sops, 2) != 0) e(0);

	if (rcv(parent) != 8) e(0);

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 2;
	if (semop(id, sops, 2) != -1) e(0);
	if (errno != ERANGE) e(0);

	memset(&act, 0, sizeof(act));
	act.sa_handler = got_signal;
	sigfillset(&act.sa_mask);
	if (sigaction(SIGHUP, &act, NULL) != 0) e(0);

	if (rcv(parent) != 9) e(0);

	memset(sops, 0, sizeof(sops));
	sops[0].sem_num = 0;
	sops[0].sem_op = 0;
	sops[1].sem_num = 0;
	sops[1].sem_op = 1;
	sops[2].sem_num = 1;
	sops[2].sem_op = 0;
	if (semop(id, sops, 3) != -1)
	if (errno != EINTR) e(0);
	if (nr_signals != 1) e(0);

	TEST_SEM(id, 0, 0, parent->pid, 0, 0);
	TEST_SEM(id, 1, 1, parent->pid, 0, 0);

	if (rcv(parent) != 10) e(0);

	memset(sops, 0, sizeof(sops));
	sops[0].sem_op = -3;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != EIDRM) e(0);

	id = rcv(parent);

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	if (semop(id, sops, 2) != -1) e(0);
	if (errno != ERANGE) e(0);

	if (rcv(parent) != 11) e(0);

	sops[0].sem_num = 1;
	sops[0].sem_op = 0;
	sops[1].sem_num = 0;
	sops[1].sem_op = -1;
	if (semop(id, sops, 2) != 0) e(0);

	id = rcv(parent);

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 0;
	if (semop(id, sops, 2) != 0) e(0);

	snd(parent, errct);
	if (rcv(parent) != 12) e(0);

	/* The child will be killed during this call.  It should not return. */
	sops[0].sem_num = 1;
	sops[0].sem_op = -1;
	sops[1].sem_num = 0;
	sops[1].sem_op = 3;
	(void)semop(id, sops, 2);

	e(0);
}

/*
 * Test the basic semop(2) functionality.
 */
static void
test88b(void)
{
	struct seminfo seminfo;
	struct semid_ds semds;
	struct sembuf *sops, *sops2;
	size_t size;
	struct link child;
	time_t now;
	unsigned short val[2];
	int id;

	subtest = 1;

	/* Allocate a buffer for operations. */
	if (semctl(0, 0, IPC_INFO, &seminfo) == -1) e(0);

	if (seminfo.semopm < 3 || seminfo.semopm > USHRT_MAX) e(0);

	size = sizeof(sops[0]) * (seminfo.semopm + 1);
	if ((sops = malloc(size)) == NULL) e(0);
	memset(sops, 0, size);

	/* Do a few first tests with a set containing one semaphore. */
	if ((id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600)) == -1) e(0);

	/* If no operations are given, the call should succeed. */
	if (semop(id, NULL, 0) != 0) e(0);

	/*
	 * If any operations are given, the pointer must be valid.  Moreover,
	 * partially valid buffers must never be processed partially.
	 */
	if (semop(id, NULL, 1) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semop(id, bad_ptr, 1) != -1) e(0);
	if (errno != EFAULT) e(0);

	memset(page_ptr, 0, page_size);
	sops2 = ((struct sembuf *)bad_ptr) - 1;
	sops2->sem_op = 1;
	if (semop(id, sops2, 2) != -1) e(0);
	if (errno != EFAULT) e(0);

	TEST_SEM(id, 0, 0, 0, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);

	/*
	 * A new semaphore set is initialized to an all-zeroes state, and a
	 * zeroed operation tests for a zeroed semaphore.  This should pass.
	 */
	time(&now);
	if (semop(id, sops, 1) != 0) e(0);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime < now || semds.sem_otime >= now + 10) e(0);

	/* Test the limit on the number of operations. */
	if (semop(id, sops, seminfo.semopm) != 0) e(0);

	if (semop(id, sops, seminfo.semopm + 1) != -1) e(0);
	if (errno != E2BIG) e(0);

	if (semop(id, sops, SIZE_MAX) != -1) e(0);
	if (errno != E2BIG) e(0);

	/* Test the range check on the semaphore numbers. */
	sops[1].sem_num = 1;
	if (semop(id, sops, 2) != -1) e(0);
	if (errno != EFBIG) e(0);

	sops[1].sem_num = USHRT_MAX;
	if (semop(id, sops, 2) != -1) e(0);
	if (errno != EFBIG) e(0);

	/*
	 * Test nonblocking operations on a single semaphore, starting with
	 * value limit and overflow cases.
	 */
	if (seminfo.semvmx < 3 || seminfo.semvmx > SHRT_MAX) e(0);

	sops[0].sem_flg = IPC_NOWAIT;

	/* This block does not trigger on MINIX3. */
	if (seminfo.semvmx < SHRT_MAX) {
		sops[0].sem_op = seminfo.semvmx + 1;
		if (semop(id, sops, 1) != -1) e(0);
		if (errno != ERANGE) e(0);
		if (semctl(id, 0, GETVAL) != 0) e(0);
	}

	sops[0].sem_op = seminfo.semvmx;
	if (semop(id, sops, 1) != 0) e(0);
	if (semctl(id, 0, GETVAL) != seminfo.semvmx) e(0);

	/* As of writing, the proper checks for this is missing on NetBSD. */
	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != ERANGE) e(0);
	if (semctl(id, 0, GETVAL) != seminfo.semvmx) e(0);

	sops[0].sem_op = seminfo.semvmx;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != ERANGE) e(0);
	if (semctl(id, 0, GETVAL) != seminfo.semvmx) e(0);

	sops[0].sem_op = SHRT_MAX;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != ERANGE) e(0);
	if (semctl(id, 0, GETVAL) != seminfo.semvmx) e(0);

	/* This block does trigger on MINIX3. */
	if (seminfo.semvmx < -(int)SHRT_MIN) {
		sops[0].sem_op = -seminfo.semvmx - 1;
		if (semop(id, sops, 1) != -1) e(0);
		if (errno != EAGAIN) e(0);
		if (semctl(id, 0, GETVAL) != seminfo.semvmx) e(0);
	}

	sops[0].sem_op = -seminfo.semvmx;
	if (semop(id, sops, 1) != 0) e(0);
	if (semctl(id, 0, GETVAL) != 0) e(0);

	/*
	 * Test basic nonblocking operations on a single semaphore.
	 */
	sops[0].sem_op = 0;
	if (semop(id, sops, 1) != 0) e(0);

	sops[0].sem_op = 2;
	if (semop(id, sops, 1) != 0) e(0);
	if (semctl(id, 0, GETVAL) != 2) e(0);

	sops[0].sem_op = 0;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != EAGAIN) e(0);

	sops[0].sem_op = -3;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != EAGAIN) e(0);

	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);
	if (semctl(id, 0, GETVAL) != 3) e(0);

	sops[0].sem_op = -1;
	if (semop(id, sops, 1) != 0) e(0);
	if (semctl(id, 0, GETVAL) != 2) e(0);

	sops[0].sem_op = 0;
	if (semop(id, sops, 1) != -1) e(0);
	if (errno != EAGAIN) e(0);

	sops[0].sem_op = -2;
	if (semop(id, sops, 1) != 0) e(0);
	if (semctl(id, 0, GETVAL) != 0) e(0);

	sops[0].sem_op = 0;
	if (semop(id, sops, 1) != 0) e(0);

	/* Make sure that not too much data is being read in. */
	sops2->sem_op = 0;
	sops2--;
	if (semop(id, sops2, 2) != 0) e(0);

	/* Even if no operations are given, the identifier must be valid. */
	if (semctl(id, 0, IPC_RMID) != 0) e(0);

	if (semop(id, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semop(-1, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semop(INT_MIN, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	memset(&semds, 0, sizeof(semds));
	id = IXSEQ_TO_IPCID(seminfo.semmni, semds.sem_perm);
	if (semop(id, NULL, 0) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * Test permission checks.  As part of this, test basic nonblocking
	 * multi-operation calls, including operation processing in array order
	 * and the order of (permission vs other) checks.
	 */
	test_perm(test88b_perm, 0 /*owner_test*/);

	/*
	 * Test blocking operations, starting with a single blocking operation.
	 */
	if ((id = semget(IPC_PRIVATE, 3, 0600)) == -1) e(0);

	memset(sops, 0, sizeof(sops[0]));
	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);

	TEST_SEM(id, 0, 1, getpid(), 0, 0);

	spawn(&child, test88b_child, DROP_NONE);

	snd(&child, id);

	/*
	 * In various places, we have to sleep in order to allow the child to
	 * get itself blocked in a semop(2) call.
	 */
	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, getpid(), 0, 1);

	sops[0].sem_op = -1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);

	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);

	TEST_SEM(id, 0, 1, getpid(), 0, 0);

	snd(&child, 1);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, getpid(), 1, 0);

	/* This should cause a (fruitless) retry of the blocking operation. */
	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 2, getpid(), 1, 0);

	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);

	/*
	 * Test blocking operations, verifying the correct operation of
	 * multiple (partially) blocking operations and atomicity.
	 */
	memset(sops, 0, sizeof(sops[0]) * 2);
	if (semop(id, sops, 1) != 0) e(0);

	/* One blocking operation. */
	snd(&child, 2);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	TEST_SEM(id, 1, 0, 0, 1, 0);
	TEST_SEM(id, 2, 0, 0, 0, 0);

	sops[0].sem_num = 1;
	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, child.pid, 0, 0);
	TEST_SEM(id, 1, 0, child.pid, 0, 0);
	TEST_SEM(id, 2, 2, child.pid, 0, 0);

	/* Two blocking operations in one call, resolved at once. */
	snd(&child, 3);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, child.pid, 0, 1);
	TEST_SEM(id, 1, 0, child.pid, 0, 0);
	TEST_SEM(id, 2, 2, child.pid, 0, 0);

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 2;
	sops[1].sem_op = -2;
	if (semop(id, sops, 2) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, 1, child.pid, 0, 0);
	TEST_SEM(id, 2, 1, child.pid, 0, 0);

	/* Two blocking operations in one call, resolved one by one. */
	snd(&child, 4);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, 1, child.pid, 1, 0);
	TEST_SEM(id, 2, 1, child.pid, 0, 0);

	sops[0].sem_num = 1;
	sops[0].sem_op = 1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, 2, getpid(), 0, 0);
	TEST_SEM(id, 2, 1, child.pid, 0, 1);

	sops[0].sem_num = 2;
	sops[0].sem_op = -1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, 0, child.pid, 0, 0);
	TEST_SEM(id, 2, 0, child.pid, 0, 0);

	/* One blocking op followed by a nonblocking one, cleared at once. */
	sops[0].sem_num = 0;
	sops[0].sem_op = 0;
	sops[1].sem_num = 1;
	sops[1].sem_op = 0;
	if (semop(id, sops, 2) != 0) e(0);

	snd(&child, 5);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, getpid(), 1, 0);
	TEST_SEM(id, 1, 0, getpid(), 0, 0);

	sops[0].sem_num = 0;
	sops[0].sem_op = 1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	if (semop(id, sops, 2) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, 0, child.pid, 0, 0);

	/* One blocking op followed by a nonblocking one, only one cleared. */
	sops[0].sem_num = 0;
	sops[0].sem_op = 1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	if (semop(id, sops, 2) != 0) e(0);

	snd(&child, 6);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, getpid(), 0, 0);
	TEST_SEM(id, 1, 1, getpid(), 0, 1);

	sops[0].sem_num = 1;
	sops[0].sem_op = -1;
	if (semop(id, sops, 1) != 0) e(0);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, getpid(), 0, 0);
	TEST_SEM(id, 1, 0, getpid(), 0, 0);

	/*
	 * Ensure that all semaphore numbers are checked immediately, which
	 * given the earlier test results also implies that permissions are
	 * checked immediately (so we don't have to recheck that too).  We do
	 * not check whether permissions are rechecked after a blocking
	 * operation, because the specification does not describe the intended
	 * behavior on this point.
	 */
	sops[0].sem_num = 0;
	sops[0].sem_op = 0;
	sops[1].sem_num = 4;
	sops[1].sem_op = 0;
	if (semop(id, sops, 2) != -1) e(0);
	if (errno != EFBIG) e(0);

	/*
	 * Ensure that semaphore value overflow is detected properly, at the
	 * moment that the operation is actually processed.
	 */
	sops[0].sem_num = 1;
	sops[0].sem_op = seminfo.semvmx;
	if (semop(id, sops, 1) != 0) e(0);

	snd(&child, 7);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 1, getpid(), 0, 1);
	TEST_SEM(id, 1, seminfo.semvmx, getpid(), 0, 0);

	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 1;
	sops[1].sem_op = -1;
	if (semop(id, sops, 2) != 0) e(0);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, seminfo.semvmx, child.pid, 0, 0);

	sops[0].sem_num = 1;
	sops[0].sem_op = -2;
	if (semop(id, sops, 1) != 0) e(0);

	snd(&child, 8);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, child.pid, 1, 0);
	TEST_SEM(id, 1, seminfo.semvmx - 2, getpid(), 0, 0);

	sops[0].sem_num = 0;
	sops[0].sem_op = 1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	if (semop(id, sops, 2) != 0) e(0);

	TEST_SEM(id, 0, 1, getpid(), 0, 0);
	TEST_SEM(id, 1, seminfo.semvmx - 1, getpid(), 0, 0);

	sops[0].sem_num = 0;
	sops[0].sem_op = seminfo.semvmx - 1;
	sops[1].sem_num = 0;
	sops[1].sem_op = seminfo.semvmx - 1;
	sops[2].sem_num = 0;
	sops[2].sem_op = 2;
	/*
	 * With the current SEMVMX, the sum of the values is now USHRT_MAX-1,
	 * which if processed could result in a zero semaphore value.  That
	 * should not happen.  Looking at you, NetBSD.
	 */
	if (semop(id, sops, 3) != -1) e(0);
	if (errno != ERANGE) e(0);

	TEST_SEM(id, 0, 1, getpid(), 0, 0);

	/*
	 * Check that a blocking semop(2) call fails with EINTR if a signal is
	 * caught by the process after the call has blocked.
	 */
	if (semctl(id, 1, SETVAL, 0) != 0) e(0);
	sops[0].sem_num = 0;
	sops[0].sem_op = -1;
	sops[1].sem_num = 1;
	sops[1].sem_op = 1;
	if (semop(id, sops, 2) != 0) e(0);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	TEST_SEM(id, 1, 1, getpid(), 0, 0);

	snd(&child, 9);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	TEST_SEM(id, 1, 1, getpid(), 0, 1);

	kill(child.pid, SIGHUP);
	/*
	 * Kills are not guaranteed to be delivered immediately to processes
	 * other than the caller of kill(2), so let the child perform checks.
	 */

	/*
	 * Check that a blocking semop(2) call fails with EIDRM if the
	 * semaphore set is removed after the call has blocked.
	 */
	snd(&child, 10);

	usleep(WAIT_USECS);

	if (semctl(id, 0, IPC_RMID) != 0) e(0);

	/*
	 * Check if sem_otime is updated correctly.  Instead of sleeping for
	 * whole seconds so as to be able to detect differences, use SETVAL,
	 * which does not update sem_otime at all.  This doubles as a first
	 * test to see if SETVAL correctly wakes up a blocked semop(2) call.
	 */
	if ((id = semget(IPC_PRIVATE, 2, 0600)) == -1) e(0);

	snd(&child, id);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, 0, 1, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);

	if (semctl(id, 1, SETVAL, seminfo.semvmx) != 0) e(0);

	TEST_SEM(id, 0, 0, 0, 1, 0);
	TEST_SEM(id, 1, seminfo.semvmx, 0, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);

	if (semctl(id, 0, SETVAL, 1) != 0) e(0);
	TEST_SEM(id, 0, 1, 0, 0, 0);
	TEST_SEM(id, 1, seminfo.semvmx, 0, 0, 0);

	if (semctl(id, 0, SETVAL, 0) != 0) e(0);

	snd(&child, 11);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, 0, 0, 0);
	TEST_SEM(id, 1, seminfo.semvmx, 0, 0, 1);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);

	if (semctl(id, 1, SETVAL, 0) != 0) e(0);

	TEST_SEM(id, 0, 0, 0, 1, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);

	time(&now);
	if (semctl(id, 0, SETVAL, 2) != 0) e(0);

	TEST_SEM(id, 0, 1, child.pid, 0, 0);
	TEST_SEM(id, 1, 0, child.pid, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime < now || semds.sem_otime >= now + 10) e(0);

	if (semctl(id, 0, IPC_RMID) != 0) e(0);

	/*
	 * Perform a similar test for SETALL, ensuring that it causes an
	 * ongoing semop(2) to behave correctly.
	 */
	if ((id = semget(IPC_PRIVATE, 2, 0600)) == -1) e(0);

	snd(&child, id);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, 0, 1, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);

	val[0] = 1;
	val[1] = 1;
	if (semctl(id, 0, SETALL, val) != 0) e(0);

	TEST_SEM(id, 0, 1, 0, 0, 0);
	TEST_SEM(id, 1, 1, 0, 0, 1);

	val[0] = 0;
	val[1] = 1;
	if (semctl(id, 0, SETALL, val) != 0) e(0);

	TEST_SEM(id, 0, 0, 0, 1, 0);
	TEST_SEM(id, 1, 1, 0, 0, 0);

	val[0] = 1;
	val[1] = 1;
	if (semctl(id, 0, SETALL, val) != 0) e(0);

	TEST_SEM(id, 0, 1, 0, 0, 0);
	TEST_SEM(id, 1, 1, 0, 0, 1);

	val[0] = 0;
	val[1] = 0;
	if (semctl(id, 0, SETALL, val) != 0) e(0);

	TEST_SEM(id, 0, 0, 0, 1, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);

	time(&now);
	val[0] = 1;
	val[1] = 0;
	if (semctl(id, 0, SETALL, val) != 0) e(0);

	TEST_SEM(id, 0, 0, child.pid, 0, 0);
	TEST_SEM(id, 1, 0, child.pid, 0, 0);
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime < now || semds.sem_otime >= now + 10) e(0);

	/*
	 * Finally, ensure that if the child is killed, its blocked semop(2)
	 * call is properly cancelled.
	 */
	sops[0].sem_num = 0;
	sops[0].sem_op = 0;
	sops[1].sem_num = 1;
	sops[1].sem_op = 0;
	if (semop(id, sops, 2) != 0) e(0);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	TEST_SEM(id, 1, 0, getpid(), 0, 0);

	/* We'll be terminating the child, so let it report its errors now. */
	if (rcv(&child) != 0) e(0);

	snd(&child, 12);

	usleep(WAIT_USECS);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	TEST_SEM(id, 1, 0, getpid(), 1, 0);

	terminate(&child);

	TEST_SEM(id, 0, 0, getpid(), 0, 0);
	TEST_SEM(id, 1, 0, getpid(), 0, 0);

	if (semctl(id, 0, IPC_RMID) != 0) e(0);

	free(sops);
}

/*
 * Test semctl(2) permission checks, part 1: regular commands.
 */
static void
test88c_perm1(struct link * parent)
{
	static const int cmds[] = { GETVAL, GETPID, GETNCNT, GETZCNT };
	struct semid_ds semds;
	struct seminfo seminfo;
	unsigned short val[3];
	int i, r, tbit, bit, id[3], cmd;
	void *ptr;

	while ((tbit = rcv(parent)) != -1) {
		id[0] = rcv(parent);
		id[1] = rcv(parent);
		id[2] = rcv(parent);

		/* First the read-only, no-argument cases. */
		bit = 4;
		for (i = 0; i < __arraycount(cmds); i++) {
			r = semctl(id[0], 0, cmds[i]);
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if (((bit & tbit) == bit) != (r != -1)) e(0);

			r = semctl(id[1], 0, cmds[i]);
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if (((bit & tbit) == bit) != (r != -1)) e(0);

			if (semctl(id[2], 0, cmds[i]) != -1) e(0);
			if (errno != EACCES) e(0);
		}

		/*
		 * Then SETVAL, which requires write permission and is the only
		 * one that takes an integer argument.
		 */
		bit = 2;
		r = semctl(id[0], 0, SETVAL, 0);
		if (r < 0 && (r != -1 || errno != EACCES)) e(0);
		if (((bit & tbit) == bit) != (r != -1)) e(0);

		r = semctl(id[1], 0, SETVAL, 0);
		if (r < 0 && (r != -1 || errno != EACCES)) e(0);
		if (((bit & tbit) == bit) != (r != -1)) e(0);

		if (semctl(id[2], 0, SETVAL, 0) != -1) e(0);
		if (errno != EACCES) e(0);

		/*
		 * Finally the commands that require read or write permission
		 * and take a pointer as argument.
		 */
		memset(val, 0, sizeof(val));

		for (i = 0; i < 3; i++) {
			switch (i) {
			case 0:
				cmd = GETALL;
				ptr = val;
				bit = 4;
				break;
			case 1:
				cmd = SETALL;
				ptr = val;
				bit = 2;
				break;
			case 2:
				cmd = IPC_STAT;
				ptr = &semds;
				bit = 4;
				break;
			default:
				abort();
			}

			r = semctl(id[0], 0, cmd, ptr);
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if (((bit & tbit) == bit) != (r != -1)) e(0);

			r = semctl(id[1], 0, cmd, ptr);
			if (r < 0 && (r != -1 || errno != EACCES)) e(0);
			if (((bit & tbit) == bit) != (r != -1)) e(0);

			if (semctl(id[2], 0, cmd, ptr) != -1) e(0);
			if (errno != EACCES) e(0);
		}

		/*
		 * I was hoping to avoid this, but otherwise we have to make
		 * the other child iterate through all semaphore sets to find
		 * the right index for each of the identifiers.  As noted in
		 * the IPC server itself as well, duplicating these macros is
		 * not a big deal since the split is firmly hardcoded through
		 * the exposure of IXSEQ_TO_IPCID to userland.
		 */
#ifndef IPCID_TO_IX
#define IPCID_TO_IX(id)		((id) & 0xffff)
#endif

		bit = 4;

		r = semctl(IPCID_TO_IX(id[0]), 0, SEM_STAT, &semds);
		if (r < 0 && (r != -1 || errno != EACCES)) e(0);
		if (((bit & tbit) == bit) != (r != -1)) e(0);

		r = semctl(IPCID_TO_IX(id[1]), 0, SEM_STAT, &semds);
		if (r < 0 && (r != -1 || errno != EACCES)) e(0);
		if (((bit & tbit) == bit) != (r != -1)) e(0);

		if (semctl(IPCID_TO_IX(id[2]), 0, SEM_STAT, &semds) != -1)
			e(0);
		if (errno != EACCES) e(0);

		/*
		 * IPC_INFO and SEM_INFO should always succeed.  They do not
		 * even take a semaphore set identifier.
		 */
		if (semctl(0, 0, IPC_INFO, &seminfo) == -1) e(0);
		if (semctl(0, 0, SEM_INFO, &seminfo) == -1) e(0);

		snd(parent, 0);
	}
}

/*
 * Test semctl(2) permission checks, part 2: the IPC_SET command.
 */
static void
test88c_perm2(struct link * parent)
{
	struct semid_ds semds;
	int r, shift, id[3];

	while ((shift = rcv(parent)) != -1) {
		id[0] = rcv(parent);
		id[1] = rcv(parent);
		id[2] = rcv(parent);

		/*
		 * Test IPC_SET.  Ideally, we would set the permissions to what
		 * they currently are, but we do not actually know what they
		 * are, and IPC_STAT requires read permission which we may not
		 * have!  However, no matter what we do, we cannot prevent the
		 * other child from being able to remove the semaphore sets
		 * afterwards.  So, we just set the permissions to all-zeroes;
		 * even though those values are meaningful (mode 0000, uid 0,
		 * gid 0) they could be anything: the API will accept anything.
		 * This does mean we need to test IPC_RMID permissions from
		 * another procedure, because we may now be locking ourselves
		 * out.  The System V IPC interface is pretty strange that way.
		 */
		memset(&semds, 0, sizeof(semds));

		r = semctl(id[0], 0, IPC_SET, &semds);
		if (r < 0 && (r != -1 || errno != EPERM)) e(0);
		if ((shift == 6) != (r != -1)) e(0);

		r = semctl(id[1], 0, IPC_SET, &semds);
		if (r < 0 && (r != -1 || errno != EPERM)) e(0);
		if ((shift == 6) != (r != -1)) e(0);

		/* For once, this too should succeed. */
		r = semctl(id[2], 0, IPC_SET, &semds);
		if (r < 0 && (r != -1 || errno != EPERM)) e(0);
		if ((shift == 6) != (r != -1)) e(0);

		snd(parent, 0);
	}
}

/*
 * Test semctl(2) permission checks, part 3: the IPC_RMID command.
 */
static void
test88c_perm3(struct link * parent)
{
	int r, shift, id[3];

	while ((shift = rcv(parent)) != -1) {
		id[0] = rcv(parent);
		id[1] = rcv(parent);
		id[2] = rcv(parent);

		r = semctl(id[0], 0, IPC_RMID);
		if (r < 0 && (r != -1 || errno != EPERM)) e(0);
		if ((shift == 6) != (r != -1)) e(0);

		r = semctl(id[1], 0, IPC_RMID);
		if (r < 0 && (r != -1 || errno != EPERM)) e(0);
		if ((shift == 6) != (r != -1)) e(0);

		/* Okay, twice then. */
		r = semctl(id[2], 0, IPC_RMID);
		if (r < 0 && (r != -1 || errno != EPERM)) e(0);
		if ((shift == 6) != (r != -1)) e(0);

		snd(parent, 0);
	}
}

/*
 * Test the basic semctl(2) functionality.
 */
static void
test88c(void)
{
	static const int cmds[] = { GETVAL, GETPID, GETNCNT, GETZCNT };
	struct seminfo seminfo;
	struct semid_ds semds, osemds;
	unsigned short val[4], seen[2];
	char statbuf[sizeof(struct semid_ds) + 1];
	unsigned int i, j;
	time_t now;
	int r, id, id2, badid1, badid2, cmd;

	subtest = 2;

	if (semctl(0, 0, IPC_INFO, &seminfo) == -1) e(0);

	/*
	 * Start with permission checks on the commands.  IPC_SET and IPC_RMID
	 * are special: they check for ownership (uid/cuid) and return EPERM
	 * rather than EACCES on permission failure.
	 */
	test_perm(test88c_perm1, 0 /*owner_test*/);
	test_perm(test88c_perm2, 1 /*owner_test*/);
	test_perm(test88c_perm3, 1 /*owner_test*/);

	/* Create identifiers known to be invalid. */
	if ((badid1 = semget(IPC_PRIVATE, 1, 0600)) < 0) e(0);

	if (semctl(badid1, 0, IPC_RMID) != 0) e(0);

	memset(&semds, 0, sizeof(semds));
	badid2 = IXSEQ_TO_IPCID(seminfo.semmni, semds.sem_perm);

	if ((id = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) < 0) e(0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime == 0) e(0);

	/* In this case we can't avoid sleeping for longer periods.. */
	while (time(&now) == semds.sem_ctime)
		usleep(250000);

	/*
	 * Test the simple GET commands.  The actual functionality of these
	 * commands have already been tested thoroughly as part of the
	 * semop(2) part of the test set, so we do not repeat that here.
	 */
	for (i = 0; i < __arraycount(cmds); i++) {
		for (j = 0; j < 3; j++)
			if (semctl(id, j, cmds[i]) != 0) e(0);

		if (semctl(badid1, 0, cmds[i]) != -1) e(0);
		if (errno != EINVAL) e(0);

		if (semctl(badid2, 0, cmds[i]) != -1) e(0);
		if (errno != EINVAL) e(0);

		if (semctl(-1, 0, cmds[i]) != -1) e(0);
		if (errno != EINVAL) e(0);

		if (semctl(INT_MIN, 0, cmds[i]) != -1) e(0);
		if (errno != EINVAL) e(0);

		if (semctl(id, -1, cmds[i]) != -1) e(0);
		if (errno != EINVAL) e(0);

		if (semctl(id, 3, cmds[i]) != -1) e(0);
		if (errno != EINVAL) e(0);

		/* These commands should not update ctime or otime. */
		if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
		if (semds.sem_otime != 0) e(0);
		if (semds.sem_ctime >= now) e(0);
	}

	/*
	 * Test the GETALL command.
	 */
	/*
	 * Contrary to what the Open Group specification suggests, actual
	 * implementations agree that the semnum parameter is to be ignored for
	 * calls not involving a specific semaphore in the set.
	 */
	for (j = 0; j < 5; j++) {
		for (i = 0; i < __arraycount(val); i++)
			val[i] = USHRT_MAX;

		if (semctl(id, (int)j - 1, GETALL, val) != 0) e(0);
		for (i = 0; i < 3; i++)
			if (val[i] != 0) e(0);
		if (val[i] != USHRT_MAX) e(0);
	}

	for (i = 0; i < __arraycount(val); i++)
		val[i] = USHRT_MAX;

	if (semctl(badid1, 0, GETALL, val) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(badid2, 0, GETALL, val) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(-1, 0, GETALL, val) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(INT_MIN, 0, GETALL, val) != -1) e(0);
	if (errno != EINVAL) e(0);

	for (i = 0; i < __arraycount(val); i++)
		if (val[i] != USHRT_MAX) e(0);

	if (semctl(id, 0, GETALL, NULL) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, GETALL, bad_ptr) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, GETALL, ((unsigned short *)bad_ptr) - 2) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, GETALL, ((unsigned short *)bad_ptr) - 3) != 0) e(0);

	/* Still no change in either otime or ctime. */
	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime >= now) e(0);

	/*
	 * Test the IPC_STAT command.  This is the last command we are testing
	 * here that does not affect sem_ctime, so in order to avoid extra
	 * sleep times, we test this command first now.
	 */
	/*
	 * The basic IPC_STAT functionality has already been tested heavily as
	 * part of the semget(2) and permission tests, so we do not repeat that
	 * here.
	 */
	memset(statbuf, 0x5a, sizeof(statbuf));

	if (semctl(badid1, 0, IPC_STAT, statbuf) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(badid2, 0, IPC_STAT, statbuf) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(-1, 0, IPC_STAT, statbuf) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(INT_MIN, 0, IPC_STAT, statbuf) != -1) e(0);
	if (errno != EINVAL) e(0);

	for (i = 0; i < sizeof(statbuf); i++)
		if (statbuf[i] != 0x5a) e(0);

	if (semctl(id, 0, IPC_STAT, statbuf) != 0) e(0);

	if (statbuf[sizeof(statbuf) - 1] != 0x5a) e(0);

	if (semctl(id, 0, IPC_STAT, NULL) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, IPC_STAT, bad_ptr) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, IPC_STAT, ((struct semid_ds *)bad_ptr) - 1) != 0)
		e(0);

	if (semctl(id, -1, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime >= now) e(0);

	/*
	 * Test SEM_STAT.
	 */
	if ((id2 = semget(KEY_A, seminfo.semmsl, IPC_CREAT | 0642)) < 0) e(0);

	memset(statbuf, 0x5a, sizeof(statbuf));

	if (semctl(-1, 0, SEM_STAT, statbuf) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(seminfo.semmni, 0, SEM_STAT, statbuf) != -1) e(0);
	if (errno != EINVAL) e(0);

	for (i = 0; i < sizeof(statbuf); i++)
		if (statbuf[i] != 0x5a) e(0);

	memset(seen, 0, sizeof(seen));

	for (i = 0; i < seminfo.semmni; i++) {
		errno = 0;
		if ((r = semctl(i, i / 2 - 1, SEM_STAT, statbuf)) == -1) {
			if (errno != EINVAL) e(0);
			continue;
		}
		if (r < 0) e(0);
		memcpy(&semds, statbuf, sizeof(semds));
		if (!(semds.sem_perm.mode & SEM_ALLOC)) e(0);
		if (semds.sem_ctime == 0) e(0);
		if (IXSEQ_TO_IPCID(i, semds.sem_perm) != r) e(0);
		if (r == id) {
			seen[0]++;
			if (semds.sem_perm.mode != (SEM_ALLOC | 0600)) e(0);
			if (semds.sem_perm.uid != geteuid()) e(0);
			if (semds.sem_perm.gid != getegid()) e(0);
			if (semds.sem_perm.cuid != semds.sem_perm.uid) e(0);
			if (semds.sem_perm.cgid != semds.sem_perm.gid) e(0);
			if (semds.sem_perm._key != IPC_PRIVATE) e(0);
			if (semds.sem_nsems != 3) e(0);
			if (semds.sem_otime != 0) e(0);

			/* This is here because we need a valid index. */
			if (semctl(i, 0, SEM_STAT, NULL) != -1) e(0);
			if (errno != EFAULT) e(0);

			if (semctl(i, 0, SEM_STAT, bad_ptr) != -1) e(0);
			if (errno != EFAULT) e(0);
		} else if (r == id2) {
			seen[1]++;
			if (semds.sem_perm.mode != (SEM_ALLOC | 0642)) e(0);
			if (semds.sem_perm.uid != geteuid()) e(0);
			if (semds.sem_perm.gid != getegid()) e(0);
			if (semds.sem_perm.cuid != semds.sem_perm.uid) e(0);
			if (semds.sem_perm.cgid != semds.sem_perm.gid) e(0);
			if (semds.sem_perm._key != KEY_A) e(0);
			if (semds.sem_nsems != seminfo.semmsl) e(0);
		}
	}

	if (seen[0] != 1) e(0);
	if (seen[1] != 1) e(0);

	if (statbuf[sizeof(statbuf) - 1] != 0x5a) e(0);

	if (semctl(id, 5, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime >= now) e(0);

	/*
	 * Test SETVAL.  We start with all the failure cases, so as to be able
	 * to check that sem_ctime is not changed in those cases.
	 */
	if (semctl(badid1, 0, SETVAL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(badid2, 0, SETVAL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(-1, 0, SETVAL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(INT_MIN, 0, SETVAL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, -1, SETVAL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 3, SETVAL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, SETVAL, -1) != -1) e(0);
	if (errno != ERANGE) e(0);

	if (semctl(id, 0, SETVAL, seminfo.semvmx + 1) != -1) e(0);
	if (errno != ERANGE) e(0);

	TEST_SEM(id, 0, 0, 0, 0, 0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime >= now) e(0);

	/* Alright, there we go.. */
	if (semctl(id, 1, SETVAL, 0) != 0) e(0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime < now || semds.sem_ctime >= now + 10) e(0);

	TEST_SEM(id, 1, 0, 0, 0, 0);

	if (semctl(id, 2, SETVAL, seminfo.semvmx) != 0) e(0);

	TEST_SEM(id, 2, seminfo.semvmx, 0, 0, 0);

	if (semctl(id, 0, SETVAL, 1) != 0) e(0);

	TEST_SEM(id, 0, 1, 0, 0, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	TEST_SEM(id, 2, seminfo.semvmx, 0, 0, 0);

	if (semctl(id, 0, GETALL, val) != 0) e(0);
	if (val[0] != 1) e(0);
	if (val[1] != 0) e(0);
	if (val[2] != seminfo.semvmx) e(0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);

	while (time(&now) == semds.sem_ctime)
		usleep(250000);

	/*
	 * Test SETALL.  Same idea: failure cases first.
	 */
	if (semctl(badid1, 0, SETALL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(badid2, 0, SETALL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(-1, 0, SETALL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(INT_MIN, 0, SETALL, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	val[0] = seminfo.semvmx + 1;
	val[1] = 0;
	val[2] = 0;
	if (semctl(id, 0, SETALL, val) != -1) e(0);
	if (errno != ERANGE) e(0);

	val[0] = 0;
	val[1] = 1;
	val[2] = seminfo.semvmx + 1;
	if (semctl(id, 0, SETALL, val) != -1) e(0);
	if (errno != ERANGE) e(0);

	if (semctl(id, 0, SETALL, NULL) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, SETALL, bad_ptr) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, SETALL, ((unsigned short *)bad_ptr) - 2) != -1) e(0);
	if (errno != EFAULT) e(0);

	TEST_SEM(id, 0, 1, 0, 0, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	TEST_SEM(id, 2, seminfo.semvmx, 0, 0, 0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime >= now) e(0);

	val[0] = seminfo.semvmx;
	val[1] = 0;
	val[2] = 0;
	if (semctl(id, 0, SETALL, val) != 0) e(0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_otime != 0) e(0);
	if (semds.sem_ctime < now || semds.sem_ctime >= now + 10) e(0);

	TEST_SEM(id, 0, seminfo.semvmx, 0, 0, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	TEST_SEM(id, 2, 0, 0, 0, 0);

	val[0] = 0;
	val[1] = 1;
	val[2] = seminfo.semvmx;
	if (semctl(id, INT_MAX, SETALL, val) != 0) e(0);

	TEST_SEM(id, 0, 0, 0, 0, 0);
	TEST_SEM(id, 1, 1, 0, 0, 0);
	TEST_SEM(id, 2, seminfo.semvmx, 0, 0, 0);

	memset(page_ptr, 0, page_size);
	if (semctl(id, 0, SETALL, ((unsigned short *)bad_ptr) - 3) != 0) e(0);

	TEST_SEM(id, 0, 0, 0, 0, 0);
	TEST_SEM(id, 1, 0, 0, 0, 0);
	TEST_SEM(id, 2, 0, 0, 0, 0);

	while (time(&now) == semds.sem_ctime)
		usleep(250000);

	/*
	 * Test IPC_SET.  Its core functionality has already been tested
	 * thoroughly as part of the permission tests.
	 */
	if (semctl(badid1, 0, IPC_SET, &semds) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(badid2, 0, IPC_SET, &semds) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(-1, 0, IPC_SET, &semds) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(INT_MIN, 0, IPC_SET, &semds) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, IPC_SET, NULL) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, IPC_SET, bad_ptr) != -1) e(0);
	if (errno != EFAULT) e(0);

	if (semctl(id, 0, IPC_STAT, &osemds) != 0) e(0);
	if (osemds.sem_otime != 0) e(0);
	if (osemds.sem_ctime >= now) e(0);

	/*
	 * Only mode, uid, gid may be set.  While the given mode is sanitized
	 * in our implementation (see below; the open group specification
	 * leaves this undefined), the uid and gid are not (we do not test this
	 * exhaustively).  The other given fields must be ignored.  The ctime
	 * field will be updated.
	 */
	memset(&semds, 0x5b, sizeof(semds));
	semds.sem_perm.mode = 0712;
	semds.sem_perm.uid = UID_MAX;
	semds.sem_perm.gid = GID_MAX - 1;
	if (semctl(id, 0, IPC_SET, &semds) != 0) e(0);

	if (semctl(id, 0, IPC_STAT, &semds) != 0) e(0);
	if (semds.sem_perm.mode != (SEM_ALLOC | 0712)) e(0);
	if (semds.sem_perm.uid != UID_MAX) e(0);
	if (semds.sem_perm.gid != GID_MAX - 1) e(0);
	if (semds.sem_perm.cuid != osemds.sem_perm.cuid) e(0);
	if (semds.sem_perm.cgid != osemds.sem_perm.cgid) e(0);
	if (semds.sem_perm._seq != osemds.sem_perm._seq) e(0);
	if (semds.sem_perm._key != osemds.sem_perm._key) e(0);
	if (semds.sem_nsems != osemds.sem_nsems) e(0);
	if (semds.sem_otime != osemds.sem_otime) e(0);
	if (semds.sem_ctime < now || semds.sem_ctime >= now + 10) e(0);

	/* It should be possible to set any mode, but mask 0777 is applied. */
	semds.sem_perm.uid = osemds.sem_perm.uid;
	semds.sem_perm.gid = osemds.sem_perm.gid;
	for (i = 0; i < 0777; i++) {
		semds.sem_perm.mode = i;
		if (semctl(id, i / 2 - 1, IPC_SET, &semds) != 0) e(0);

		if (semctl(id, i / 2 - 2, IPC_STAT, &semds) != 0) e(0);
		if (semds.sem_perm.mode != (SEM_ALLOC | i)) e(0);

		semds.sem_perm.mode = ~0777 | i;
		if (semctl(id, i / 2 - 3, IPC_SET, &semds) != 0) e(0);

		if (semctl(id, i / 2 - 4, IPC_STAT, &semds) != 0) e(0);
		if (semds.sem_perm.mode != (SEM_ALLOC | i)) e(0);
	}
	if (semds.sem_perm.uid != osemds.sem_perm.uid) e(0);
	if (semds.sem_perm.gid != osemds.sem_perm.gid) e(0);

	if (semctl(id, 0, IPC_SET, ((struct semid_ds *)bad_ptr) - 1) != 0)
		e(0);

	/*
	 * Test IPC_RMID.  Its basic functionality has already been tested
	 * multiple times over, so there is not much left to do here.
	 */
	if (semctl(badid1, 0, IPC_RMID) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(badid2, 0, IPC_RMID) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(-1, 0, IPC_RMID) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(INT_MIN, 0, IPC_RMID) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, IPC_RMID) != 0) e(0);

	if (semctl(id, 0, IPC_RMID) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, IPC_STAT, &semds) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id2, 1, IPC_RMID) != 0) e(0);

	if (semctl(id2, 1, IPC_RMID) != -1) e(0);
	if (errno != EINVAL) e(0);

	/*
	 * Test IPC_INFO and SEM_INFO.  Right now, for all practical purposes,
	 * these identifiers behave pretty much the same.
	 */
	if ((id = semget(IPC_PRIVATE, 3, 0600)) == -1) e(0);
	if ((id2 = semget(IPC_PRIVATE, 1, 0600)) == -1) e(0);

	for (i = 0; i <= 1; i++) {
		cmd = (i == 0) ? IPC_INFO : SEM_INFO;

		memset(&seminfo, 0xff, sizeof(seminfo));

		if ((r = semctl(0, 0, cmd, &seminfo)) == -1) e(0);

		/*
		 * These commands return the index of the highest in-use slot
		 * in the semaphore set table.  Bad idea of course, because
		 * that means the value 0 has two potential meanings.  Since we
		 * cannot guarantee that no other running application is using
		 * semaphores, we settle for "at least" tests based on the two
		 * semaphore sets we just created.
		 */
		if (r < 1 || r >= seminfo.semmni) e(0);

		/*
		 * Many of these checks are rather basic because of missing
		 * SEM_UNDO support.  The only difference between IPC_INFO and
		 * SEM_INFO is the meaning of the semusz and semaem fields.
		 */
		if (seminfo.semmap < 0) e(0);
		if (seminfo.semmni < 3 || seminfo.semmni > USHRT_MAX) e(0);
		if (seminfo.semmns < 3 || seminfo.semmns > USHRT_MAX) e(0);
		if (seminfo.semmnu < 0) e(0);
		if (seminfo.semmsl < 3 || seminfo.semmsl > USHRT_MAX) e(0);
		if (seminfo.semopm < 3 || seminfo.semopm > USHRT_MAX) e(0);
		if (seminfo.semume < 0) e(0);
		if (cmd == SEM_INFO) {
			if (seminfo.semusz < 2) e(0);
		} else
			if (seminfo.semusz < 0) e(0);
		if (seminfo.semvmx < 3 || seminfo.semvmx > SHRT_MAX) e(0);
		if (cmd == SEM_INFO) {
			if (seminfo.semaem < 4) e(0);
		} else
			if (seminfo.semaem < 0) e(0);

		if (semctl(INT_MAX, -1, cmd, &seminfo) == -1) e(0);
		if (semctl(-1, INT_MAX, cmd, &seminfo) == -1) e(0);

		if (semctl(0, 0, cmd, NULL) != -1) e(0);
		if (errno != EFAULT) e(0);

		if (semctl(0, 0, cmd, bad_ptr) != -1) e(0);
		if (errno != EFAULT) e(0);

		if (semctl(0, 0, cmd, ((struct seminfo *)bad_ptr) - 1) == -1)
			e(0);
	}

	if (semctl(id2, 0, IPC_RMID) != 0) e(0);

	/*
	 * Finally, test invalid commands.  Well, hopefully invalid commands,
	 * anyway.
	 */
	if (semctl(id, 0, INT_MIN) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, INT_MAX) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, IPC_RMID) != 0) e(0);
}

/*
 * Test SEM_UNDO support.  Right now this functionality is missing altogether.
 * For now, we test that any attempt to use SEM_UNDO fails.
 */
static void
test88d(void)
{
	struct sembuf sop;
	int id;

	subtest = 3;

	if ((id = semget(IPC_PRIVATE, 1, 0600)) == -1) e(0);

	/*
	 * Use an all-ones (but positive) flag field.  This will include
	 * SEM_UNDO, but also tell the IPC server to report no warning.
	 */
	if (!(SHRT_MAX & SEM_UNDO)) e(0);
	sop.sem_num = 0;
	sop.sem_op = 1;
	sop.sem_flg = SHRT_MAX;
	if (semop(id, &sop, 1) != -1) e(0);
	if (errno != EINVAL) e(0);

	if (semctl(id, 0, IPC_RMID) != 0) e(0);
}

enum {
	RESUME_SEMOP,	/* use semop() to resume blocked parties */
	RESUME_SETVAL,	/* use semctl(SETVAL) to resume blocked parties */
	RESUME_SETALL,	/* use semctl(SETALL) to resume blocked parties */
	NR_RESUMES
};

enum {
	MATCH_FIRST,	/* first match completes, blocks second match */
	MATCH_SECOND,	/* first match does not complete, second match does */
	MATCH_KILL,	/* second match completes after first is aborted */
	MATCH_BOTH,	/* first and second match both complete */
	MATCH_CASCADE,	/* completed match in turn causes another match */
	MATCH_ALL,	/* a combination of the last two */
	NR_MATCHES
};

/*
 * Auxiliary child procedure.  The auxiliary children will deadlock until the
 * semaphore set is removed.
 */
static void
test88e_childaux(struct link * parent)
{
	struct sembuf sops[3];
	struct seminfo seminfo;
	int child, id, num;

	child = rcv(parent);
	id = rcv(parent);
	num = rcv(parent);

	memset(sops, 0, sizeof(sops));

	/* These operations are guaranteed to never return successfully. */
	switch (child) {
	case 1:
		sops[0].sem_num = num;
		sops[0].sem_op = 1;
		sops[1].sem_num = num;
		sops[1].sem_op = 0;
		sops[2].sem_num = 0;
		sops[2].sem_op = 1;
		break;
	case 2:
		if (semctl(0, 0, IPC_INFO, &seminfo) == -1) e(0);
		sops[0].sem_num = num;
		sops[0].sem_op = -seminfo.semvmx;
		sops[1].sem_num = num;
		sops[1].sem_op = -seminfo.semvmx;
		sops[2].sem_num = 0;
		sops[2].sem_op = 1;
		break;
	default:
		e(0);
	}

	snd(parent, 0);

	if (semop(id, sops, 3) != -1) e(0);
	if (errno != EIDRM) e(0);
}

/*
 * First child procedure.
 */
static void
test88e_child1(struct link * parent)
{
	struct sembuf sops[3];
	size_t nsops;
	int match, id, expect;

	match = rcv(parent);
	id = rcv(parent);

	/* Start off with some defaults, then refine by match type. */
	memset(sops, 0, sizeof(sops));
	sops[0].sem_num = 2;
	sops[0].sem_op = -1;
	nsops = 2;
	expect = 0;
	switch (match) {
	case MATCH_FIRST:
		sops[1].sem_num = 3;
		sops[1].sem_op = 1;
		break;
	case MATCH_SECOND:
		sops[1].sem_num = 3;
		sops[1].sem_op = -1;
		sops[2].sem_num = 0;
		sops[2].sem_op = 1;
		nsops = 3;
		expect = -1;
		break;
	case MATCH_KILL:
		sops[1].sem_num = 0;
		sops[1].sem_op = 1;
		expect = INT_MIN;
		break;
	case MATCH_BOTH:
	case MATCH_CASCADE:
	case MATCH_ALL:
		sops[1].sem_num = 3;
		sops[1].sem_op = 1;
		break;
	default:
		e(0);
	}

	snd(parent, 0);

	if (semop(id, sops, nsops) != expect) e(0);
	if (expect == -1 && errno != EIDRM) e(0);
}

/*
 * Second child procedure.
 */
static void
test88e_child2(struct link * parent)
{
	struct sembuf sops[2];
	size_t nsops;
	int match, id, expect;

	match = rcv(parent);
	id = rcv(parent);

	/* Start off with some defaults, then refine by match type. */
	memset(sops, 0, sizeof(sops));
	sops[0].sem_num = 2;
	sops[0].sem_op = -1;
	nsops = 2;
	expect = 0;
	switch (match) {
	case MATCH_FIRST:
		sops[1].sem_num = 0;
		sops[1].sem_op = 1;
		expect = -1;
		break;
	case MATCH_SECOND:
	case MATCH_KILL:
		nsops = 1;
		break;
	case MATCH_BOTH:
	case MATCH_ALL:
		sops[1].sem_num = 3;
		sops[1].sem_op = 1;
		break;
	case MATCH_CASCADE:
		sops[0].sem_num = 3;
		nsops = 1;
		break;
	default:
		e(0);
	}

	snd(parent, 0);

	if (semop(id, sops, nsops) != expect) e(0);
	if (expect == -1 && errno != EIDRM) e(0);
}

/*
 * Third child procedure.
 */
static void
test88e_child3(struct link * parent)
{
	struct sembuf sops[1];
	size_t nsops;
	int match, id;

	match = rcv(parent);
	id = rcv(parent);

	/* Things are a bit simpler here. */
	memset(sops, 0, sizeof(sops));
	nsops = 1;
	switch (match) {
	case MATCH_ALL:
		sops[0].sem_num = 3;
		sops[0].sem_op = -2;
		break;
	default:
		e(0);
	}

	snd(parent, 0);

	if (semop(id, sops, nsops) != 0) e(0);
}

/*
 * Perform one test for operations affecting multiple processes.
 */
static void
sub88e(unsigned int match, unsigned int resume, unsigned int aux)
{
	struct link aux1, aux2, child1, child2, child3;
	struct sembuf sop;
	unsigned short val[4];
	int id, inc, aux_zcnt, aux_ncnt;

	/*
	 * For this test we use one single semaphore set, with four semaphores.
	 * The first semaphore is increased in the case that an operation that
	 * should never complete does complete, and thus should stay zero.
	 * Depending on 'aux', the second or third semaphore is used by the
	 * auxiliary children (if any, also depending on 'aux') to deadlock on.
	 * The third and higher semaphores are used in the main operations.
	 */
	if ((id = semget(IPC_PRIVATE, __arraycount(val), 0666)) == -1) e(0);

	aux_zcnt = aux_ncnt = 0;

	/* Start the first auxiliary child if desired, before all others. */
	if (aux & 1) {
		spawn(&aux1, test88e_childaux, DROP_ALL);

		snd(&aux1, 1);
		snd(&aux1, id);
		snd(&aux1, (aux & 4) ? 2 : 1);

		if (rcv(&aux1) != 0) e(0);

		if (aux & 4)
			aux_zcnt++;
	}

	/* Start and configure all children for this specific match test. */
	spawn(&child1, test88e_child1, DROP_ALL);

	snd(&child1, match);
	snd(&child1, id);

	if (rcv(&child1) != 0) e(0);

	/*
	 * For fairness tests, we must ensure that the first child blocks on
	 * the semaphore before the second child does.
	 */
	switch (match) {
	case MATCH_FIRST:
	case MATCH_SECOND:
	case MATCH_KILL:
		usleep(WAIT_USECS);
		break;
	}

	spawn(&child2, test88e_child2, DROP_NONE);

	snd(&child2, match);
	snd(&child2, id);

	if (rcv(&child2) != 0) e(0);

	if (match == MATCH_ALL) {
		spawn(&child3, test88e_child3, DROP_USER);

		snd(&child3, match);
		snd(&child3, id);

		if (rcv(&child3) != 0) e(0);
	}

	/* Start the second auxiliary child if desired, after all others. */
	if (aux & 2) {
		spawn(&aux2, test88e_childaux, DROP_NONE);

		snd(&aux2, 2);
		snd(&aux2, id);
		snd(&aux2, (aux & 4) ? 2 : 1);

		if (rcv(&aux2) != 0) e(0);

		if (aux & 4)
			aux_ncnt++;
	}

	usleep(WAIT_USECS);

	/*
	 * Test semaphore values and determine the value with which to increase
	 * the third semaphore.  For MATCH_KILL, also kill the first child.
	 */
	inc = 1;
	switch (match) {
	case MATCH_FIRST:
	case MATCH_SECOND:
		TEST_SEM(id, 2, 0, 0, 2 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 0, 0);
		break;
	case MATCH_KILL:
		TEST_SEM(id, 2, 0, 0, 2 + aux_ncnt, aux_zcnt);

		terminate(&child1);

		/* As stated before, non-self kills need not be instant. */
		usleep(WAIT_USECS);

		TEST_SEM(id, 2, 0, 0, 1 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 0, 0);
		break;
	case MATCH_BOTH:
		TEST_SEM(id, 2, 0, 0, 2 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 0, 0);
		inc = 2;
		break;
	case MATCH_CASCADE:
		TEST_SEM(id, 2, 0, 0, 1 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 1, 0);
		break;
	case MATCH_ALL:
		TEST_SEM(id, 2, 0, 0, 2 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 1, 0);
		inc = 2;
		break;
	default:
		e(0);
	}

	TEST_SEM(id, 0, 0, 0, 0, 0);
	TEST_SEM(id, 1, 0, 0, -1, -1);

	/* Resume the appropriate set of children. */
	switch (resume) {
	case RESUME_SEMOP:
		memset(&sop, 0, sizeof(sop));
		sop.sem_num = 2;
		sop.sem_op = inc;
		if (semop(id, &sop, 1) != 0) e(0);
		break;
	case RESUME_SETVAL:
		if (semctl(id, 2, SETVAL, inc) != 0) e(0);
		break;
	case RESUME_SETALL:
		memset(val, 0, sizeof(val));
		val[2] = inc;
		if (semctl(id, 0, SETALL, val) != 0) e(0);
		break;
	default:
		e(0);
	}

	/*
	 * See if the right children were indeed resumed, and retest the
	 * semaphore values.
	 */
	switch (match) {
	case MATCH_FIRST:
		TEST_SEM(id, 2, 0, child1.pid, 1 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 1, child1.pid, 0, 0);
		collect(&child1);
		break;
	case MATCH_SECOND:
		TEST_SEM(id, 2, 0, child2.pid, 1 + aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 0, 0);
		collect(&child2);
		break;
	case MATCH_KILL:
		TEST_SEM(id, 2, 0, child2.pid, aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, 0, 0, 0);
		collect(&child2);
		break;
	case MATCH_BOTH:
		/*
		 * The children are not ordered in this case, so we do not know
		 * which one gets access to the semaphores last.
		 */
		TEST_SEM(id, 2, 0, -1, aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 2, -1, 0, 0);
		collect(&child1);
		collect(&child2);
		break;
	case MATCH_CASCADE:
		TEST_SEM(id, 2, 0, child1.pid, aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, child2.pid, 0, 0);
		collect(&child1);
		collect(&child2);
		break;
	case MATCH_ALL:
		TEST_SEM(id, 2, 0, -1, aux_ncnt, aux_zcnt);
		TEST_SEM(id, 3, 0, child3.pid, 0, 0);
		collect(&child1);
		collect(&child2);
		collect(&child3);
		break;
	default:
		e(0);
	}

	TEST_SEM(id, 0, 0, 0, 0, 0);
	TEST_SEM(id, 1, 0, 0, -1, -1);

	/* Remove the semaphore set.  This should unblock remaining callers. */
	if (semctl(id, 0, IPC_RMID) != 0) e(0);

	/* Wait for the children that were not resumed, but should be now. */
	switch (match) {
	case MATCH_FIRST:
		collect(&child2);
		break;
	case MATCH_SECOND:
		collect(&child1);
		break;
	case MATCH_KILL:
	case MATCH_BOTH:
	case MATCH_CASCADE:
	case MATCH_ALL:
		break;
	default:
		e(0);
	}

	/* Wait for the auxiliary children as well. */
	if (aux & 1)
		collect(&aux1);
	if (aux & 2)
		collect(&aux2);
}

/*
 * Test operations affecting multiple processes, ensuring the following points:
 * 1) an operation resumes all possible waiters; 2) a resumed operation in turn
 * correctly resumes other now-unblocked operations; 3) a basic level of FIFO
 * fairness is provided between blocked parties; 4) all the previous points are
 * unaffected by additional waiters that are not being resumed; 5) identifier
 * removal properly resumes all affected waiters.
 */
static void
test88e(void)
{
	unsigned int resume, match, aux;

	subtest = 4;

	for (match = 0; match < NR_MATCHES; match++)
		for (resume = 0; resume < NR_RESUMES; resume++)
			for (aux = 1; aux <= 8; aux++) /* 0 and 4 are equal */
				sub88e(match, resume, aux);
}

/*
 * Verify that non-root processes can use sysctl(2) to see semaphore sets
 * created by root.
 */
static void
test88f_child(struct link * parent)
{
	static const int mib[] = { CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_INFO,
	    KERN_SYSVIPC_SEM_INFO };
	struct sem_sysctl_info *semsi;
	size_t len;
	int id[2], id2, seen[2];
	int32_t i;

	id[0] = rcv(parent);
	id[1] = rcv(parent);

	if (sysctl(mib, __arraycount(mib), NULL, &len, NULL, 0) != 0) e(0);

	if ((semsi = malloc(len)) == NULL) e(0);

	if (sysctl(mib, __arraycount(mib), semsi, &len, NULL, 0) != 0) e(0);

	seen[0] = seen[1] = 0;
	for (i = 0; i < semsi->seminfo.semmni; i++) {
		if (!(semsi->semids[i].sem_perm.mode & SEM_ALLOC))
			continue;

		id2 = IXSEQ_TO_IPCID(i, semsi->semids[i].sem_perm);
		if (id2 == id[0])
			seen[0]++;
		else if (id2 == id[1])
			seen[1]++;
	}

	free(semsi);

	if (seen[0] != 1) e(0);
	if (seen[1] != 1) e(0);
}

/*
 * Test sysctl(2) based information retrieval.  This test aims to ensure that
 * in particular ipcs(1) and ipcrm(1) will be able to do their jobs.
 */
static void
test88f(void)
{
	static const int mib[] = { CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_INFO,
	    KERN_SYSVIPC_SEM_INFO };
	struct seminfo seminfo, seminfo2;
	struct sem_sysctl_info *semsi;
	struct semid_ds_sysctl *semds;
	struct link child;
	size_t len, size;
	int id[2], id2;
	int32_t i, slot[2];

	/*
	 * Verify that we can retrieve only the general semaphore information,
	 * without any actual semaphore set entries.  This is actually a dirty
	 * sysctl-level hack, as sysctl requests should not behave differently
	 * based on the requested length.  However, ipcs(1) relies on this.
	 */
	len = sizeof(seminfo);
	if (sysctl(mib, __arraycount(mib), &seminfo, &len, NULL, 0) != 0) e(0);
	if (len != sizeof(seminfo)) e(0);

	if (semctl(0, 0, IPC_INFO, &seminfo2) == -1) e(0);

	if (memcmp(&seminfo, &seminfo2, sizeof(seminfo)) != 0) e(0);

	/* Verify that the correct size estimation is returned. */
	if (seminfo.semmni <= 0) e(0);
	if (seminfo.semmni > SHRT_MAX) e(0);

	size = sizeof(*semsi) +
	    sizeof(semsi->semids[0]) * (seminfo.semmni - 1);

	len = 0;
	if (sysctl(mib, __arraycount(mib), NULL, &len, NULL, 0) != 0) e(0);
	if (len != size) e(0);

	/* Create two semaphore sets that should show up in the listing. */
	if ((id[0] = semget(KEY_A, 5, IPC_CREAT | 0612)) < 0) e(0);

	if ((id[1] = semget(IPC_PRIVATE, 3, 0650)) < 0) e(0);

	/*
	 * Retrieve the entire semaphore array, and verify that the general
	 * semaphore information is still correct.
	 */
	if ((semsi = malloc(size)) == NULL) e(0);

	len = size;
	if (sysctl(mib, __arraycount(mib), semsi, &len, NULL, 0) != 0) e(0);
	if (len != size) e(0);

	if (sizeof(semsi->seminfo) != sizeof(seminfo)) e(0);
	if (memcmp(&semsi->seminfo, &seminfo, sizeof(semsi->seminfo)) != 0)
	    e(0);

	/* Verify that our semaphore sets are each in the array once. */
	slot[0] = slot[1] = -1;
	for (i = 0; i < seminfo.semmni; i++) {
		if (!(semsi->semids[i].sem_perm.mode & SEM_ALLOC))
			continue;

		id2 = IXSEQ_TO_IPCID(i, semsi->semids[i].sem_perm);
		if (id2 == id[0]) {
			if (slot[0] != -1) e(0);
			slot[0] = i;
		} else if (id2 == id[1]) {
			if (slot[1] != -1) e(0);
			slot[1] = i;
		}
	}

	if (slot[0] < 0) e(0);
	if (slot[1] < 0) e(0);

	/* Check that the semaphore sets have the expected properties. */
	semds = &semsi->semids[slot[0]];
	if (semds->sem_perm.uid != geteuid()) e(0);
	if (semds->sem_perm.gid != getegid()) e(0);
	if (semds->sem_perm.cuid != geteuid()) e(0);
	if (semds->sem_perm.cgid != getegid()) e(0);
	if (semds->sem_perm.mode != (SEM_ALLOC | 0612)) e(0);
	if (semds->sem_perm._key != KEY_A) e(0);
	if (semds->sem_nsems != 5) e(0);
	if (semds->sem_otime != 0) e(0);
	if (semds->sem_ctime == 0) e(0);

	semds = &semsi->semids[slot[1]];
	if (semds->sem_perm.uid != geteuid()) e(0);
	if (semds->sem_perm.gid != getegid()) e(0);
	if (semds->sem_perm.cuid != geteuid()) e(0);
	if (semds->sem_perm.cgid != getegid()) e(0);
	if (semds->sem_perm.mode != (SEM_ALLOC | 0650)) e(0);
	if (semds->sem_perm._key != IPC_PRIVATE) e(0);
	if (semds->sem_nsems != 3) e(0);
	if (semds->sem_otime != 0) e(0);
	if (semds->sem_ctime == 0) e(0);

	/* Make sure that non-root users can see them as well. */
	spawn(&child, test88f_child, DROP_ALL);

	snd(&child, id[0]);
	snd(&child, id[1]);

	collect(&child);

	/* Clean up, and verify that the sets are no longer in the listing. */
	if (semctl(id[0], 0, IPC_RMID) != 0) e(0);
	if (semctl(id[1], 0, IPC_RMID) != 0) e(0);

	len = size;
	if (sysctl(mib, __arraycount(mib), semsi, &len, NULL, 0) != 0) e(0);
	if (len != size) e(0);

	for (i = 0; i < seminfo.semmni; i++) {
		if (!(semsi->semids[i].sem_perm.mode & SEM_ALLOC))
			continue;

		id2 = IXSEQ_TO_IPCID(i, semsi->semids[i].sem_perm);
		if (id2 == id[0]) e(0);
		if (id2 == id[1]) e(0);
	}

	free(semsi);
}

/*
 * Initialize the test.
 */
static void
test88_init(void)
{
	static const int mib[] = { CTL_KERN, KERN_SYSVIPC, KERN_SYSVIPC_SEM };
	struct group *gr;
	size_t len;
	int i;

	/* Start with full root privileges. */
	setuid(geteuid());

	if ((gr = getgrnam(ROOT_GROUP)) == NULL) e(0);

	setgid(gr->gr_gid);
	setegid(gr->gr_gid);

	/*
	 * Verify that the IPC service is running at all.  If not, there is
	 * obviously no point in running this test.
	 */
	len = sizeof(i);
	if (sysctl(mib, __arraycount(mib), &i, &len, NULL, 0) != 0) e(0);
	if (len != sizeof(i)) e(0);

	if (i == 0) {
		printf("skipped\n");
		cleanup();
		exit(0);
	}

	/* Allocate a memory page followed by an unmapped page. */
	page_size = getpagesize();
	page_ptr = mmap(NULL, page_size * 2, PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_PRIVATE, -1, 0);
	if (page_ptr == MAP_FAILED) e(0);
	bad_ptr = page_ptr + page_size;
	if (munmap(bad_ptr, page_size) != 0) e(0);
}

/*
 * Test program for SysV IPC semaphores.
 */
int
main(int argc, char ** argv)
{
	int i, m;

	start(88);

	test88_init();

	if (argc == 2)
		m = atoi(argv[1]);
	else
		m = 0xFF;

	for (i = 0; i < ITERATIONS; i++) {
		if (m & 0x01) test88a();
		if (m & 0x02) test88b();
		if (m & 0x04) test88c();
		if (m & 0x08) test88d();
		if (m & 0x10) test88e();
		if (m & 0x20) test88f();
	}

	quit();
}
