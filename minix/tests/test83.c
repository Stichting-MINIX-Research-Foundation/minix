/*
 * test83: test bad network packets
 */

#define DEBUG 0

#if DEBUG
#define dbgprintf(...)	do {						\
				struct timeval time = { };		\
				gettimeofday(&time, NULL);		\
				fprintf(stderr, "[%2d:%.2d:%.2d.%.6d p%d %s:%d] ", \
					(int) ((time.tv_sec / 3600) % 24), \
					(int) ((time.tv_sec / 60) % 60), \
					(int) (time.tv_sec % 60),	\
					time.tv_usec,			\
					getpid(),			\
					__FUNCTION__,			\
					__LINE__);			\
				fprintf(stderr, __VA_ARGS__);		\
				fflush(stderr);				\
			} while (0)
#else
#define	dbgprintf(...)
#endif

#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "common.h"

int max_error = 100;

/* https://tools.ietf.org/html/rfc791 */
struct header_ip {
	uint8_t  ver_ihl; /* Version (4 bits) + IHL (4 bits) */
	uint8_t  tos;     /* Type of Service */
	uint16_t len;     /* Total Length */
	uint16_t id;      /* Identification */
	uint16_t fl_fo;   /* Flags (3 bits) + Fragment Offset (13 bits) */
	uint8_t  ttl;     /* Time to Live */
	uint8_t  prot;    /* Protocol */
	uint16_t cs;      /* Header Checksum */
	uint32_t src;     /* Source Address */
	uint32_t dst;     /* Destination Address */
	uint8_t  opt[16]; /* Options  */
};
#define IP_FLAG_EVIL	(1 << 2)
#define IP_FLAG_DF	(1 << 1)
#define IP_FLAG_MF	(1 << 0)

/* https://tools.ietf.org/html/rfc790 */
#define IP_PROT_ICMP	1
#define IP_PROT_TCP	6
#define IP_PROT_UDP	17

/* https://tools.ietf.org/html/rfc768 */
struct header_udp {
	uint16_t src; /* Source Port */
	uint16_t dst; /* Destination Port */
	uint16_t len; /* Length */
	uint16_t cs;  /* Checksum */
};

struct header_udp_pseudo {
	uint32_t src;
	uint32_t dst;
	uint8_t  zero;
	uint8_t  prot;
	uint16_t len;
};

/* https://tools.ietf.org/html/rfc793 */
struct header_tcp {
	uint16_t src;     /* Source Port */
	uint16_t dst;     /* Destination Port */
	uint32_t seq;     /* Sequence Number */
	uint32_t ack;     /* Acknowledgment Number */
	uint8_t  doff;    /* Data Offset */
	uint8_t  fl;      /* Flags */
	uint16_t win;     /* Window */
	uint16_t cs;      /* Checksum */
	uint16_t uptr;    /* Urgent Pointer */
	uint8_t  opt[16]; /* Options  */
};
#define TCP_FLAG_URG	(1 << 5)
#define TCP_FLAG_ACK	(1 << 4)
#define TCP_FLAG_PSH	(1 << 3)
#define TCP_FLAG_RST	(1 << 2)
#define TCP_FLAG_SYN	(1 << 1)
#define TCP_FLAG_FIN	(1 << 0)

#define PORT_BASE	12345
#define PORT_COUNT_TCP	4
#define PORT_COUNT_UDP	2
#define PORT_COUNT	(PORT_COUNT_TCP + PORT_COUNT_UDP)

#define PORT_BASE_SRC	(PORT_BASE + PORT_COUNT)
#define PORT_COUNT_SRC	79

#define PAYLOADSIZE_COUNT 6
static const size_t payloadsizes[] = {
	0,
	1,
	100,
	1024,
	2345,
	65535 - sizeof(struct header_ip) - sizeof(struct header_udp),
};

/* In its current configuration, this test uses the loopback interface only. */
static uint32_t addrsrc = INADDR_LOOPBACK; /* 127.0.0.1 (localhost) */
static uint32_t addrdst = INADDR_LOOPBACK; /* 127.0.0.1 (localhost) */
static uint32_t addrs[] = {
	INADDR_LOOPBACK, /* 127.0.0.1 (localhost) */
};

#define CLOSE(fd) do { assert(fd >= 0); if (close((fd)) != 0) efmt("close failed"); } while (0);
enum server_action {
	sa_close,
	sa_read,
	sa_selectr,
	sa_selectrw,
	sa_write,
};
static int server_done;

static void server_alarm(int seconds);

static char *sigstr_cat(char *p, const char *s) {
	size_t slen = strlen(s);
	memcpy(p, s, slen);
	return p + slen;
}

static char *sigstr_itoa(char *p, unsigned long n) {
	unsigned digit;
	unsigned long factor = 1000000000UL;
	int first = 1;

	while (factor > 0) {
		digit = (n / factor) % 10;
		if (!first || digit || factor == 1) {
			*(p++) = digit + '0';
			first = 0;
		}
		factor /= 10;
	}
	return p;
}

