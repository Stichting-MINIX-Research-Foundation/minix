#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
/* Return the tainted state of our child to our parent */
  pid_t childpid;
  int status;

  childpid = fork();
  if (childpid == (pid_t) -1) exit(-2);
  else if (childpid == 0) {
	exit(issetugid());
  } else {
	wait(&status);
  }

  return(WEXITSTATUS(status));
}

