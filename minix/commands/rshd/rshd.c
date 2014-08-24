/*
in.rshd.c
*/

/*
	main channel:

	back channel\0
	remuser\0
	locuser\0
	command\0
	data

	back channel:
	signal\0

*/

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <netdb.h>
#include <net/gen/socket.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include <net/hton.h>
#include <net/netlib.h>

#define DEBUG 0

#if DEBUG
#define where() fprintf(stderr, "%s, %d: ", __FILE__, __LINE__)
#endif

char cmdbuf[_POSIX_ARG_MAX+1], locuser[16], remuser[16];
extern char **environ;
char username[20]="USER=";
char homedir[64]="HOME=";
char shell[64]="SHELL=";
char tz[1024]="TZ=";
char *envinit[]= {homedir, shell, username, tz, "PATH=:/bin:/usr/bin", 0};
char *prog_name;
char buffer[PIPE_BUF];

#if __STDC__
#define PROTO(func, args) func args
#else
#define PROTO(func, args) func ()
#endif

PROTO (int main, (int argc, char *argv[]));
PROTO (void getstr, (char*buf, int cnt, char *err));
PROTO (void close_on_exec, (int fd));

int main(argc, argv)
int argc;
char *argv[];
{
	int result, result1;
	nwio_tcpconf_t tcpconf, err_tcpconf;
	nwio_tcpcl_t tcpconnopt;
	nwio_tcpatt_t tcpattachopt;
	tcpport_t tcpport;
	tcpport_t err_port;
	int err_fd, pds[2];
	pid_t pid, pid1, new_pg;
#if USEATTACH
	int err2_fd;
#endif
	struct passwd *pwent;
	char *cp, *buff_ptr, *TZ;
	char sig;

	prog_name= argv[0];
	if (argc != 1)
	{
		fprintf(stderr, "%s: wrong number of arguments (%d)\n",
			prog_name, argc);
		exit(1);
	}

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);

#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif
	result= ioctl (0, NWIOGTCPCONF, &tcpconf);
	if (result<0)
	{
		fprintf(stderr, "%s: ioctl(NWIOGTCPCONF)= %d : %s\n", 
			prog_name, errno, strerror(errno));
		exit(1);
	}
#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif

	tcpport= ntohs(tcpconf.nwtc_remport);
	if (tcpport >= TCPPORT_RESERVED || tcpport < TCPPORT_RESERVED/2)
	{
		printf("\1%s: unprotected port (%d)\n", prog_name, tcpport);
		exit(1);
	}
	alarm(60);
	err_port= 0;
	for (;;)
	{
		char c;
		result= read(0, &c, 1);
		if (result <0)
		{
			fprintf(stderr, "%s: read= %d : %s\n", prog_name, 
				errno, strerror(errno));
		}
		if (result<1)
			exit(1);
		if (c == 0)
			break;
		err_port= err_port*10 + c - '0';
	}
	alarm(0);
	if (err_port != 0)
	{
		int n, pid, lport;

		pid= getpid();
		lport= 1;
		do {
			lport= (lport << 1) | (pid & 1);
			pid >>= 1;
		} while (lport < TCPPORT_RESERVED/2);

		n= TCPPORT_RESERVED/2;
		do
		{
			if (--lport < TCPPORT_RESERVED/2)
				lport= TCPPORT_RESERVED-1;
			err_fd= open ("/dev/tcp", O_RDWR);
			if (err_fd<0)
			{
				fprintf(stderr, "%s: open= %d : %s\n", 
					prog_name, errno, strerror(errno));
				exit(1);
			}
			close_on_exec(err_fd);
			err_tcpconf.nwtc_flags= NWTC_LP_SET | NWTC_SET_RA |
				NWTC_SET_RP | NWTC_EXCL;
			err_tcpconf.nwtc_locport= htons(lport);
			err_tcpconf.nwtc_remport= htons(err_port);
			err_tcpconf.nwtc_remaddr= tcpconf.nwtc_remaddr;

#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif
			result= ioctl (err_fd, NWIOSTCPCONF, &err_tcpconf);
			if (result == 0) break;
			if (errno != EADDRINUSE)
			{
				fprintf(stderr, 
					"%s: ioctl(NWIOSTCPCONF)= %d : %s\n",
					prog_name, errno, strerror(errno));
				exit(1);
			}
			close(err_fd);
		} while (--n > 0);
		if (n == 0)
		{
			printf("\1can't get stderr port\n");
			exit(1);
		}

		err_tcpconf.nwtc_flags= NWTC_SHARED;
#if DEBUG
{ where(); fprintf(stderr, "\n"); }
#endif
		result= ioctl (err_fd, NWIOSTCPCONF, &err_tcpconf);
		if (result<0)
		{
			fprintf(stderr, 
				"%s: ioctl(NWIOSTCPCONF)= %d : %s\n",
				prog_name, errno, strerror(errno));
			exit(1);
		}
#if DEBUG
{ where(); fprintf(stderr, "\n"); }
#endif
		tcpconnopt.nwtcl_flags= 0;

		n= 20;
		for (;;)
		{
#if DEBUG
{ where(); fprintf(stderr, "\n"); }
#endif
			result= ioctl (err_fd, NWIOTCPCONN, &tcpconnopt);
			if (result == 0) break;
			if (errno != EAGAIN && errno != ECONNREFUSED)
			{
				fprintf(stderr,
					"%s: ioctl(NWIOTCPCONN)= %d : %s\n",
					prog_name, errno, strerror(errno));
				exit(1);
			}
			if (--n == 0) break;
			sleep(1);
#if DEBUG
{ where(); fprintf(stderr, "\n"); }
#endif
		}
#if USEATTACH
		err2_fd= open ("/dev/tcp", O_RDWR);
		close_on_exec(err2_fd);
		if (err2_fd<0)
		{
			fprintf(stderr, "%s: open= %d : %s\n", errno,
				prog_name, strerror(errno));
			exit(1);
		}
#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif
		result= ioctl (err2_fd, NWIOSTCPCONF, &err_tcpconf);
		if (result<0)
		{
			fprintf(stderr, "%s: ioctl(NWIOSTCPCONF)= %d : %s\n",
				prog_name, errno, strerror(errno));
			exit(1);
		}
#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif
		tcpattachopt.nwta_flags= 0;
#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif
		result= ioctl (err2_fd, NWIOTCPATTACH, &tcpattachopt);
		if (result<0)
		{
			fprintf(stderr, "%s: ioctl(NWIOTCPATTACH)= %d : %s\n",
				prog_name, errno, strerror(errno));
			exit(1);
		}
#if DEBUG
 { where(); fprintf(stderr, "\n"); }
#endif
#endif
	}
	getstr(remuser, sizeof(remuser), "remuser");
	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "cmdbuf");
	setpwent();
	pwent= getpwnam(locuser);
	if (!pwent)
	{
		printf("\1Login incorrect.\n");
		exit(1);
	}
	endpwent();
	if (chdir(pwent->pw_dir) < 0)
	{
		chdir("/");
	}
