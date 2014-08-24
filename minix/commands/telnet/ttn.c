/*
ttn.c
*/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/hton.h>
#include <net/netlib.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <netdb.h>
#include <net/gen/tcp.h>
#include <net/gen/tcp_io.h>
#include "ttn.h"

#if __STDC__
#define PROTOTYPE(func,args) func args
#else
#define PROTOTYPE(func,args) func()
#endif

static int do_read(int fd, char *buf, unsigned len);
static void screen(void);
static void keyboard(void);
static void send_brk(void);
static int process_opt (char *bp, int count);
static void do_option (int optsrt);
static void dont_option (int optsrt);
static void will_option (int optsrt);
static void wont_option (int optsrt);
static int writeall (int fd, char *buffer, int buf_size);
static int sb_termtype (char *sb, int count);
static void fatal(char *fmt, ...);
static void usage(void);

#if DEBUG
#define where() (fprintf(stderr, "%s %d:", __FILE__, __LINE__))
#endif

static char *prog_name;
static int tcp_fd;
static char *term_env;
static int esc_char= '~';
static enum { LS_NORM, LS_BOL, LS_ESC } line_state= LS_BOL;

int main(int argc, char *argv[])
{
	struct hostent *hostent;
	struct servent *servent;
	ipaddr_t host;
	tcpport_t port;
	int pid, ppid;
	nwio_tcpconf_t tcpconf;
	int c, r;
	nwio_tcpcl_t tcpconnopt;
	struct termios termios;
	char *tcp_device, *remote_name, *port_name;
	char *e_arg;

	(prog_name=strrchr(argv[0],'/')) ? prog_name++ : (prog_name=argv[0]);

	e_arg= NULL;
	while (c= getopt(argc, argv, "?e:"), c != -1)
	{
		switch(c)
		{
		case '?': usage();
		case 'e': e_arg= optarg; break;
		default:
			fatal("Optind failed: '%c'", c);
		}
	}

	if (optind >= argc)
		usage();
	remote_name= argv[optind++];
	if (optind < argc)
		port_name= argv[optind++];
	else
		port_name= NULL;
	if (optind != argc)
		usage();

	if (e_arg)
	{
		switch(strlen(e_arg))
		{
		case 0: esc_char= -1; break;
		case 1: esc_char= e_arg[0]; break;
		default: fatal("Invalid escape character '%s'", e_arg);
		}
	}

	hostent= gethostbyname(remote_name);
	if (!hostent)
		fatal("Unknown host %s", remote_name);
	host= *(ipaddr_t *)(hostent->h_addr);

	if (!port_name)
		port= htons(TCPPORT_TELNET);
	else
	{
		servent= getservbyname (port_name, "tcp");
		if (!servent)
		{
			port= htons(strtol(port_name, (char **)0, 0));
			if (!port)
				fatal("Unknown port %s", port_name);
		}
		else
			port= (tcpport_t)(servent->s_port);
	}

	fprintf(stderr, "Connecting to %s:%u...\n",
		inet_ntoa(host), ntohs(port));

	tcp_device= getenv("TCP_DEVICE");
	if (tcp_device == NULL)
		tcp_device= TCP_DEVICE;
	tcp_fd= open (tcp_device, O_RDWR);
	if (tcp_fd == -1)
		fatal("Unable to open %s: %s", tcp_device, strerror(errno));

	tcpconf.nwtc_flags= NWTC_LP_SEL | NWTC_SET_RA | NWTC_SET_RP;
	tcpconf.nwtc_remaddr= host;
	tcpconf.nwtc_remport= port;

	r= ioctl (tcp_fd, NWIOSTCPCONF, &tcpconf);
	if (r == -1)
		fatal("NWIOSTCPCONF failed: %s", strerror(errno));

	tcpconnopt.nwtcl_flags= 0;
	do
	{
		r= ioctl (tcp_fd, NWIOTCPCONN, &tcpconnopt);
		if (r == -1 && errno == EAGAIN)
		{
			fprintf(stderr, "%s: Got EAGAIN, sleeping(1s)\n",
				prog_name);
			sleep(1);
		}
	} while (r == -1 && errno == EAGAIN);
	if (r == -1)
		fatal("Unable to connect: %s", strerror(errno));
	printf("Connected\n");
	ppid= getpid();
	pid= fork();
	switch(pid)
	{
	case 0:
		keyboard();
#if DEBUG
fprintf(stderr, "killing %d with %d\r\n", ppid, SIGKILL);
#endif
		kill(ppid, SIGKILL);
		break;
	case -1:
		fprintf(stderr, "%s: fork failed: %s\r\n", argv[0],
			strerror(errno));
		exit(1);
		break;
	default:
		tcgetattr(0, &termios);
		screen();
#if DEBUG
fprintf(stderr, "killing %d with %d\r\n", pid, SIGKILL);
#endif
		kill(pid, SIGKILL);
		tcsetattr(0, TCSANOW, &termios);
		break;
	}
	exit(0);
}

