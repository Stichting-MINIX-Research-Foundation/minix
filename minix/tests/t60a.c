#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
/* Return our tainted state to the parent */
  int newmode;
  char cmd[30];

  if (argc < 2) return(-2);
  if ((newmode = atoi(argv[1])) > 0) {
	snprintf(cmd, sizeof(cmd), "chmod %d %s", newmode, argv[0]);
	system(cmd);
  }

  return(issetugid());
}