#if DEBUG
 { where(); fprintf(stderr, "calling iruserok(%s, %d, %s, %s)\n", 
	inet_ntoa(tcpconf.nwtc_remaddr), 0, remuser, locuser); }
#endif
	if (iruserok(tcpconf.nwtc_remaddr, 0, remuser, locuser) < 0)
	{
		printf("\1Permission denied.\n");
		exit(1);
	}
	if (err_port)
	{
		/* Let's go to a different process group. */
		new_pg= setsid();
		pid= fork();
		if (pid<0)
		{
			if (errno != EAGAIN)
			{
				fprintf(stderr, "%s: fork()= %d : %s\n",
					prog_name, errno, strerror(errno));
			}
			printf("\1Try again.\n");
			exit(1);
		}
		if (pid)
		{
			close(0);	/* stdin */
			close(1);	/* stdout */
#if USEATTACH
			close(err_fd);	/* stderr for shell */
#endif
			dup2(2,0);
			dup2(2,1);
			for (;;)
			{
#if !USEATTACH
				if (read(err_fd, &sig, 1) <= 0)
#else
				if (read(err2_fd, &sig, 1) <= 0)
#endif
				{
#if 0
					printf("read failed: %d\n", errno);
#endif
					exit(0);
				}
				pid= 0;
#if 0
				printf("killing %d with %d\n", -new_pg, sig);
#endif
				kill(-new_pg, sig);
			}
		}
#if USEATTACH
		close(err2_fd);	/* signal channel for parent */
#endif
		result= pipe(pds);
		if (result<0)
		{
			printf("\1Can't make pipe\n");
			kill(getppid(), SIGTERM);
			exit(1);
		}
		pid1= fork();
		if (pid1<0)
		{
			if (errno != EAGAIN)
			{
				fprintf(stderr, "%s: fork()= %d : %s\n",
					prog_name, errno, strerror(errno));
			}
			printf("\1Try again.\n");
			kill(-new_pg, SIGTERM);
			exit(1);
		}
		if (pid1)
		{
			close(pds[1]);	/* write side of pipe */
			for (;;)
			{
				result= read(pds[0], buffer, sizeof(buffer));
				if (result<=0)
				{
					kill(pid, SIGTERM);
					exit(0);
				}
				buff_ptr= buffer;
				while (result>0)
				{
					result1= write (err_fd, buff_ptr,
						result);
					if (result1 <= 0)
					{
						fprintf(stderr, 
						"%s: write()= %d : %s\n",
							prog_name, errno,
							strerror(errno));
						kill(-new_pg, SIGTERM);
						exit(1);
					}
					result -= result1;
				}
			}
		}
		close(err_fd);	/* file descriptor for error channel */
		close (pds[0]);	/* read side of pipe */
		dup2(pds[1], 2);
		close (pds[1]);	/* write side of pipe */
	}
	if (*pwent->pw_shell == '\0')
		pwent->pw_shell= "/bin/sh";
#if __minix_vmd
	initgroups(pwent->pw_name, pwent->pw_gid);
#endif
	setgid(pwent->pw_gid);
	setuid(pwent->pw_uid);
	TZ=getenv("TZ");
	environ= envinit;
	strncat(homedir, pwent->pw_dir, sizeof(homedir)-6);
	strncat(shell, pwent->pw_shell, sizeof(shell)-7);
	strncat(username, pwent->pw_name, sizeof(username)-6);
	if (TZ)
		strncat(tz, TZ, sizeof(tz)-4);
	else
		envinit[3]= NULL;

	cp= strrchr(pwent->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp= pwent->pw_shell;

	if (!err_port)
		dup2(1, 2);
	write(1, "\0", 1);

	execl(pwent->pw_shell, cp, "-c", cmdbuf, 0);
	close(2);
	open("/dev/tty", O_RDWR);
	fprintf(stderr, "%s: execl(%s, %s, .., %s)= %d : %s\n", prog_name,
		pwent->pw_shell, cp, cmdbuf, errno, strerror(errno));
	kill(getppid(), SIGTERM);
	exit(1);
}

void getstr(buf, cnt, err)
char *buf;
int cnt;
char *err;
{
	char c;

	do
	{
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0)
		{
			printf("\1%s too long", err);
			exit(1);
		}
	} while (c != 0);
}

void close_on_exec(fd)
int fd;
{
	(void) fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}