#if 0
static void dbgprintdata(const void *data, size_t size) {
	size_t addr;
	const unsigned char *p = data;

	for (addr = 0; addr < size; addr++) {
		if (addr % 16 == 0) {
			if (addr > 0) fprintf(stderr, "\n");
			fprintf(stderr, "%.4zx", addr);
		}
		fprintf(stderr, " %.2x", p[addr]);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}
#endif

static void dbgprint_sig(const char *name) {
#if DEBUG
	char buf[256];
	char *p = buf;

	/* fprintf not used to be signal safe */
	p = sigstr_cat(p, "[");
	p = sigstr_itoa(p, getpid());
	p = sigstr_cat(p, "] ");
	p = sigstr_cat(p, name);
	p = sigstr_cat(p, "\n");
	write(STDERR_FILENO, buf, p - buf);
#endif
}

#define SIGNAL(sig, handler) (signal_checked((sig), (handler), #sig, __FILE__, __FUNCTION__, __LINE__))

static void signal_checked(int sig, void (* handler)(int), const char *signame,
	const char *file, const char *func, int line) {
	char buf[256];
	char *p = buf;
	struct sigaction sa = {
		.sa_handler = handler,
	};

	if (sigaction(sig, &sa, NULL) == 0) return;

	/* efmt not used to be signal safe */
	p = sigstr_cat(p, "[");
	p = sigstr_cat(p, file);
	p = sigstr_cat(p, ":");
	p = sigstr_itoa(p, line);
	p = sigstr_cat(p, "] error: sigaction(");
	p = sigstr_cat(p, signame);
	p = sigstr_cat(p, ") failed in function ");
	p = sigstr_cat(p, func);
	p = sigstr_cat(p, ": ");
	p = sigstr_itoa(p, errno);
	p = sigstr_cat(p, "\n");
	write(STDERR_FILENO, buf, p - buf);
	errct++;
}

static void server_sigusr1(int signo) {
	dbgprint_sig("SIGUSR1");

	/* terminate on the first opportunity */
	server_done = 1;

	/* in case signal is caught before a blocking operation,
	 * keep interrupting
	 */
	server_alarm(1);
}

static void server_stop(pid_t pid) {

	if (pid < 0) return;

	dbgprintf("sending SIGUSR1 to child %d\n", (int) pid);
	if (kill(pid, SIGUSR1) != 0) efmt("kill failed");
}

static void server_wait(pid_t pid) {
	int exitcode, status;
	pid_t r;

	if (pid < 0) return;

	dbgprintf("waiting for child %d\n", (int) pid);
	r = waitpid(pid, &status, 0);
	if (r != pid) {
		efmt("waitpid failed");
		return;
	}

	if (WIFEXITED(status)) {
		exitcode = WEXITSTATUS(status);
		if (exitcode < 0) {
			efmt("negative exit code from child %d\n", (int) pid);
		} else {
			dbgprintf("child exited exitcode=%d\n", exitcode);
			errct += exitcode;
		}
	} else if (WIFSIGNALED(status)) {
		efmt("child killed by signal %d", WTERMSIG(status));
	} else {
		efmt("child has unexpected exit status 0x%x", status);
	}
}

static void server_sigalrm(int signum) {
	server_alarm(1);
}

static void server_alarm(int seconds) {
	SIGNAL(SIGALRM, server_sigalrm);
	alarm(seconds);
}

static void server_no_alarm(void) {
	int errno_old = errno;
	alarm(0);
	SIGNAL(SIGALRM, SIG_DFL);
	errno = errno_old;
}

static int server_rw(int fd, int is_write, int *success) {
	char buf[4096];
	ssize_t r;

	/* return 0 means close connection, *success=0 means stop server */

	if (is_write) {
		/* ignore SIGPIPE */
		SIGNAL(SIGPIPE, SIG_IGN);

		/* initialize buffer */
		memset(buf, -1, sizeof(buf));
	}

	/* don't block for more than 1s */
	server_alarm(1);

	/* perform read or write operation */
	dbgprintf("server_rw waiting is_write=%d\n", is_write);
	r = is_write ? write(fd, buf, sizeof(buf)) : read(fd, buf, sizeof(buf));

	/* stop alarm (preserves errno) */
	server_no_alarm();

	/* handle read/write result */
	if (r >= 0) {
		dbgprintf("server_rw done\n");
		*success = 1;
		return r > 0;
	}

	switch (errno) {
	case EINTR:
		dbgprintf("server_rw interrupted\n");
		*success = 1;
		return 0;
	case ECONNRESET:
		dbgprintf("server_rw connection reset\n");
		*success = 1;
		return 0;
	case EPIPE:
		if (is_write) {
			dbgprintf("server_rw EPIPE\n");
			*success = 1;
			return 0;
		}
		/* fall through */
	default:
		efmt("%s failed", is_write ? "write" : "read");
		*success = 0;
		return 0;
	}
}

static int server_select(int fd, int is_rw, int *success,
	enum server_action *actionnext) {
	int r;
	fd_set readfds, writefds;
	struct timeval timeout = { .tv_sec = 1, .tv_usec = 0 };

	/* return 0 means close connection, *success=0 means stop server */

	/* prepare fd sets */
	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	FD_ZERO(&writefds);
	if (is_rw) FD_SET(fd, &writefds);

	/* perform select */
	errno = 0;
	dbgprintf("server_select waiting\n");
	r = select(fd + 1, &readfds, &writefds, NULL, &timeout);

	/* handle result */
	if (r < 0) {
		switch (errno) {
		case EINTR:
			dbgprintf("server_select interrupted\n");
			*success = 1;
			return 0;
		default:
			efmt("select failed");
			*success = 0;
			return 0;
		}
	}
	if (r == 0) {
		dbgprintf("server_select nothing available\n");
		*success = 1;
		return 0;
	}

	if (FD_ISSET(fd, &readfds)) {
		dbgprintf("server_select read available\n");
		*actionnext = sa_read;
		*success = 1;
		return 1;
	} else if (FD_ISSET(fd, &writefds)) {
		dbgprintf("server_select write available\n");
		*actionnext = sa_write;
		*success = 1;
		return 1;
	}

	*success = 0;
	efmt("select did not set fd");
	return 0;
}

static int server_accept(int servfd, int type, enum server_action action) {
	enum server_action actionnext;
	struct sockaddr addr;
	socklen_t addrsize;
	int connfd;
	int success = 0;

	/* if connection-oriented, accept a conmection */
	if (type == SOCK_DGRAM) {
		connfd = servfd;
	} else {
		dbgprintf("server_accept waiting for connection\n");
		addrsize = sizeof(addr);
		connfd = accept(servfd, &addr, &addrsize);
		if (connfd < 0) {
			switch (errno) {
			case EINTR:
				dbgprintf("server_accept interrupted\n");
				return 1;
			default:
				efmt("cannot accept connection");
				return 0;
			}
		}
		dbgprintf("server_accept new connection\n");
	}

	/* perform requested action while the connection is open */
	actionnext = action;
	while (!server_done) {
		switch (actionnext) {
		case sa_close:
			success = 1;
			goto cleanup;
		case sa_read:
			if (!server_rw(connfd, 0, &success)) goto cleanup;
			actionnext = action;
			break;
		case sa_selectr:
		case sa_selectrw:
			if (!server_select(connfd, actionnext == sa_selectrw,
				&success, &actionnext)) {
				goto cleanup;
			}
			break;
		case sa_write:
			if (!server_rw(connfd, 1, &success)) goto cleanup;
			actionnext = action;
			break;
		default:
			efmt("bad server action");
			success = 0;
			goto cleanup;
		}
	}

	/* socket connection socket */
cleanup:
	dbgprintf("server_accept done success=%d\n", success);
	if (connfd != servfd) CLOSE(connfd);
	return success;
}

static pid_t server_start(int type, int port, enum server_action action) {
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	int fd, on;
	pid_t pid = -1;

	dbgprintf("server_start port %d\n", port);

	/* create socket */
	fd = socket(AF_INET, type, 0);
	if (fd < 0) {
		efmt("cannot create socket");
		goto cleanup;
	}

	on = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
		efmt("cannot set SO_REUSEADDR option on socket");
		goto cleanup;
	}

	/* bind socket */
	if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
		efmt("cannot bind socket");
		goto cleanup;
	}

	/* make it a server socket if needed */
	if (type != SOCK_DGRAM) {
		if (listen(fd, 5) != 0) {
			efmt("cannot listen on socket");
			goto cleanup;
		}
	}

	/* intercept SIGUSR1 in case parent wants the server to stop */
	SIGNAL(SIGUSR1, server_sigusr1);

	/* fork; parent continues, child becomes server */
	pid = fork();
	if (pid < 0) {
		efmt("cannot create socket");
		goto cleanup;
	}
	if (pid) goto cleanup;

	/* server loop */
	dbgprintf("server_start child\n");
	while (!server_done && server_accept(fd, type, action)) {}
	dbgprintf("server_start child returns\n");

	CLOSE(fd);
	exit(errct);

