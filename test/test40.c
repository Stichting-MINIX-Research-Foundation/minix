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

int main(int argc, char **argv) {
  char *tests[] = {"t40a", "t40b", "t40c", "t40d", "t40e", "t40f"};
  int no_tests, i, forkres, status = 0, errorct = 0;

  no_tests = sizeof(tests) / sizeof(char *);
  
  printf("Test 40 ");
  fflush(stdout);

  for(i = 0; i < no_tests; i++) {
    char subtest[2];
    sprintf(subtest, "%d", i+1);
    
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

  if(errorct == 0) {
    printf("ok\n");
    exit(0);
  } else {
    printf("%d error(s)\n", errorct);
    exit(1);
  }
  
  return (-1); /* Impossible */
}