static int do_read(fd, buf, len)
int fd;
char *buf;
unsigned len;
{
	nwio_tcpopt_t tcpopt;
	int count;

	for (;;)
	{
		count= read (fd, buf, len);
		if (count <0)
		{
			if (errno == EURG || errno == ENOURG)
			{
				/* Toggle urgent mode. */
				tcpopt.nwto_flags= errno == EURG ?
					NWTO_RCV_URG : NWTO_RCV_NOTURG;
				if (ioctl(tcp_fd, NWIOSTCPOPT, &tcpopt) == -1)
				{
					return -1;
				}
				continue;
			}
			return -1;
		}
		return count;
	}
}

static void screen()
{
	char buffer[1024], *bp, *iacptr;
	int count, optsize;

	for (;;)
	{
		count= do_read (tcp_fd, buffer, sizeof(buffer));
#if DEBUG && 0
 { where(); fprintf(stderr, "read %d bytes\r\n", count); }
#endif
		if (count <0)
		{
			perror ("read");
			return;
		}
		if (!count)
			return;
		bp= buffer;
		do
		{
			iacptr= memchr (bp, IAC, count);
			if (!iacptr)
			{
				write(1, bp, count);
				count= 0;
			}
			if (iacptr && iacptr>bp)
			{
#if DEBUG 
 { where(); fprintf(stderr, "iacptr-bp= %d\r\n", iacptr-bp); }
#endif
				write(1, bp, iacptr-bp);
				count -= (iacptr-bp);
				bp= iacptr;
				continue;
			}
			if (iacptr)
			{
assert (iacptr == bp);
				optsize= process_opt(bp, count);
#if DEBUG && 0
 { where(); fprintf(stderr, "process_opt(...)= %d\r\n", optsize); }
#endif
				if (optsize<0)
					return;
assert (optsize);
				bp += optsize;
				count -= optsize;
			}
		} while (count);
	}
}

static void keyboard()
{
	char c, buffer[1024];
	int count;

	for (;;)
	{
		count= read (0, buffer, 1 /* sizeof(buffer) */);
		if (count == -1)
			fatal("Read: %s\r\n", strerror(errno));
		if (!count)
			return;

		if (line_state != LS_NORM)
		{
			c= buffer[0];
			if (line_state == LS_BOL)
			{
				if (c == esc_char)
				{
					line_state= LS_ESC;
					continue;
				}
				line_state= LS_NORM;
			}
			else if (line_state == LS_ESC)
			{
				line_state= LS_NORM;
				if (c == '.')
					return;
				if (c == '#')
				{
					send_brk();
					continue;
				}

				/* Not a valid command or a repeat of the
				 * escape char
				 */
				if (c != esc_char)
				{
					c= esc_char;
					write(tcp_fd, &c, 1);
				}
			}
		}
		if (buffer[0] == '\n')
			write(tcp_fd, "\r", 1);
		count= write(tcp_fd, buffer, count);
		if (buffer[0] == '\r')
		{
			line_state= LS_BOL;
			write(tcp_fd, "\0", 1);
		}
		if (count<0)
		{
			perror("write");
			fprintf(stderr, "errno= %d\r\n", errno);
			return;
		}
		if (!count)
			return;
	}
}