cleanup:
	dbgprintf("server_start parent returns pid=%d\n", (int) pid);
	if (fd >= 0) CLOSE(fd);
	return pid;
}

static ssize_t send_packet_raw(int fd, const void *buf, size_t size) {
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	return sendto(fd, buf, size, 0, (struct sockaddr *)&sin, sizeof(sin));
}

enum settings_ip {
	si_bad_version   = (1 <<  0),
	si_bad_ihl_small = (1 <<  1),
	si_bad_ihl_big   = (1 <<  2),
	si_bad_len_small = (1 <<  3),
	si_bad_len_big   = (1 <<  4),
	si_bad_len_huge  = (1 <<  5),
	si_bad_cs        = (1 <<  6),
	si_zero_cs       = (1 <<  7),

	si_flag_evil     = (1 <<  8),
	si_flag_df       = (1 <<  9),
	si_flag_mf       = (1 << 10),

	si_opt_end       = (1 << 11),
	si_opt_topsec    = (1 << 12),
	si_opt_nop       = (1 << 13),
	si_opt_badopt    = (1 << 14),
	si_opt_badpad    = (1 << 15),
};

enum settings_udp {
	su_bad_len_small = (1 <<  0),
	su_bad_len_big   = (1 <<  1),
	su_bad_len_huge  = (1 <<  2),
	su_bad_cs        = (1 <<  3),
	su_zero_cs       = (1 <<  4),
};

enum fragmode_ip {
	fi_as_needed,
	fi_one,
	fi_two,
	fi_frag_tiny,
	fi_frag_overlap,
	fi_frag_first,
	fi_frag_last,
	fi_frag_repeat,
	fi_fo_max,
};

static uint16_t checksum_ip(const void *header, size_t headersize) {
	const uint16_t *p = header;
	uint32_t sum = 0;

	while (headersize > 0) {
		assert(headersize >= sizeof(*p));
		sum += ntohs(*p);
		headersize -= sizeof(*p);
		p++;
	}
	sum += sum >> 16;
	return htons(~sum);
}

