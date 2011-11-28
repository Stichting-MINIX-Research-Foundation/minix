#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

#define MAX_ERROR 5
#include "common.c"

int subtest = -1;

void test_self(void);
void test_setnone(void);
void test_setuid(void);
void test_setgid(void);
void test_effugid(void);
int execute(const char *prog, const char *arg);

int execute(const char *prog, const char *arg)
{
  pid_t childpid;
  int status;
  char cmd[30];

  snprintf(cmd, sizeof(cmd), "./%s", prog);

  childpid = fork();
  if (childpid == (pid_t) -1) {
	return(-2);
  } else if (childpid == 0) {
	if (execl(cmd, prog, arg, NULL) == -1) {
		exit(-2);
	}
	return(-2);	/* Never reached */
  } else {
	wait(&status);
  }

  return(WEXITSTATUS(status));
}

void test_setgid(void)
{
/* Execve a new process that has setgid bits set */
  subtest = 3;

  /* When we exec a new process which has setgid set, that process should
   * be tainted.
   */
  system("chmod 2755 setgid");
  if (execute("setgid", "0000") != 1) e(2);

  /* When we exec a new process which has setgid set, but unsets that bit
   * before calling issetugid() should still be tainted
   */
  system("chmod 2755 setgid");
  if (execute("setgid", "0755") != 1) e(3);

  /* When we exec a new process which has setgid set, and then also sets
   * setuid before calling issetugid() should still be tainted
   */
  system("chmod 2755 setgid");
  if (execute("setgid", "06755") != 1) e(4);

  /* When we exec a new process that has setgid set, and which upon
   * execution forks, the forked child should also be tainted */
  system("chmod 2755 setgidfork");
  if (execute("setgidfork", "0000") != 1) e(5);
}

void test_setuid(void)
{
/* Execve a new process that has setuid bits set */
  subtest = 4;

  /* When we exec a new process which has setuid set, that process should
   * be tainted.
   */
  system("chmod 4755 setuid");
  if (execute("setuid", "0000") != 1) e(1);

  /* When we exec a new process which has setuid set, but unsets that bit
   * before calling issetugid() should still be tainted
   */
  system("chmod 4755 setuid");
  if (execute("setuid", "0755") != 1) e(2);

  /* When we exec a new process which has setuid set, and then also sets
   * setgid before calling issetugid() should still be tainted
   */
  system("chmod 4755 setuid");
  if (execute("setuid", "06755") != 1) e(3);

  /* When we exec a new process that has setgid set, and which upon
   * execution forks, the forked child should also be tainted */
  system("chmod 4755 setuidfork");
  if (execute("setuidfork", "0000") != 1) e(4);

}

void test_setugid(void)
{
/* Execve a new process that has setuid and setgid bits set */
  subtest = 5;

  /* When we exec a new process which has setugid set, that
   * process should be tainted.
   */
  system("chmod 6755 setugid");
  if (execute("setugid", "0000") != 1) e(1);

  /* When we exec a new process which has setugid set, but unsets those bits
   * before calling issetugid() should still be tainted
   */
  system("chmod 6755 setugid");
  if (execute("setugid", "0755") != 1) e(2);

  /* When we exec a new process that has setugid set, and which upon
   * execution forks, the forked child should also be tainted */
  system("chmod 6755 setugidfork");
  if (execute("setugidfork", "0000") != 1) e(4);

}

void test_effugid(void)
{
/* Test taint status with different effective uid and gid */
  pid_t childpid;
  int status;

  subtest = 6;

  /* Start with effective uid */
  childpid = fork();
  if (childpid == (pid_t) -1) e(1);
  else if (childpid == 0) {
	/* We're the child */

	/* We should be tainted */
	if (issetugid() != 1) e(2);

	/* Now execute a program without set{u,g}id; should not be tainted */
	system("chmod 755 nobits");
	if (execute("nobits", "0000") != 0) e(3);

	/* Change effective uid into current+42 and try nobits again. This time
	 * it should be tainted */
	if (seteuid(geteuid() + 42) != 0) e(4);
	if (execute("nobits", "0000") != 1) e(5);
	exit(EXIT_SUCCESS);
  } else {
	/* We're the parent, wait for the child to finish */
	wait(&status);
  }

  /* Now test effective gid */
  childpid = fork();
  if (childpid == (pid_t) -1) e(1);
  else if (childpid == 0) {
	/* We're the child */

	/* We should be tainted */
	if (issetugid() != 1) e(2);

	/* Now execute a program without set{u,g}id; should not be tainted */
	system("chmod 755 nobits");
	if (execute("nobits", "0000") != 0) e(3);

	/* Change effective gid into current+42 and try nobits again. This time
	 * it should be tainted */
	if (seteuid(getegid() + 42) != 0) e(4);
	if (execute("nobits", "0000") != 1) e(5);
	exit(EXIT_SUCCESS);
  } else {
	/* We're the parent, wait for the child to finish */
	wait(&status);
  }
}

void test_setnone(void)
{
/* Execve a new process that does not have setuid or setgid bits set */
  subtest = 2;

  /* When we exec a new process which doesn't have set{u,g}id set, that
   * process should not be tainted */
  system("chmod 755 nobits");
  if (execute("nobits", "0000") != 0) e(2);

  /* When we exec a new process which doesn't have set{u,g}id set, but
   * sets them after execution, the process should still not be tainted
   */
  system("chmod 755 nobits");
  if (execute("nobits", "02755") != 0) e(4);
  system("chmod 755 nobits");
  if (execute("nobits", "04755") != 0) e(3);
  system("chmod 755 nobits");
  if (execute("nobits", "06755") != 0) e(5);

  /* When we exec a new process that doesn't have setugid set, and which upon
   * execution forks, the forked child should not be tainted either */
  system("chmod 755 nobitsfork");
  if (execute("nobitsfork", "0000") != 0) e(6);
}

void test_self(void)
{
/* We're supposed to be setuid. Verify. */

  int status;
  pid_t childpid;

  subtest = 1;

  if (issetugid() != 1) e(1);
  childpid = fork();
  if (childpid == -1) e(2);
  else if (childpid == 0) {
	/* We're the child and should inherit the tainted status of the parent
	 */
	if (issetugid() != 1) e(3);

	/* Let's change to the bin user */
	if (setuid((uid_t) 2) != 0) e(4);
	if (getuid() != (uid_t) 2) e(5);

	/* At this point, taint status should not have changed. */
	if (issetugid() != 1) e(6);

	exit(EXIT_SUCCESS);
  } else {
	/* We're the parent. Wait for the child to finish */
	wait(&status);
  }
}

void switch_to_su(void)
{
  subtest = 0;
  if (setuid(0) != 0) e(1);
}

int main(int argc, char **argv)
{
  start(60);
  system("cp ../t60a nobits");
  system("cp ../t60a setgid");
  system("cp ../t60a setuid");
  system("cp ../t60a setugid");
  system("cp ../t60b nobitsfork");
  system("cp ../t60b setuidfork");
  system("cp ../t60b setgidfork");
  system("cp ../t60b setugidfork");

  switch_to_su();	/* We have to be root to perform this test */
  test_self();
  test_setnone();
  test_setuid();
  test_setgid();
  test_setugid();
  test_effugid();

  quit();

  return(-1);	/* Never reached */
}
