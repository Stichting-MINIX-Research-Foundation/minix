
#include <stdio.h>

int main(int argc, char *argv[], char *envp[])
{
	int p;
	for(p = 0; envp[p] && *envp[p]; p++) {
		printf("%s\n", envp[p]);
	}
	return 0;
}