static void send_packet_ip_base(
	int fd,
	enum settings_ip ipsettings,
	uint8_t  tos,
	uint16_t id,
	uint16_t fo,
	uint8_t  ttl,
	uint8_t  prot,
	uint32_t srcip,
	uint32_t dstip,
	const void *payload,
	size_t payloadsize) {
	uint8_t ver = (ipsettings & si_bad_version) ? 3 : 4;
	uint8_t ihl, ihl_fuzzed;
	uint16_t fl = ((ipsettings & si_flag_evil) ? IP_FLAG_EVIL : 0) |
	              ((ipsettings & si_flag_df)   ? IP_FLAG_DF   : 0) |
	              ((ipsettings & si_flag_mf)   ? IP_FLAG_MF   : 0);
	uint16_t len;
	int optlen;
	struct header_ip header = {
		.tos     = tos,
		.id      = htons(id),
		.fl_fo   = (fl << 13) | fo, /* no htons(), lwip swaps this */
		.ttl     = ttl,
		.prot    = prot,
		.cs      = 0,
		.src     = htonl(srcip),
		.dst     = htonl(dstip),
	};
	char packet[6536];
	size_t packetsize;
	ssize_t r;

	dbgprintf("sending IP packet src=%d.%d.%d.%d dst=%d.%d.%d.%d "
		"payloadsize=%zu id=0x%.4x fragoff=%d%s\n",
		(uint8_t) (srcip >> 24), (uint8_t) (srcip >> 16),
		(uint8_t) (srcip >> 8), (uint8_t) (srcip >> 0),
		(uint8_t) (dstip >> 24), (uint8_t) (dstip >> 16),
		(uint8_t) (dstip >> 8), (uint8_t) (dstip >> 0),
		payloadsize, id, fo, (ipsettings & si_flag_mf) ? " (MF)" : "");

	optlen = 0;
	if (ipsettings & si_opt_badpad) memset(header.opt, -1, sizeof(header.opt));
	if (ipsettings & si_opt_nop) header.opt[optlen++] = 0x01;
	if (ipsettings & si_opt_topsec) {
		header.opt[optlen++] = 0x82;
		header.opt[optlen++] = 0x0b;
		header.opt[optlen++] = 0x6b; /* S: top secret */
		header.opt[optlen++] = 0xc5; /* S: top secret */
		header.opt[optlen++] = 0x00; /* C */
		header.opt[optlen++] = 0x00; /* C */
		header.opt[optlen++] = 'A'; /* H */
		header.opt[optlen++] = 'B'; /* H */
		header.opt[optlen++] = 'C'; /* TCC */
		header.opt[optlen++] = 'D'; /* TCC */
		header.opt[optlen++] = 'E'; /* TCC */
	}
	if (ipsettings & si_opt_badopt) header.opt[optlen++] = 0xff;
	if (ipsettings & si_opt_end) header.opt[optlen++] = 0x00;
	assert(optlen <= sizeof(header.opt));

	ihl = ihl_fuzzed = (20 + optlen + 3) / 4;
	if (ipsettings & si_bad_ihl_small) ihl_fuzzed = 4;
	if (ipsettings & si_bad_ihl_big) ihl_fuzzed = 15;
	header.ver_ihl = (ver << 4) | ihl_fuzzed;

	len = ihl * 4 + payloadsize;
	if (ipsettings & si_bad_len_small) len = ihl * 4 - 1;
	if (ipsettings & si_bad_len_big) len += 1;
	if (ipsettings & si_bad_len_huge) len = 0xffff;
	header.len = len; /* no htons(), lwip swaps this */

	packetsize = ihl * 4 + payloadsize;
	if (packetsize > sizeof(packet)) {
		payloadsize = sizeof(packet) - ihl * 4;
		packetsize = sizeof(packet);
	}

	header.cs = checksum_ip(&header, ihl * 4);
	if (ipsettings & si_zero_cs) header.cs = 0;
	if (ipsettings & si_bad_cs) header.cs += 1;

	memset(packet, 0, sizeof(packet));
	memcpy(packet, &header, ihl * 4);
	memcpy(packet + ihl * 4, payload, payloadsize);

	errno = 0;
	r = send_packet_raw(fd, packet, packetsize);
	if (r == -1 && errno == EPACKSIZE &&
		(packetsize < 60 || packetsize > 1514)) {
		return;
	}
	if (r != packetsize) {
		efmt("write to network interface failed");
	}
}

static void send_packet_ip(
	int fd,
	enum settings_ip ipsettings,
	uint8_t tos,
	uint16_t id,
	uint8_t ttl,
	uint8_t prot,
	uint32_t srcip,
	uint32_t dstip,
	enum fragmode_ip fragmode,
	const void *payload,
	size_t payloadsize) {
	enum settings_ip flags;
	size_t fragcount = 1;
	size_t fragsize, fragsizecur;
	size_t fragstart = 0;
	size_t fragstep;

	switch (fragmode) {
	case fi_as_needed:
		fragsize = fragstep = 1500;
		fragcount = (payloadsize + fragsize - 1) / fragsize;
		break;
	case fi_one:
	case fi_fo_max:
		fragsize = fragstep = payloadsize;
		break;
	case fi_two:
		fragcount = 2;
		fragsize = fragstep = (payloadsize + 1) / 2;
		break;
	case fi_frag_tiny:
		fragcount = (payloadsize >= 100) ? 100 :
			(payloadsize < 1) ? 1 : payloadsize;
		fragsize = fragstep = (payloadsize + fragcount - 1) / fragcount;
		break;
	case fi_frag_overlap:
		fragcount = 2;
		fragsize = (payloadsize * 2 + 2) / 3;
		fragstep = (payloadsize + 1) / 2;
		break;
	case fi_frag_first:
		fragcount = 1;
		fragsize = fragstep = (payloadsize + 1) / 2;
		break;
	case fi_frag_last:
		fragcount = 1;
		fragsize = fragstep = (payloadsize + 1) / 2;
		break;
	case fi_frag_repeat:
		fragcount = 2;
		fragsize = payloadsize;
		fragstep = 0;
		break;
	default:
		abort();
	}

	while (fragcount > 0) {
		if (fragstart >= payloadsize) {
			fragsizecur = 0;
		} else if (payloadsize - fragstart < fragsize) {
			fragsizecur = payloadsize - fragstart;
		} else {
			fragsizecur = fragsize;
		}

		flags = 0;
		if (fragstart + fragsizecur < payloadsize) flags |= si_flag_mf;
		send_packet_ip_base(
			fd,
			ipsettings | flags,
			tos,
			id,
			(fragmode == fi_fo_max) ? 0x1fff : fragstart,
			ttl,
			prot,
			srcip,
			dstip,
			(uint8_t *) payload + fragstart,
			fragsizecur);

		fragcount--;
		fragstart += fragstep;
	}
}

