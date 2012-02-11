/*	getpass() - read a password		Author: Kees J. Bot
 *							Feb 16 1993
 */
#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <string.h>

#ifdef __weak_alias
__weak_alias(getpass, _getpass)
#endif

static int intr;

static void catch(int sig)
{
	intr= 1;
}

char *getpass(const char *prompt)
{
	struct sigaction osa, sa;
	struct termios cooked, raw;
	static char password[32+1];
	int fd, n= 0;

	/* Try to open the controlling terminal. */
	if ((fd= open("/dev/tty", O_RDONLY)) < 0) return NULL;

	/* Trap interrupts unless ignored. */
	intr= 0;
	sigaction(SIGINT, NULL, &osa);
	if (osa.sa_handler != SIG_IGN) {
		sigemptyset(&sa.sa_mask);
		sa.sa_flags= 0;
		sa.sa_handler= catch;
		sigaction(SIGINT, &sa, &osa);
	}

	/* Set the terminal to non-echo mode. */
	tcgetattr(fd, &cooked);
	raw= cooked;
	raw.c_iflag|= ICRNL;
	raw.c_lflag&= ~ECHO;
	raw.c_lflag|= ECHONL;
	raw.c_oflag|= OPOST | ONLCR;
	tcsetattr(fd, TCSANOW, &raw);

	/* Print the prompt.  (After setting non-echo!) */
	write(2, prompt, strlen(prompt));

	/* Read the password, 32 characters max. */
	while (read(fd, password+n, 1) > 0) {
		if (password[n] == '\n') break;
		if (n < 32) n++;
	}
	password[n]= 0;

	/* Terminal back to cooked mode. */
	tcsetattr(fd, TCSANOW, &cooked);

	close(fd);

	/* Interrupt? */
	sigaction(SIGINT, &osa, NULL);
	if (intr) raise(SIGINT);

	return password;
}