static void send_brk(void)
{
	int r;
	unsigned char buffer[2];

	buffer[0]= IAC;
	buffer[1]= IAC_BRK;

	r= writeall(tcp_fd, (char *)buffer, 2);
	if (r == -1)
		fatal("Error writing to TCP connection: %s", strerror(errno));
}

#define next_char(var) \
	if (offset<count) { (var) = bp[offset++]; } \
	else if (do_read(tcp_fd, (char *)&(var), 1) <= 0) \
	{ perror ("read"); return -1; }

static int process_opt (char *bp, int count)
{
	unsigned char iac, command, optsrt, sb_command;
	int offset, result;	;
#if DEBUG && 0
 { where(); fprintf(stderr, "process_opt(bp= 0x%x, count= %d)\r\n",
	bp, count); }
#endif

	offset= 0;
assert (count);
	next_char(iac);
assert (iac == IAC);
	next_char(command);
	switch(command)
	{
	case IAC_NOP:
		break;
	case IAC_DataMark:
		/* Ought to flush input queue or something. */
		break;
	case IAC_BRK:
fprintf(stderr, "got a BRK\r\n");
		break;
	case IAC_IP:
fprintf(stderr, "got a IP\r\n");
		break;
	case IAC_AO:
fprintf(stderr, "got a AO\r\n");
		break;
	case IAC_AYT:
fprintf(stderr, "got a AYT\r\n");
		break;
	case IAC_EC:
fprintf(stderr, "got a EC\r\n");
		break;
	case IAC_EL:
fprintf(stderr, "got a EL\r\n");
		break;
	case IAC_GA:
fprintf(stderr, "got a GA\r\n");
		break;
	case IAC_SB:
		next_char(sb_command);
		switch (sb_command)
		{
		case OPT_TERMTYPE:
#if DEBUG && 0
fprintf(stderr, "got SB TERMINAL-TYPE\r\n");
#endif
			result= sb_termtype(bp+offset, count-offset);
			if (result<0)
				return result;
			else
				return result+offset;
		default:
fprintf(stderr, "got an unknown SB (skiping)\r\n");
			for (;;)
			{
				next_char(iac);
				if (iac != IAC)
					continue;
				next_char(optsrt);
				if (optsrt == IAC)
					continue;
if (optsrt != IAC_SE)
	fprintf(stderr, "got IAC %d\r\n", optsrt);
				break;
			}
		}
		break;
	case IAC_WILL:
		next_char(optsrt);
		will_option(optsrt);
		break;
	case IAC_WONT:
		next_char(optsrt);
		wont_option(optsrt);
		break;
	case IAC_DO:
		next_char(optsrt);
		do_option(optsrt);
		break;
	case IAC_DONT:
		next_char(optsrt);
		dont_option(optsrt);
		break;
	case IAC:
fprintf(stderr, "got a IAC\r\n");
		break;
	default:
fprintf(stderr, "got unknown command (%d)\r\n", command);
	}
	return offset;
}

static void do_option (int optsrt)
{
	unsigned char reply[3];
	int result;

	switch (optsrt)
	{
	case OPT_TERMTYPE:
		if (WILL_terminal_type)
			return;
		if (!WILL_terminal_type_allowed)
		{
			reply[0]= IAC;
			reply[1]= IAC_WONT;
			reply[2]= optsrt;
		}
		else
		{
			WILL_terminal_type= TRUE;
			term_env= getenv("TERM");
			if (!term_env)
				term_env= "unknown";
			reply[0]= IAC;
			reply[1]= IAC_WILL;
			reply[2]= optsrt;
		}
		break;
	default:
#if DEBUG
		fprintf(stderr, "got a DO (%d)\r\n", optsrt);
		fprintf(stderr, "WONT (%d)\r\n", optsrt);
#endif
		reply[0]= IAC;
		reply[1]= IAC_WONT;
		reply[2]= optsrt;
		break;
	}
	result= writeall(tcp_fd, (char *)reply, 3);
	if (result<0)
		perror("write");
}