static uint32_t checksum_udp_sum(const void *buf, size_t size) {
	const uint16_t *p = buf;
	uint32_t sum = 0;

	while (size > 0) {
		assert(size >= sizeof(*p));
		sum += ntohs(*p);
		size -= sizeof(*p);
		p++;
	}
	return sum;
}

static uint16_t checksum_udp(
	uint32_t srcip,
	uint32_t dstip,
	uint8_t prot,
	const void *packet,
	size_t packetsize) {
	uint32_t sum = 0;
	struct header_udp_pseudo header = {
		.src = htonl(srcip),
		.dst = htonl(dstip),
		.zero = 0,
		.prot = prot,
		.len = htons(packetsize),
	};

	sum = checksum_udp_sum(&header, sizeof(header)) +
		checksum_udp_sum(packet, packetsize + packetsize % 2);
	sum += sum >> 16;
	return ntohs(~sum);
}

static void send_packet_udp(
	int fd,
	enum settings_ip ipsettings,
	uint8_t tos,
	uint16_t id,
	uint8_t ttl,
	uint8_t prot,
	uint32_t srcip,
	uint32_t dstip,
	enum fragmode_ip fragmode,
	enum settings_udp udpsettings,
	uint16_t srcport,
	uint16_t dstport,
	const void *payload,
	size_t payloadsize) {
	uint16_t len;
	struct header_udp header = {
		.src = htons(srcport),
		.dst = htons(dstport),
		.cs = 0,
	};
	char packet[65536];
	size_t packetsize;

	dbgprintf("sending UDP packet srcport=%d dstport=%d payloadsize=%zu\n",
		srcport, dstport, payloadsize);

	len = sizeof(struct header_udp) + payloadsize;
	if (udpsettings & su_bad_len_small) len = sizeof(struct header_udp) - 1;
	if (udpsettings & su_bad_len_big) len += 1;
	if (udpsettings & su_bad_len_huge) len = 65535 - sizeof(struct header_ip);
	header.len = htons(len);

	packetsize = sizeof(header) + payloadsize;
	assert(packetsize <= sizeof(packet));

	memcpy(packet, &header, sizeof(header));
	memcpy(packet + sizeof(header), payload, payloadsize);
	if (packetsize % 2) packet[packetsize] = 0;

	header.cs = checksum_udp(srcip, dstip, prot, packet, packetsize);
	if (udpsettings & su_zero_cs) header.cs = 0;
	if (udpsettings & su_bad_cs) header.cs += 1;

	memcpy(packet, &header, sizeof(header));
	send_packet_ip(
		fd,
		ipsettings,
		tos,
		id,
		ttl,
		prot,
		srcip,
		dstip,
		fragmode,
		packet,
		packetsize);
}

struct send_packet_udp_simple_params {
	int fd;
	enum settings_ip ipsettings;
	uint8_t tos;
	uint16_t *id;
	uint8_t ttl;
	uint8_t prot;
	uint32_t srcip;
	uint32_t dstip;
	enum fragmode_ip fragmode;
	enum settings_udp udpsettings;
	uint16_t srcport;
	uint16_t dstport;
	size_t payloadsize;
};

static void send_packet_udp_simple(
	const struct send_packet_udp_simple_params *params) {
	int i;
	char payload[65536];

	assert(params->payloadsize <= sizeof(payload));
	for (i = 0; i < params->payloadsize; i++) {
		payload[i] = *params->id + i;
	}

	send_packet_udp(
		params->fd,
		params->ipsettings,
		params->tos,
		*params->id,
		params->ttl,
		params->prot,
		params->srcip,
		params->dstip,
		params->fragmode,
		params->udpsettings,
		params->srcport,
		params->dstport,
		payload,
		params->payloadsize);
	*params->id += 5471;
}

static void send_packets_ip_settings(
	const struct send_packet_udp_simple_params *paramsbase) {
	struct send_packet_udp_simple_params params;
	int i;
	enum settings_ip ipsettings[] = {
		0,
		si_bad_version,
		si_bad_ihl_small,
		si_bad_ihl_big,
		si_bad_len_small,
		si_bad_len_big,
		si_bad_len_huge,
		si_bad_cs,
		si_zero_cs,
		si_flag_evil,
		si_flag_df,
		si_flag_mf,
		si_opt_end,
		si_opt_topsec,
		si_opt_nop,
		si_opt_badopt,
		si_opt_nop | si_opt_end | si_opt_badpad,
	};
	uint8_t ttls[] = { 0, 1, 127, 128, 255 };

	/* various types of flags/options/corruptions */
	params = *paramsbase;
	for (i = 0; i < 17; i++) {
		params.ipsettings = ipsettings[i];
		send_packet_udp_simple(&params);
	}

	/* various TTL settings */
	params = *paramsbase;
	for (i = 0; i < 5; i++) {
		params.ttl = ttls[i];
		send_packet_udp_simple(&params);
	}
}

