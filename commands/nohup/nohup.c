/*
 * Copyright (c) 2009, Erik van der Kouwe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO 
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/*
 * Functionality implemented according to this specification:
 * http://www.opengroup.org/onlinepubs/000095399/utilities/nohup.html
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define NOHUP_OUT_FILENAME "nohup.out"

static void print_usage(const char *argv0)
{
	printf("Usage: %s command [arg...]\n", argv0);
}

static int redirect_tty(void)
{
	int fd;
	char buffer[PATH_MAX + 1], *home;

	/* redirect stdout to a file if needed */
	if (isatty(STDOUT_FILENO))
	{
		/* first try: current directory */
		fd = open(NOHUP_OUT_FILENAME, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
		if (fd < 0)
		{
			/* alternative: home directory */
			home = getenv("HOME");
			if (home)
			{
				snprintf(buffer, sizeof(buffer), "%s/%s", home, NOHUP_OUT_FILENAME);
				buffer[sizeof(buffer) - 1] = 0;
				fd = open(buffer, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
			}
		}
				
		if (fd < 0)
		{
			perror("cannot create " NOHUP_OUT_FILENAME " and $HOME/" NOHUP_OUT_FILENAME);
			return -1;
		}
		
		/* move the fd to stdout */
		if (dup2(fd, STDOUT_FILENO) < 0 || close(fd) < 0)
		{
			perror("cannot redirect stdout");
			return -1;
		}
	}
	
	/* redirect stderr to stdout if needed */
	if (isatty(STDERR_FILENO))
	{
		if (dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
		{
			perror("cannot redirect stderr");
			return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct sigaction sa;

	/* check parameters */
	if (argc < 2)
	{
		print_usage(argv[0]);
		return 127;
	}

	/* ignore SIGHUP */
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGHUP, &sa, NULL) < 0)
	{
		perror("cannot ignore SIGHUP");
		return 127;
	}

	/* redirect TTY input and output */
	if (redirect_tty() < 0)
		return 127;

	/* run the command */
	execvp(argv[1], argv + 1);
	perror("cannot execute");

	/* exit code depends on whether the utility was found */
	switch (errno)
	{
		case ELOOP:
		case ENAMETOOLONG:
		case ENOENT:
		case ENOTDIR:
			/* utility not found */
			return 127;

		default:
			/* exec failed for other reason */
			return 126;
	}
}
