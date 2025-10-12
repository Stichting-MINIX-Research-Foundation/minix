#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_FORKS 100

int main() {
    int i;
    pid_t pid;

    for (i = 0; i < NUM_FORKS; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Child process
            exit(0);
        }
    }

    // Parent process
    for (i = 0; i < NUM_FORKS; i++) {
        wait(NULL);
    }

    printf("ASID stress test completed successfully.\n");

    return 0;
}