static void send_packets_ip(int fd) {
	enum fragmode_ip fragmode;
	int i, j;
	uint16_t id = 0;
	struct send_packet_udp_simple_params params;
	const struct send_packet_udp_simple_params paramsbase = {
		.fd            = fd,
		.ipsettings    = 0,
		.tos           = 0,
		.id            = &id,
		.ttl           = 10,
		.prot          = IP_PROT_UDP,
		.srcip         = addrsrc,
		.dstip         = addrdst,
		.fragmode      = fi_as_needed,
		.udpsettings   = 0,
		.srcport       = PORT_BASE + 0,
		.dstport       = PORT_BASE + 1,
		.payloadsize   = 1234,
	};

	/* send packets with various payload sizes and corruptions */
	params = paramsbase;
	for (i = 0; i < PAYLOADSIZE_COUNT; i++) {
		params.payloadsize = payloadsizes[i];
		send_packets_ip_settings(&params);
	}

	/* send packets with various addresses and corruptions */
	params = paramsbase;
	for (i = 0; i < __arraycount(addrs); i++) {
	for (j = 0; j < __arraycount(addrs); j++) {
		params.srcip = addrs[i];
		params.dstip = addrs[j];
		send_packets_ip_settings(&params);
	}
	}

	/* send valid packets with various fragmentation settings */
	params = paramsbase;
	for (i = 0; i < PAYLOADSIZE_COUNT; i++) {
	for (fragmode = fi_as_needed; fragmode <= fi_fo_max; fragmode++) {
		params.payloadsize = payloadsizes[i];
		params.fragmode = fragmode;
		send_packet_udp_simple(&params);
	}
	}

	/* send a packet for each protocol */
	params = paramsbase;
	for (i = 0; i < 256; i++) {
		params.prot = i;
		send_packet_udp_simple(&params);
	}

	/* send a packet for each tos */
	params = paramsbase;
	for (i = 0; i < 256; i++) {
		params.tos = i;
		send_packet_udp_simple(&params);
	}
}

static void send_packets_udp(int fd) {
	int i, j, k;
	uint16_t id = 0;
	struct send_packet_udp_simple_params params;
	const struct send_packet_udp_simple_params paramsbase = {
		.fd            = fd,
		.ipsettings    = 0,
		.tos           = 0,
		.id            = &id,
		.ttl           = 10,
		.prot          = IP_PROT_UDP,
		.srcip         = addrsrc,
		.dstip         = addrdst,
		.fragmode      = fi_as_needed,
		.udpsettings   = 0,
		.srcport       = PORT_BASE + 0,
		.dstport       = PORT_BASE + 1,
		.payloadsize   = 1234,
	};
	uint16_t ports[] = {
		0,
		PORT_BASE + 0,
		PORT_BASE + 1,
		32767,
		65535,
	};
	enum settings_udp udpsettings[] = {
		0,
		su_bad_len_small,
		su_bad_len_big,
		su_bad_len_huge,
		su_bad_cs,
		su_zero_cs,
	};

	/* send packets with various corruptions */
	params = paramsbase;
	for (i = 0; i < 6; i++) {
		params.udpsettings = udpsettings[i];
		send_packet_udp_simple(&params);
	}

	/* send packets with various addresses and ports */
	params = paramsbase;
	for (i = 0; i < __arraycount(addrs); i++) {
	for (j = 0; j < __arraycount(addrs); j++) {
	for (k = 0; k < 5; k++) {
		params.srcip = addrs[i];
		params.dstip = addrs[j];
		params.dstport = ports[k];
		send_packet_udp_simple(&params);
	}
	}
	}
	params = paramsbase;
	for (i = 0; i < __arraycount(addrs); i++) {
	for (j = 0; j < 5; j++) {
	for (k = 0; k < 5; k++) {
		params.dstip = addrs[i];
		params.srcport = ports[j];
		params.dstport = ports[k];
		send_packet_udp_simple(&params);
	}
	}
	}
}

enum settings_tcp {
	st_bad_doff_small  = (1 <<  0),
	st_bad_doff_big    = (1 <<  1),
	st_bad_doff_huge   = (1 <<  2),
	st_bad_cs          = (1 <<  3),
	st_zero_cs         = (1 <<  4),
	st_opt_end         = (1 <<  5),
	st_opt_nop         = (1 <<  6),
	st_opt_mss_small   = (1 <<  7),
	st_opt_mss_big     = (1 <<  8),
	st_opt_mss_huge    = (1 <<  9),
	st_opt_badpad      = (1 << 10),
};

