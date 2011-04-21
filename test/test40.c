/* Test40.c
 *
 * Test select(...) system call
 *
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#define MAX_ERROR 5
#include "common.c"

int main(int argc, char **argv) {
  char *tests[] = {"t40a", "t40b", "t40c", "t40d", "t40e", "t40f"};
  char copy_command[8+PATH_MAX+1];
  int no_tests, i, forkres, status = 0, errorct = 0;

  no_tests = sizeof(tests) / sizeof(char *);
  
  start(40);

  for(i = 0; i < no_tests; i++) {
    char subtest[2];
    snprintf(subtest, 2, "%d", i+1);
    
    /* Copy subtest */
    snprintf(copy_command, 8 + PATH_MAX, "cp ../%s .", tests[i]);
    system(copy_command);

    forkres = fork();
    if(forkres == 0) { /* Child */
      execl(tests[i], tests[i], subtest, (char *) 0); 
      printf("Failed to execute subtest %s\n", tests[i]);
      exit(-2);
    } else if(forkres > 0) { /* Parent */
      if(waitpid(forkres, &status, 0) > 0 && WEXITSTATUS(status) < 20) {
	errorct += WEXITSTATUS(status); /* Count errors */
      }
      status = 0; /* Reset */
    } else {
      printf("Failed to fork\n");
      exit(-2);
    }
  }

  quit();
  
  return (-1); /* Impossible */
}

