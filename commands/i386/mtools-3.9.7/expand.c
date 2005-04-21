/*
 * Do filename expansion with the shell.
 */

#define EXPAND_BUF	2048

#include "sysincludes.h"
#include "mtools.h"


int safePopenOut(char **command, char *output, int len)
{
	int pipefd[2];
	pid_t pid;
	int status;
	int last;

	if(pipe(pipefd)) {
		return -2;
	}
	switch((pid=fork())){
		case -1:
			return -2;
		case 0: /* the son */
			close(pipefd[0]);
			destroy_privs();
			close(1);
			close(2); /* avoid nasty error messages on stderr */
			dup(pipefd[1]);
			close(pipefd[1]);
			execvp(command[0], command+1);
			exit(1);
		default:
			close(pipefd[1]);
			break;
	}
	last=read(pipefd[0], output, len);
	kill(pid,9);
	wait(&status);
	if(last<0) {
		return -1;
	}
	return last;
}



const char *expand(const char *input, char *ans)
{
	int last;
	char buf[256];
	char *command[] = { "/bin/sh", "sh", "-c", 0, 0 };

	ans[EXPAND_BUF-1]='\0';

	if (input == NULL)
		return(NULL);
	if (*input == '\0')
		return("");
					/* any thing to expand? */
	if (!strpbrk(input, "$*(){}[]\\?`~")) {
		strncpy(ans, input, EXPAND_BUF-1);
		return(ans);
	}
					/* popen an echo */
#ifdef HAVE_SNPRINTF
	snprintf(buf, 255, "echo %s", input);
#else
	sprintf(buf, "echo %s", input);
#endif

	command[3]=buf;
	last=safePopenOut(command, ans, EXPAND_BUF-1);
	if(last<0) {
		perror("Pipe read error");
		exit(1);
	}
	if(last)
		ans[last-1] = '\0';
	else
		strncpy(ans, input, EXPAND_BUF-1);
	return ans;
}