static void send_packet_tcp(
	int fd,
	enum settings_ip ipsettings,
	uint8_t tos,
	uint16_t id,
	uint8_t ttl,
	uint8_t prot,
	uint32_t srcip,
	uint32_t dstip,
	enum fragmode_ip fragmode,
	enum settings_tcp tcpsettings,
	uint16_t srcport,
	uint16_t dstport,
	uint32_t seq,
	uint32_t ack,
	uint8_t fl,
	uint16_t win,
	uint16_t uptr,
	const void *payload,
	size_t payloadsize) {
	uint8_t doff, doff_fuzzed;
	int optlen;
	struct header_tcp header = {
		.src  = htons(srcport),
		.dst  = htons(dstport),
		.seq  = htonl(seq),
		.ack  = htonl(ack),
		.fl   = fl,
		.win  = htons(win),
		.cs   = 0,
		.uptr = htons(uptr),
	};
	char packet[65536];
	size_t packetsize;

	dbgprintf("sending TCP packet srcport=%d dstport=%d fl=%s%s%s%s%s%s "
		"payloadsize=%zu\n", srcport, dstport,
		(fl & TCP_FLAG_URG) ? "  URG" : "",
		(fl & TCP_FLAG_ACK) ? "  ACK" : "",
		(fl & TCP_FLAG_PSH) ? "  PSH" : "",
		(fl & TCP_FLAG_RST) ? "  RST" : "",
		(fl & TCP_FLAG_SYN) ? "  SYN" : "",
		(fl & TCP_FLAG_FIN) ? "  FIN" : "",
		payloadsize);

	optlen = 0;
	if (tcpsettings & st_opt_badpad) memset(header.opt, -1, sizeof(header.opt));
	if (tcpsettings & st_opt_nop) header.opt[optlen++] = 0x01;
	if (tcpsettings & st_opt_mss_small) {
		header.opt[optlen++] = 0x02;
		header.opt[optlen++] = 0x04;
		header.opt[optlen++] = 0x00;
		header.opt[optlen++] = 0x00;
	}
	if (tcpsettings & st_opt_mss_big) {
		header.opt[optlen++] = 0x02;
		header.opt[optlen++] = 0x04;
		header.opt[optlen++] = 0x10;
		header.opt[optlen++] = 0x00;
	}
	if (tcpsettings & st_opt_mss_huge) {
		header.opt[optlen++] = 0x02;
		header.opt[optlen++] = 0x04;
		header.opt[optlen++] = 0xff;
		header.opt[optlen++] = 0xff;
	}
	if (tcpsettings & st_opt_end) header.opt[optlen++] = 0x00;

	doff = doff_fuzzed = (20 + optlen + 3) / 4;
	if (tcpsettings & su_bad_len_small) doff_fuzzed -= 1;
	if (tcpsettings & su_bad_len_big) doff_fuzzed += 1;
	if (tcpsettings & su_bad_len_huge) doff_fuzzed = 15;
	header.doff = doff_fuzzed << 4;

	packetsize = doff * 4 + payloadsize;
	assert(packetsize <= sizeof(packet));

	memcpy(packet, &header, sizeof(header));
	memcpy(packet + sizeof(header), payload, payloadsize);
	if (packetsize % 2) packet[packetsize] = 0;

	header.cs = checksum_udp(srcip, dstip, prot, packet, packetsize);
	if (tcpsettings & su_zero_cs) header.cs = 0;
	if (tcpsettings & su_bad_cs) header.cs += 1;

	memcpy(packet, &header, sizeof(header));
	send_packet_ip(
		fd,
		ipsettings,
		tos,
		id,
		ttl,
		prot,
		srcip,
		dstip,
		fragmode,
		packet,
		packetsize);
}

struct send_packet_tcp_simple_params {
	int fd;
	enum settings_ip ipsettings;
	uint8_t tos;
	uint16_t *id;
	uint8_t ttl;
	uint8_t prot;
	uint32_t srcip;
	uint32_t dstip;
	enum fragmode_ip fragmode;
	enum settings_tcp tcpsettings;
	uint16_t srcport;
	uint16_t dstport;
	uint32_t seq;
	uint32_t ack;
	uint8_t fl;
	uint16_t win;
	uint16_t uptr;
	size_t payloadsize;
};

static void send_packet_tcp_simple(
	const struct send_packet_tcp_simple_params *params) {
	int i;
	char payload[65536];

	if (!params->srcip || !params->dstip) return; /* crashes QEMU */

	assert(params->payloadsize <= sizeof(payload));
	for (i = 0; i < params->payloadsize; i++) {
		payload[i] = *params->id + i;
	}
	send_packet_tcp(
		params->fd,
		params->ipsettings,
		params->tos,
		*params->id,
		params->ttl,
		params->prot,
		params->srcip,
		params->dstip,
		params->fragmode,
		params->tcpsettings,
		params->srcport,
		params->dstport,
		params->seq,
		params->ack,
		params->fl,
		params->win,
		params->uptr,
		payload,
		params->payloadsize);
	*params->id += 5471;
}

static void send_packets_tcp(int fd) {
	int i, j, k;
	uint16_t id = 0;
	const struct send_packet_tcp_simple_params paramsbase = {
		.fd          = fd,
		.ipsettings  = 0,
		.tos         = 0,
		.id          = &id,
		.ttl         = 10,
		.prot        = IP_PROT_TCP,
		.srcip       = addrsrc,
		.dstip       = addrdst,
		.fragmode    = fi_as_needed,
		.tcpsettings = 0,
		.srcport     = PORT_BASE + 0,
		.dstport     = PORT_BASE + 1,
		.seq         = 0x12345678,
		.ack         = 0x87654321,
		.fl          = TCP_FLAG_SYN,
		.win         = 4096,
		.uptr        = 0,
		.payloadsize = 1234,
	};
	uint16_t payloadsizes[] = {
		0,
		1,
		999,
		1500,
		1600,
		9999,
	};
	uint16_t ports[] = {
		0,
		PORT_BASE + 0,
		PORT_BASE + 1,
		PORT_BASE + 2,
		PORT_BASE + 3,
		32767,
		65535,
	};
	enum settings_tcp tcpsettings[] = {
		0,
		st_bad_doff_small,
		st_bad_doff_big,
		st_bad_doff_huge,
		st_bad_cs,
		st_zero_cs,
		st_opt_end,
		st_opt_nop,
		st_opt_mss_small,
		st_opt_mss_big,
		st_opt_mss_huge,
		st_opt_badpad,
	};
	struct send_packet_tcp_simple_params params;

	/* send packets with various corruptions */
	params = paramsbase;
	for (i = 0; i < 12; i++) {
		params.tcpsettings = tcpsettings[i];
		send_packet_tcp_simple(&params);
	}

	/* send packets with various addresses and ports */
	params = paramsbase;
	for (i = 0; i < __arraycount(addrs); i++) {
	for (j = 0; j < __arraycount(addrs); j++) {
	for (k = 0; k < 7; k++) {
		params.srcip = addrs[i];
		params.dstip = addrs[j];
		params.dstport = ports[k];
		send_packet_tcp_simple(&params);
	}
	}
	}
	params = paramsbase;
	for (i = 0; i < __arraycount(addrs); i++) {
	for (j = 0; j < 7; j++) {
	for (k = 0; k < 7; k++) {
		params.dstip = addrs[i];
		params.srcport = ports[j];
		params.dstport = ports[k];
		send_packet_tcp_simple(&params);
	}
	}
	}

	/* send packets with different sequence numbers */
	params = paramsbase;
	for (i = 0; i < 16; i++) {
		params.seq = 0x1fffffff;
		send_packet_tcp_simple(&params);
	}

	/* send packets with all combinations of flags */
	params = paramsbase;
	for (i = 0; i < 256; i++) {
		params.fl = i;
		send_packet_tcp_simple(&params);
	}

	/* send packets with different window sizes */
	params = paramsbase;
	for (i = 0; i < 6; i++) {
		params.win = payloadsizes[i];
		send_packet_tcp_simple(&params);
	}

	/* send packets with different payload sizes */
	params = paramsbase;
	for (i = 0; i < 6; i++) {
		params.payloadsize = payloadsizes[i];
		send_packet_tcp_simple(&params);
	}
}