static void will_option (int optsrt)
{
	unsigned char reply[3];
	int result;

	switch (optsrt)
	{
	case OPT_ECHO:
		if (DO_echo)
			break;
		if (!DO_echo_allowed)
		{
			reply[0]= IAC;
			reply[1]= IAC_DONT;
			reply[2]= optsrt;
		}
		else
		{
			struct termios termios;

			tcgetattr(0, &termios);
			termios.c_iflag &= ~(ICRNL|IGNCR|INLCR|IXON|IXOFF);
			termios.c_oflag &= ~(OPOST);
			termios.c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN|ISIG);
			tcsetattr(0, TCSANOW, &termios);

			DO_echo= TRUE;
			reply[0]= IAC;
			reply[1]= IAC_DO;
			reply[2]= optsrt;
		}
		result= writeall(tcp_fd, (char *)reply, 3);
		if (result<0)
			perror("write");
		break;
	case OPT_SUPP_GA:
		if (DO_suppress_go_ahead)
			break;
		if (!DO_suppress_go_ahead_allowed)
		{
			reply[0]= IAC;
			reply[1]= IAC_DONT;
			reply[2]= optsrt;
		}
		else
		{
			DO_suppress_go_ahead= TRUE;
			reply[0]= IAC;
			reply[1]= IAC_DO;
			reply[2]= optsrt;
		}
		result= writeall(tcp_fd, (char *)reply, 3);
		if (result<0)
			perror("write");
		break;
	default:
#if DEBUG
		fprintf(stderr, "got a WILL (%d)\r\n", optsrt);
		fprintf(stderr, "DONT (%d)\r\n", optsrt);
#endif
		reply[0]= IAC;
		reply[1]= IAC_DONT;
		reply[2]= optsrt;
		result= writeall(tcp_fd, (char *)reply, 3);
		if (result<0)
			perror("write");
		break;
	}
}

static int writeall (fd, buffer, buf_size)
int fd;
char *buffer;
int buf_size;
{
	int result;

	while (buf_size)
	{
		result= write (fd, buffer, buf_size);
		if (result <= 0)
			return -1;
assert (result <= buf_size);
		buffer += result;
		buf_size -= result;
	}
	return 0;
}

static void dont_option (int optsrt)
{
	switch (optsrt)
	{
	default:
#if DEBUG
		fprintf(stderr, "got a DONT (%d)\r\n", optsrt);
#endif
		break;
	}
}

static void wont_option (int optsrt)
{
	switch (optsrt)
	{
	default:
#if DEBUG
		fprintf(stderr, "got a WONT (%d)\r\n", optsrt);
#endif
		break;
	}
}

static int sb_termtype (char *bp, int count)
{
	unsigned char command, iac, optsrt;
	unsigned char buffer[4];
	int offset, result;

	offset= 0;
	next_char(command);
	if (command == TERMTYPE_SEND)
	{
		buffer[0]= IAC;
		buffer[1]= IAC_SB;
		buffer[2]= OPT_TERMTYPE;
		buffer[3]= TERMTYPE_IS;
		result= writeall(tcp_fd, (char *)buffer,4);
		if (result<0)
			return result;
		count= strlen(term_env);
		if (!count)
		{
			term_env= "unknown";
			count= strlen(term_env);
		}
		result= writeall(tcp_fd, term_env, count);
		if (result<0)
			return result;
		buffer[0]= IAC;
		buffer[1]= IAC_SE;
		result= writeall(tcp_fd, (char *)buffer,2);
		if (result<0)
			return result;

	}
	else
	{
#if DEBUG
 where();
#endif
		fprintf(stderr, "got an unknown command (skipping)\r\n");
	}
	for (;;)
	{
		next_char(iac);
		if (iac != IAC)
			continue;
		next_char(optsrt);
		if (optsrt == IAC)
			continue;
		if (optsrt != IAC_SE)
		{
#if DEBUG
 where();
#endif
			fprintf(stderr, "got IAC %d\r\n", optsrt);
		}
		break;
	}
	return offset;
}

static void fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: ", prog_name);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);

	exit(1);
}

static void usage(void)
{
	fprintf(stderr, "Usage: %s [-e esc-char] host [port]\r\n",
		prog_name);
	exit(1);
}

/*
 * $PchId: ttn.c,v 1.5 2002/05/07 12:06:41 philip Exp $
 */