static void recv_packets_nb(int fd) {
	char buf[4096];
	int flags;
	ssize_t r;

	flags = fcntl(fd, F_GETFL);
	if (flags < 0) {
		efmt("fcntl(F_GETFL) failed");
		return;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		efmt("fcntl(F_SETFL) failed");
		return;
	}

	for (;;) {
		errno = 0;
		r = read(fd, buf, sizeof(buf));
		if (r <= 0) {
			if (errno != EAGAIN) efmt("nb read failed");
			dbgprintf("no more packets to receive\n");
			break;
		}
		dbgprintf("received packet of size %zd\n", r);
	}

	if (fcntl(fd, F_SETFL, flags) == -1) {
		efmt("fcntl(F_SETFL) failed");
		return;
	}
}

static struct timeval gettimeofday_checked(void) {
	struct timeval time = {};

	if (gettimeofday(&time, NULL) != 0) {
		efmt("gettimeofday failed");
	}
	return time;
}

static int timeval_cmp(const struct timeval *x, const struct timeval *y) {
	if (x->tv_sec < y->tv_sec) return -1;
	if (x->tv_sec > y->tv_sec) return 1;
	if (x->tv_usec < y->tv_usec) return -1;
	if (x->tv_usec > y->tv_usec) return 1;
	return 0;
}

static struct timeval timeval_sub(struct timeval x, struct timeval y) {
	struct timeval z;

	/* no negative result allowed */
	if (timeval_cmp(&x, &y) < 0) {
		memset(&z, 0, sizeof(z));
	} else {
		/* no negative tv_usec allowed */
		if (x.tv_usec < y.tv_usec) {
			x.tv_sec -= 1;
			x.tv_usec += 1000000;
		}

		/* perform subtraction */
		z.tv_sec = x.tv_sec - y.tv_sec;
		z.tv_usec = x.tv_usec - y.tv_usec;
	}
	return z;
}

static size_t recv_packet_select(
	int fd,
	void *buf,
	size_t size,
	const struct timeval *deadline) {
	int nfds;
	ssize_t r;
	fd_set readfds;
	struct timeval timeout = timeval_sub(*deadline, gettimeofday_checked());

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);
	errno = 0;
	nfds = select(fd + 1, &readfds, NULL, NULL, &timeout);
	if (nfds < 0 || nfds > 1) {
		efmt("select failed");
		return 0;
	}

	if (nfds == 0) {
		if (FD_ISSET(fd, &readfds)) efmt("select spuriously set fd");
		dbgprintf("no more packets to receive\n");
		return 0;
	}

	if (!FD_ISSET(fd, &readfds)) {
		efmt("select did not set fd");
		return 0;
	}

	r = read(fd, buf, size);
	if (r <= 0) {
		efmt("read failed");
		return 0;
	}
	dbgprintf("received packet of size %zd\n", r);

	return r;
}

static void recv_packets_select(int fd) {
	char buf[4096];
	struct timeval deadline = gettimeofday_checked();

	deadline.tv_sec++;
	while (recv_packet_select(fd, buf, sizeof(buf), &deadline)) { }
}

static int open_raw_socket(int broadcast) {
	int fd, on;

	fd = socket(AF_INET, SOCK_RAW, IPPROTO_IP);
	if (fd < 0) efmt("cannot create raw socket");

	on = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) != 0)
		efmt("ioctl(IP_HDRINCL) failed");

	return fd;
}

static void do_packets(void) {
	int fd;

	/* test IP and UDP with broadcast */
	fd = open_raw_socket(1 /*broadcast*/);
	if (fd < 0) return;

	send_packets_ip(fd);
	send_packets_udp(fd);
	recv_packets_nb(fd);

	CLOSE(fd);

	/* test TCP locally to avoid crashing QEMU */
	fd = open_raw_socket(0 /*broadcast*/);
	if (fd < 0) return;

	send_packets_tcp(fd);
	recv_packets_select(fd);

	CLOSE(fd);
}

int main(int argc, char **argv)
{
	int i;
	pid_t pids[PORT_COUNT];

	start(83);

	/* start servers so we have someone to talk to */
	pids[0] = server_start(SOCK_STREAM, PORT_BASE + 0, sa_close);
	pids[1] = server_start(SOCK_STREAM, PORT_BASE + 1, sa_read);
	pids[2] = server_start(SOCK_STREAM, PORT_BASE + 2, sa_selectrw);
	pids[3] = server_start(SOCK_STREAM, PORT_BASE + 3, sa_write);
	pids[4] = server_start(SOCK_DGRAM,  PORT_BASE + 0, sa_read);
	pids[5] = server_start(SOCK_DGRAM,  PORT_BASE + 1, sa_selectr);

	/* send some bogus packets */
	do_packets();

	/* stop the servers */
	for (i = 0; i < PORT_COUNT; i++) server_stop(pids[i]);
	for (i = 0; i < PORT_COUNT; i++) server_wait(pids[i]);

	quit();
	return 0;
}
