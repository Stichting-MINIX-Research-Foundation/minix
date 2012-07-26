/*
 * Copyright (C) 2000, 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/***
 ***	DNS Query Performance Testing Tool  (queryperf.c)
 ***
 ***	Version $Id: queryperf.c,v 1.12 2007-09-05 07:36:04 marka Exp $
 ***
 ***	Stephen Jacob <sj@nominum.com>
 ***/

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <math.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#ifndef HAVE_GETADDRINFO
#include "missing/addrinfo.h"
#endif
#endif

/*
 * Configuration defaults
 */

#define DEF_MAX_QUERIES_OUTSTANDING	20
#define DEF_QUERY_TIMEOUT		5		/* in seconds */
#define DEF_SERVER_TO_QUERY		"127.0.0.1"
#define DEF_SERVER_PORT			"53"
#define DEF_BUFFER_SIZE			32		/* in k */

#define DEF_RTTARRAY_SIZE		50000
#define DEF_RTTARRAY_UNIT		100		/* in usec */

/*
 * Other constants / definitions
 */

#define COMMENT_CHAR			';'
#define CONFIG_CHAR			'#'
#define MAX_PORT			65535
#define MAX_INPUT_LEN			512
#define MAX_DOMAIN_LEN			255
#define MAX_BUFFER_LEN			8192		/* in bytes */
#define HARD_TIMEOUT_EXTRA		5		/* in seconds */
#define RESPONSE_BLOCKING_WAIT_TIME	0.1		/* in seconds */
#define EDNSLEN				11

#define FALSE				0
#define TRUE				1

#define WHITESPACE			" \t\n"

enum directives_enum	{ V_SERVER, V_PORT, V_MAXQUERIES, V_MAXWAIT };
#define DIRECTIVES	{ "server", "port", "maxqueries", "maxwait" }
#define DIR_VALUES	{ V_SERVER, V_PORT, V_MAXQUERIES, V_MAXWAIT }

#define QTYPE_STRINGS { \
	"A", "NS", "MD", "MF", "CNAME", "SOA", "MB", "MG", \
	"MR", "NULL", "WKS", "PTR", "HINFO", "MINFO", "MX", "TXT", \
	"AAAA", "SRV", "NAPTR", "A6", "AXFR", "MAILB", "MAILA", "*", "ANY" \
}

#define QTYPE_CODES { \
	1, 2, 3, 4, 5, 6, 7, 8, \
	9, 10, 11, 12, 13, 14, 15, 16, \
	28, 33, 35, 38, 252, 253, 254, 255, 255 \
}

#define RCODE_STRINGS { \
	"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN", \
	"NOTIMP", "REFUSED", "YXDOMAIN", "YXRRSET", \
	"NXRRSET", "NOTAUTH", "NOTZONE", "rcode11", \
	"rcode12", "rcode13", "rcode14", "rcode15" \
}

/*
 * Data type definitions
 */

#define QUERY_STATUS_MAGIC	0x51535441U	/* QSTA */
#define VALID_QUERY_STATUS(q)	((q) != NULL && \
				 (q)->magic == QUERY_STATUS_MAGIC)

struct query_status {
	unsigned int magic;
	int in_use;
	unsigned short int id;
	struct timeval sent_timestamp;
	char *desc;
};

/*
 * Configuration options (global)
 */

unsigned int max_queries_outstanding;			/* init 0 */
unsigned int query_timeout = DEF_QUERY_TIMEOUT;
int ignore_config_changes = FALSE;
unsigned int socket_bufsize = DEF_BUFFER_SIZE;

int family = AF_UNSPEC;
int use_stdin = TRUE;
char *datafile_name;					/* init NULL */

char *server_to_query;					/* init NULL */
char *server_port;					/* init NULL */
struct addrinfo *server_ai;				/* init NULL */

int run_only_once = FALSE;
int use_timelimit = FALSE;
unsigned int run_timelimit;				/* init 0 */
unsigned int print_interval;				/* init 0 */

unsigned int target_qps;				/* init 0 */

int serverset = FALSE, portset = FALSE;
int queriesset = FALSE, timeoutset = FALSE;
int edns = FALSE, dnssec = FALSE;
int countrcodes = FALSE;
int rcodecounts[16] = {0};

int verbose = FALSE;

/*
 * Other global stuff
 */

int setup_phase = TRUE;

FILE *datafile_ptr;					/* init NULL */
unsigned int runs_through_file;				/* init 0 */

unsigned int num_queries_sent;				/* init 0 */
unsigned int num_queries_sent_interval;
unsigned int num_queries_outstanding;			/* init 0 */
unsigned int num_queries_timed_out;			/* init 0 */
unsigned int num_queries_possiblydelayed;		/* init 0 */
unsigned int num_queries_timed_out_interval;
unsigned int num_queries_possiblydelayed_interval;

struct timeval time_of_program_start;
struct timeval time_of_first_query;
double time_of_first_query_sec;
struct timeval time_of_first_query_interval;
struct timeval time_of_end_of_run;
struct timeval time_of_stop_sending;

struct timeval time_of_queryset_start;
double query_interval;
struct timeval time_of_next_queryset;

double rtt_max = -1;
double rtt_max_interval = -1;
double rtt_min = -1;
double rtt_min_interval = -1;
double rtt_total;
double rtt_total_interval;
int rttarray_size = DEF_RTTARRAY_SIZE;
int rttarray_unit = DEF_RTTARRAY_UNIT;
unsigned int *rttarray = NULL;
unsigned int *rttarray_interval = NULL;
unsigned int rtt_overflows;
unsigned int rtt_overflows_interval;
char *rtt_histogram_file = NULL;

struct query_status *status;				/* init NULL */
unsigned int query_status_allocated;			/* init 0 */

int query_socket = -1;
int socket4 = -1, socket6 = -1;

static char *rcode_strings[] = RCODE_STRINGS;

/*
 * get_uint16:
 *   Get an unsigned short integer from a buffer (in network order)
 */
static unsigned short
get_uint16(unsigned char *buf) {
	unsigned short ret;

	ret = buf[0] * 256 + buf[1];

	return (ret);
}

/*
 * show_startup_info:
 *   Show name/version
 */
void
show_startup_info(void) {
	printf("\n"
"DNS Query Performance Testing Tool\n"
"Version: $Id: queryperf.c,v 1.12 2007-09-05 07:36:04 marka Exp $\n"
"\n");
}

/*
 * show_usage:
 *   Print out usage/syntax information
 */
void
show_usage(void) {
	fprintf(stderr,
"\n"
"Usage: queryperf [-d datafile] [-s server_addr] [-p port] [-q num_queries]\n"
"                 [-b bufsize] [-t timeout] [-n] [-l limit] [-f family] [-1]\n"
"                 [-i interval] [-r arraysize] [-u unit] [-H histfile]\n"
"                 [-T qps] [-e] [-D] [-c] [-v] [-h]\n"
"  -d specifies the input data file (default: stdin)\n"
"  -s sets the server to query (default: %s)\n"
"  -p sets the port on which to query the server (default: %s)\n"
"  -q specifies the maximum number of queries outstanding (default: %d)\n"
"  -t specifies the timeout for query completion in seconds (default: %d)\n"
"  -n causes configuration changes to be ignored\n"
"  -l specifies how a limit for how long to run tests in seconds (no default)\n"
"  -1 run through input only once (default: multiple iff limit given)\n"
"  -b set input/output buffer size in kilobytes (default: %d k)\n"
"  -i specifies interval of intermediate outputs in seconds (default: 0=none)\n"
"  -f specify address family of DNS transport, inet or inet6 (default: any)\n"
"  -r set RTT statistics array size (default: %d)\n"
"  -u set RTT statistics time unit in usec (default: %d)\n"
"  -H specifies RTT histogram data file (default: none)\n"
"  -T specify the target qps (default: 0=unspecified)\n"
"  -e enable EDNS 0\n"
"  -D set the DNSSEC OK bit (implies EDNS)\n"
"  -c print the number of packets with each rcode\n"
"  -v verbose: report the RCODE of each response on stdout\n"
"  -h print this usage\n"
"\n",
	        DEF_SERVER_TO_QUERY, DEF_SERVER_PORT,
	        DEF_MAX_QUERIES_OUTSTANDING, DEF_QUERY_TIMEOUT,
		DEF_BUFFER_SIZE, DEF_RTTARRAY_SIZE, DEF_RTTARRAY_UNIT);
}

/*
 * set_datafile:
 *   Set the datafile to read
 *
 *   Return -1 on failure
 *   Return a non-negative integer otherwise
 */
int
set_datafile(char *new_file) {
	char *dfname_tmp;

	if ((new_file == NULL) || (new_file[0] == '\0')) {
		fprintf(stderr, "Error: null datafile name\n");
		return (-1);
	}

	if ((dfname_tmp = malloc(strlen(new_file) + 1)) == NULL) {
		fprintf(stderr, "Error allocating memory for datafile name: "
		        "%s\n", new_file);
		return (-1);
	}

	free(datafile_name);
	datafile_name = dfname_tmp;

	strcpy(datafile_name, new_file);
	use_stdin = FALSE;

	return (0);
}

/*
 * set_input_stdin:
 *   Set the input to be stdin (instead of a datafile)
 */
void
set_input_stdin(void) {
	use_stdin = TRUE;
	free(datafile_name);
	datafile_name = NULL;
}

/*
 * set_server:
 *   Set the server to be queried
 *
 *   Return -1 on failure
 *   Return a non-negative integer otherwise
 */
int
set_server(char *new_name) {
	static struct hostent *server_he;

	/* If no change in server name, don't do anything... */
	if ((server_to_query != NULL) && (new_name != NULL))
		if (strcmp(new_name, server_to_query) == 0)
			return (0);

	if ((new_name == NULL) || (new_name[0] == '\0')) {
		fprintf(stderr, "Error: null server name\n");
		return (-1);
	}

	free(server_to_query);
	server_to_query = NULL;

	if ((server_to_query = malloc(strlen(new_name) + 1)) == NULL) {
		fprintf(stderr, "Error allocating memory for server name: "
		        "%s\n", new_name);
		return (-1);
	}

	strcpy(server_to_query, new_name);

	return (0);
}

/*
 * set_server_port:
 *   Set the port on which to contact the server
 *
 *   Return -1 if port is invalid
 *   Return a non-negative integer otherwise
 */
int
set_server_port(char *new_port) {
	unsigned int uint_val;

	if ((is_uint(new_port, &uint_val)) != TRUE)
		return (-1);

	if (uint_val && uint_val > MAX_PORT)
		return (-1);
	else {
		if (server_port != NULL && new_port != NULL &&
		    strcmp(server_port, new_port) == 0)
			return (0);

		free(server_port);
		server_port = NULL;

		if ((server_port = malloc(strlen(new_port) + 1)) == NULL) {
			fprintf(stderr,
				"Error allocating memory for server port: "
				"%s\n", new_port);
			return (-1);
		}

		strcpy(server_port, new_port);

		return (0);
	}
}

int
set_server_sa(void) {
	struct addrinfo hints, *res;
	static struct protoent *proto;
	int error;

	if (proto == NULL && (proto = getprotobyname("udp")) == NULL) {
		fprintf(stderr, "Error: getprotobyname call failed");
		return (-1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = proto->p_proto;
	if ((error = getaddrinfo(server_to_query, server_port,
				 &hints, &res)) != 0) {
		fprintf(stderr, "Error: getaddrinfo(%s, %s) failed\n",
			server_to_query, server_port);
		return (-1);
	}

	/* replace the server's addrinfo */
	if (server_ai != NULL)
		freeaddrinfo(server_ai);
	server_ai = res;
	return (0);
}

/*
 * is_digit:
 *   Tests if a character is a digit
 *
 *   Return TRUE if it is
 *   Return FALSE if it is not
 */
int
is_digit(char d) {
	if (d < '0' || d > '9')
		return (FALSE);
	else
		return (TRUE);
}

/*
 * is_uint:
 *   Tests if a string, test_int, is a valid unsigned integer
 *
 *   Sets *result to be the unsigned integer if it is valid
 *
 *   Return TRUE if it is
 *   Return FALSE if it is not
 */
int
is_uint(char *test_int, unsigned int *result) {
	unsigned long int value;
	char *end;

	if (test_int == NULL)
		return (FALSE);

	if (is_digit(test_int[0]) == FALSE)
		return (FALSE);

	value = strtoul(test_int, &end, 10);

	if ((errno == ERANGE) || (*end != '\0') || (value > UINT_MAX))
		return (FALSE);

	*result = (unsigned int)value;
	return (TRUE);
}

/*
 * set_max_queries:
 *   Set the maximum number of outstanding queries
 *
 *   Returns -1 on failure
 *   Returns a non-negative integer otherwise
 */
int
set_max_queries(unsigned int new_max) {
	static unsigned int size_qs = sizeof(struct query_status);
	struct query_status *temp_stat;
	unsigned int count;

	if (new_max < 0) {
		fprintf(stderr, "Unable to change max outstanding queries: "
		        "must be positive and non-zero: %u\n", new_max);
		return (-1);
	}

	if (new_max > query_status_allocated) {
		temp_stat = realloc(status, new_max * size_qs);

		if (temp_stat == NULL) {
			fprintf(stderr, "Error resizing query_status\n");
			return (-1);
		} else {
			status = temp_stat;

			/*
			 * Be careful to only initialise between above
			 * the previously allocated space. Note that the
			 * allocation may be larger than the current
			 * max_queries_outstanding. We don't want to
			 * "forget" any outstanding queries! We might
			 * still have some above the bounds of the max.
			 */
			count = query_status_allocated;
			for (; count < new_max; count++) {
				status[count].in_use = FALSE;
				status[count].magic = QUERY_STATUS_MAGIC;
				status[count].desc = NULL;
			}

			query_status_allocated = new_max;
		}
	}

	max_queries_outstanding = new_max;

	return (0);
}

/*
 * parse_args:
 *   Parse program arguments and set configuration options
 *
 *   Return -1 on failure
 *   Return a non-negative integer otherwise
 */
int
parse_args(int argc, char **argv) {
	int c;
	unsigned int uint_arg_val;

	while ((c = getopt(argc, argv,
			   "f:q:t:i:nd:s:p:1l:b:eDcvr:T::u:H:h")) != -1) {
		switch (c) {
		case 'f':
			if (strcmp(optarg, "inet") == 0)
				family = AF_INET;
#ifdef AF_INET6
			else if (strcmp(optarg, "inet6") == 0)
				family = AF_INET6;
#endif
			else if (strcmp(optarg, "any") == 0)
				family = AF_UNSPEC;
			else {
				fprintf(stderr, "Invalid address family: %s\n",
					optarg);
				return (-1);
			}
			break;
		case 'q':
			if (is_uint(optarg, &uint_arg_val) == TRUE) {
				set_max_queries(uint_arg_val);
				queriesset = TRUE;
			} else {
				fprintf(stderr, "Option requires a positive "
				        "integer value: -%c %s\n",
					c, optarg);
				return (-1);
			}
			break;

		case 't':
			if (is_uint(optarg, &uint_arg_val) == TRUE) {
				query_timeout = uint_arg_val;
				timeoutset = TRUE;
			} else {
				fprintf(stderr, "Option requires a positive "
				        "integer value: -%c %s\n",
				        c, optarg);
				return (-1);
			}
			break;

		case 'n':
			ignore_config_changes = TRUE;
			break;

		case 'd':
			if (set_datafile(optarg) == -1) {
				fprintf(stderr, "Error setting datafile "
					"name: %s\n", optarg);
				return (-1);
			}
			break;

		case 's':
			if (set_server(optarg) == -1) {
				fprintf(stderr, "Error setting server "
					"name: %s\n", optarg);
				return (-1);
			}
			serverset = TRUE;
			break;

		case 'p':
			if (is_uint(optarg, &uint_arg_val) == TRUE &&
			    uint_arg_val < MAX_PORT)
			{
				set_server_port(optarg);
				portset = TRUE;
			} else {
				fprintf(stderr, "Option requires a positive "
				        "integer between 0 and %d: -%c %s\n",
					MAX_PORT - 1, c, optarg);
				return (-1);
			}
			break;

		case '1':
			run_only_once = TRUE;
			break;

		case 'l':
			if (is_uint(optarg, &uint_arg_val) == TRUE) {
				use_timelimit = TRUE;
				run_timelimit = uint_arg_val;
			} else {
				fprintf(stderr, "Option requires a positive "
				        "integer: -%c %s\n",
					c, optarg);
				return (-1);
			}
			break;

		case 'b':
			if (is_uint(optarg, &uint_arg_val) == TRUE) {
				socket_bufsize = uint_arg_val;
			} else {
				fprintf(stderr, "Option requires a positive "
					"integer: -%c %s\n",
					c, optarg);
				return (-1);
			}
			break;
		case 'e':
			edns = TRUE;
			break;
		case 'D':
			dnssec = TRUE;
			edns = TRUE;
			break;
		case 'c':
			countrcodes = TRUE;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'i':
			if (is_uint(optarg, &uint_arg_val) == TRUE)
				print_interval = uint_arg_val;
			else {
				fprintf(stderr, "Invalid interval: %s\n",
					optarg);
				return (-1);
			}
			break;
		case 'r':
			if (is_uint(optarg, &uint_arg_val) == TRUE)
				rttarray_size = uint_arg_val;
			else {
				fprintf(stderr, "Invalid RTT array size: %s\n",
					optarg);
				return (-1);
			}
			break;
		case 'u':
			if (is_uint(optarg, &uint_arg_val) == TRUE)
				rttarray_unit = uint_arg_val;
			else {
				fprintf(stderr, "Invalid RTT unit: %s\n",
					optarg);
				return (-1);
			}
			break;
		case 'H':
			rtt_histogram_file = optarg;
			break;
		case 'T':
			if (is_uint(optarg, &uint_arg_val) == TRUE)
				target_qps = uint_arg_val;
			else {
				fprintf(stderr, "Invalid target qps: %s\n",
					optarg);
				return (-1);
			}
			break;
		case 'h':
			return (-1);
		default:
			fprintf(stderr, "Invalid option: -%c\n", optopt);
			return (-1);
		}
	}

	if (run_only_once == FALSE && use_timelimit == FALSE)
		run_only_once = TRUE;

	return (0);
}

/*
 * open_datafile:
 *   Open the data file ready for reading
 *
 *   Return -1 on failure
 *   Return non-negative integer on success
 */
int
open_datafile(void) {
	if (use_stdin == TRUE) {
		datafile_ptr = stdin;
		return (0);
	} else {
		if ((datafile_ptr = fopen(datafile_name, "r")) == NULL) {
			fprintf(stderr, "Error: unable to open datafile: %s\n",
			        datafile_name);
			return (-1);
		} else
			return (0);
	}
}

/*
 * close_datafile:
 *   Close the data file if any is open
 *
 *   Return -1 on failure
 *   Return non-negative integer on success, including if not needed
 */
int
close_datafile(void) {
	if ((use_stdin == FALSE) && (datafile_ptr != NULL)) {
		if (fclose(datafile_ptr) != 0) {
			fprintf(stderr, "Error: unable to close datafile\n");
			return (-1);
		}
	}

	return (0);
}

/*
 * open_socket:
 *   Open a socket for the queries.  When we have an active socket already,
 *   close it and open a new one.
 *
 *   Return -1 on failure
 *   Return the socket identifier
 */
int
open_socket(void) {
	int sock;
	int ret;
	int bufsize;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = server_ai->ai_family;
	hints.ai_socktype = server_ai->ai_socktype;
	hints.ai_protocol = server_ai->ai_protocol;
	hints.ai_flags = AI_PASSIVE;

	if ((ret = getaddrinfo(NULL, "0", &hints, &res)) != 0) {
		fprintf(stderr,
			"Error: getaddrinfo for bind socket failed: %s\n",
			gai_strerror(ret));
		return (-1);
	}

	if ((sock = socket(res->ai_family, SOCK_DGRAM,
			   res->ai_protocol)) == -1) {
		fprintf(stderr, "Error: socket call failed");
		goto fail;
	}

#if defined(AF_INET6) && defined(IPV6_V6ONLY)
	if (res->ai_family == AF_INET6) {
		int on = 1;

		if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
			       &on, sizeof(on)) == -1) {
			fprintf(stderr,
				"Warning: setsockopt(IPV6_V6ONLY) failed\n");
		}
	}
#endif

	if (bind(sock, res->ai_addr, res->ai_addrlen) == -1)
		fprintf(stderr, "Error: bind call failed");

	freeaddrinfo(res);

	bufsize = 1024 * socket_bufsize;

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
			 (char *) &bufsize, sizeof(bufsize));
	if (ret < 0)
		fprintf(stderr, "Warning:  setsockbuf(SO_RCVBUF) failed\n");

	ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
			 (char *) &bufsize, sizeof(bufsize));
	if (ret < 0)
		fprintf(stderr, "Warning:  setsockbuf(SO_SNDBUF) failed\n");

	return (sock);

 fail:
	if (res)
		freeaddrinfo(res);
	return (-1);
}

/*
 * close_socket:
 *   Close the query socket(s)
 *
 *   Return -1 on failure
 *   Return a non-negative integer otherwise
 */
int
close_socket(void) {
	if (socket4 != -1) {
		if (close(socket4) != 0) {
			fprintf(stderr,
				"Error: unable to close IPv4 socket\n");
			return (-1);
		}
	}

	if (socket6 != -1) {
		if (close(socket6) != 0) {
			fprintf(stderr,
				"Error: unable to close IPv6 socket\n");
			return (-1);
		}
	}

	query_socket = -1;

	return (0);
}

/*
 * change_socket:
 *   Choose an appropriate socket according to the address family of the
 *   current server.  Open a new socket if necessary.
 *
 *   Return -1 on failure
 *   Return the socket identifier
 */
int
change_socket(void) {
	int s, *sockp;

	switch (server_ai->ai_family) {
	case AF_INET:
		sockp = &socket4;
		break;
#ifdef AF_INET6
	case AF_INET6:
		sockp = &socket6;
		break;
#endif
	default:
		fprintf(stderr, "unexpected address family: %d\n",
			server_ai->ai_family);
		exit(1);
	}

	if (*sockp == -1) {
		if ((s = open_socket()) == -1)
			return (-1);
		*sockp = s;
	}

	return (*sockp);
}

/*
 * reset_rttarray:
 *   (re)allocate RTT array and zero-clear the whole buffer.
 *   if array is being used, it is freed.
 *   Returns -1 on failure
 *   Returns a non-negative integer otherwise
 */
int
reset_rttarray(int size) {
	if (rttarray != NULL)
		free(rttarray);
	if (rttarray_interval != NULL)
		free(rttarray_interval);

	rttarray = NULL;
	rttarray_interval = NULL;
	rtt_max = -1;
	rtt_min = -1;

	if (size > 0) {
		rttarray = malloc(size * sizeof(rttarray[0]));
		if (rttarray == NULL) {
			fprintf(stderr,
				"Error: allocating memory for RTT array\n");
			return (-1);
		}
		memset(rttarray, 0, size * sizeof(rttarray[0]));

		rttarray_interval = malloc(size *
					   sizeof(rttarray_interval[0]));
		if (rttarray_interval == NULL) {
			fprintf(stderr,
				"Error: allocating memory for RTT array\n");
			return (-1);
		}

		memset(rttarray_interval, 0,
		       size * sizeof(rttarray_interval[0]));
	}

	return (0);
}

/*
 * set_query_interval:
 *   set the interval of consecutive queries if the target qps are specified.
 *   Returns -1 on failure
 *   Returns a non-negative integer otherwise
 */
int
set_query_interval(unsigned int qps) {
	if (qps == 0)
		return (0);

	query_interval = (1.0 / (double)qps);

	return (0);
}

/*
 * setup:
 *   Set configuration options from command line arguments
 *   Open datafile ready for reading
 *
 *   Return -1 on failure
 *   Return non-negative integer on success
 */
int
setup(int argc, char **argv) {
	set_input_stdin();

	if (set_max_queries(DEF_MAX_QUERIES_OUTSTANDING) == -1) {
		fprintf(stderr, "%s: Unable to set default max outstanding "
		        "queries\n", argv[0]);
		return (-1);
	}

	if (set_server(DEF_SERVER_TO_QUERY) == -1) {
		fprintf(stderr, "%s: Error setting default server name\n",
		        argv[0]);
		return (-1);
	}

	if (set_server_port(DEF_SERVER_PORT) == -1) {
		fprintf(stderr, "%s: Error setting default server port\n",
		        argv[0]);
		return (-1);
	}

	if (parse_args(argc, argv) == -1) {
		show_usage();
		return (-1);
	}

	if (open_datafile() == -1)
		return (-1);

	if (set_server_sa() == -1)
		return (-1);

	if ((query_socket = change_socket()) == -1)
		return (-1);

	if (reset_rttarray(rttarray_size) == -1)
		return (-1);

	if (set_query_interval(target_qps) == -1)
		return (-1);

	return (0);
}

/*
 * set_timenow:
 *   Set a timeval struct to indicate the current time
 */
void
set_timenow(struct timeval *tv) {
	if (gettimeofday(tv, NULL) == -1) {
		fprintf(stderr, "Error in gettimeofday(). Using inaccurate "
		        "time() instead\n");
		tv->tv_sec = time(NULL);
		tv->tv_usec = 0;
	}
}

/*
 * addtv:
 *   add tv1 and tv2, store the result in tv_result.
 */
void
addtv(struct timeval *tv1, struct timeval *tv2, struct timeval *tv_result) {
	tv_result->tv_sec = tv1->tv_sec + tv2->tv_sec;
	tv_result->tv_usec = tv1->tv_usec + tv2->tv_usec;
	if (tv_result->tv_usec > 1000000) {
		tv_result->tv_sec++;
		tv_result->tv_usec -= 1000000;
	}
}

/*
 * difftv:
 *   Find the difference in seconds between two timeval structs.
 *
 *   Return the difference between tv1 and tv2 in seconds in a double.
 */
double
difftv(struct timeval tv1, struct timeval tv2) {
	long diff_sec, diff_usec;
	double diff;

	diff_sec = tv1.tv_sec - tv2.tv_sec;
	diff_usec = tv1.tv_usec - tv2.tv_usec;

	diff = (double)diff_sec + ((double)diff_usec / 1000000.0);

	return (diff);
}

/*
 * timelimit_reached:
 *   Have we reached the time limit (if any)?
 *
 *   Returns FALSE if there is no time limit or if we have not reached it
 *   Returns TRUE otherwise
 */
int
timelimit_reached(void) {
	struct timeval time_now;

	set_timenow(&time_now);

	if (use_timelimit == FALSE)
		return (FALSE);

	if (setup_phase == TRUE) {
		if (difftv(time_now, time_of_program_start)
		    < (double)(run_timelimit + HARD_TIMEOUT_EXTRA))
			return (FALSE);
		else
			return (TRUE);
	} else {
		if (difftv(time_now, time_of_first_query)
		    < (double)run_timelimit)
			return (FALSE);
		else
			return (TRUE);
	}
}

/*
 * keep_sending:
 *   Should we keep sending queries or stop here?
 *
 *   Return TRUE if we should keep on sending queries
 *   Return FALSE if we should stop
 *
 *   Side effects:
 *   Rewinds the input and clears reached_end_input if we have reached the
 *   end of the input, but we are meant to run through it multiple times
 *   and have not hit the time limit yet (if any is set).
 */
int
keep_sending(int *reached_end_input) {
	static int stop = FALSE;

	if (stop == TRUE)
		return (FALSE);

	if ((*reached_end_input == FALSE) && (timelimit_reached() == FALSE))
		return (TRUE);
	else if ((*reached_end_input == TRUE) && (run_only_once == FALSE)
	         && (timelimit_reached() == FALSE)) {
		rewind(datafile_ptr);
		*reached_end_input = FALSE;
		runs_through_file++;
		return (TRUE);
	} else {
		if (*reached_end_input == TRUE)
			runs_through_file++;
		set_timenow(&time_of_stop_sending);
		stop = TRUE;
		return (FALSE);
	}
}

/*
 * queries_outstanding:
 *   How many queries are outstanding?
 *
 *   Returns the number of outstanding queries
 */
unsigned int
queries_outstanding(void) {
	return (num_queries_outstanding);
}

/*
 * next_input_line:
 *   Get the next non-comment line from the input file
 *
 *   Put text in line, up to max of n chars. Skip comment lines.
 *   Skip empty lines.
 *
 *   Return line length on success
 *   Return 0 if cannot read a non-comment line (EOF or error)
 */
int
next_input_line(char *line, int n) {
	char *result;

	do {
		result = fgets(line, n, datafile_ptr);
	} while ((result != NULL) &&
	         ((line[0] == COMMENT_CHAR) || (line[0] == '\n')));

	if (result == NULL)
		return (0);
	else
		return (strlen(line));
}

/*
 * identify_directive:
 *   Gives us a numerical value equivelant for a directive string
 *
 *   Returns the value for the directive
 *   Returns -1 if not a valid directive
 */
int
identify_directive(char *dir) {
	static char *directives[] = DIRECTIVES;
	static int dir_values[] = DIR_VALUES;
	unsigned int index, num_directives;

	num_directives = sizeof(directives) / sizeof(directives[0]);

	if (num_directives > (sizeof(dir_values) / sizeof(int)))
		num_directives = sizeof(dir_values) / sizeof(int);

	for (index = 0; index < num_directives; index++) {
		if (strcmp(dir, directives[index]) == 0)
			return (dir_values[index]);
	}

	return (-1);
}

/*
 * update_config:
 *   Update configuration options from a line from the input file
 */
void
update_config(char *config_change_desc) {
	char *directive, *config_value, *trailing_garbage;
	char conf_copy[MAX_INPUT_LEN + 1];
	unsigned int uint_val;
	int directive_number;
	int check;
	int old_af;

	if (ignore_config_changes == TRUE) {
		fprintf(stderr, "Ignoring configuration change: %s",
		        config_change_desc);
		return;
	}

	strcpy(conf_copy, config_change_desc);

	++config_change_desc;

	if (*config_change_desc == '\0') {
		fprintf(stderr, "Invalid config: No directive present: %s\n",
		        conf_copy);
		return;
	}

	if (index(WHITESPACE, *config_change_desc) != NULL) {
		fprintf(stderr, "Invalid config: Space before directive or "
		        "no directive present: %s\n", conf_copy);
		return;
	}

	directive = strtok(config_change_desc, WHITESPACE);
	config_value = strtok(NULL, WHITESPACE);
	trailing_garbage = strtok(NULL, WHITESPACE);

	if ((directive_number = identify_directive(directive)) == -1) {
		fprintf(stderr, "Invalid config: Bad directive: %s\n",
		        conf_copy);
		return;
	}

	if (config_value == NULL) {
		fprintf(stderr, "Invalid config: No value present: %s\n",
		        conf_copy);
		return;
	}

	if (trailing_garbage != NULL) {
		fprintf(stderr, "Config warning: "
		        "trailing garbage: %s\n", conf_copy);
	}

	switch(directive_number) {

	case V_SERVER:
		if (serverset && (setup_phase == TRUE)) {
			fprintf(stderr, "Config change overridden by command "
			        "line: %s\n", directive);
			return;
		}

		if (set_server(config_value) == -1) {
			fprintf(stderr, "Set server error: unable to change "
			        "the server name to '%s'\n", config_value);
			return;
		}

		old_af = server_ai->ai_family;
		if (set_server_sa() == -1) {
			fprintf(stderr, "Set server error: unable to resolve "
				"a new server '%s'\n",
				config_value);
			return;
		}
		if (old_af != server_ai->ai_family) {
			if ((query_socket = change_socket()) == -1) {
				/* XXX: this is fatal */
				fprintf(stderr, "Set server error: "
					"unable to open a new socket "
					"for '%s'\n", config_value);
				exit(1);
			}
		}

		break;

	case V_PORT:
		if (portset && (setup_phase == TRUE)) {
			fprintf(stderr, "Config change overridden by command "
			        "line: %s\n", directive);
			return;
		}

		check = is_uint(config_value, &uint_val);

		if ((check == TRUE) && (uint_val > 0)) {
			if (set_server_port(config_value) == -1) {
				fprintf(stderr, "Invalid config: Bad value for"
				        " %s: %s\n", directive, config_value);
			} else {
				if (set_server_sa() == -1) {
					fprintf(stderr,
						"Failed to set a new port\n");
					return;
				}
			}
		} else
			fprintf(stderr, "Invalid config: Bad value for "
			        "%s: %s\n", directive, config_value);
		break;

	case V_MAXQUERIES:
		if (queriesset && (setup_phase == TRUE)) {
			fprintf(stderr, "Config change overridden by command "
			        "line: %s\n", directive);
			return;
		}

		check = is_uint(config_value, &uint_val);

		if ((check == TRUE) && (uint_val > 0)) {
			set_max_queries(uint_val);
		} else
			fprintf(stderr, "Invalid config: Bad value for "
			        "%s: %s\n", directive, config_value);
		break;

	case V_MAXWAIT:
		if (timeoutset && (setup_phase == TRUE)) {
			fprintf(stderr, "Config change overridden by command "
			        "line: %s\n", directive);
			return;
		}

		check = is_uint(config_value, &uint_val);

		if ((check == TRUE) && (uint_val > 0)) {
			query_timeout = uint_val;
		} else
			fprintf(stderr, "Invalid config: Bad value for "
			        "%s: %s\n", directive, config_value);
		break;

	default:
		fprintf(stderr, "Invalid config: Bad directive: %s\n",
		        directive);
		break;
	}
}

/*
 * parse_query:
 *   Parse a query line from the input file
 *
 *   Set qname to be the domain to query (up to a max of qnlen chars)
 *   Set qtype to be the type of the query
 *
 *   Return -1 on failure
 *   Return a non-negative integer otherwise
 */
int
parse_query(char *input, char *qname, int qnlen, int *qtype) {
	static char *qtype_strings[] = QTYPE_STRINGS;
	static int qtype_codes[] = QTYPE_CODES;
	int num_types, index;
	int found = FALSE;
	char incopy[MAX_INPUT_LEN + 1];
	char *domain_str, *type_str;

	num_types = sizeof(qtype_strings) / sizeof(qtype_strings[0]);
	if (num_types > (sizeof(qtype_codes) / sizeof(int)))
		num_types = sizeof(qtype_codes) / sizeof(int);

	strcpy(incopy, input);

	domain_str = strtok(incopy, WHITESPACE);
	type_str = strtok(NULL, WHITESPACE);

	if ((domain_str == NULL) || (type_str == NULL)) {
		fprintf(stderr, "Invalid query input format: %s\n", input);
		return (-1);
	}

	if (strlen(domain_str) > qnlen) {
		fprintf(stderr, "Query domain too long: %s\n", domain_str);
		return (-1);
	}

	for (index = 0; (index < num_types) && (found == FALSE); index++) {
		if (strcasecmp(type_str, qtype_strings[index]) == 0) {
			*qtype = qtype_codes[index];
			found = TRUE;
		}
	}

	if (found == FALSE) {
		fprintf(stderr, "Query type not understood: %s\n", type_str);
		return (-1);
	}

	strcpy(qname, domain_str);

	return (0);
}

/*
 * dispatch_query:
 *   Send the query packet for the entry 
 *
 *   Return -1 on failure
 *   Return a non-negative integer otherwise
 */
int
dispatch_query(unsigned short int id, char *dom, int qt) {
	static u_char packet_buffer[PACKETSZ + 1];
	static socklen_t sockaddrlen = sizeof(struct sockaddr);
	int buffer_len = PACKETSZ;
	int bytes_sent;
	unsigned short int net_id = htons(id);
	char *id_ptr = (char *)&net_id;

	buffer_len = res_mkquery(QUERY, dom, C_IN, qt, NULL, 0,
				 NULL, packet_buffer, PACKETSZ);
	if (buffer_len == -1) {
		fprintf(stderr, "Failed to create query packet: %s %d\n",
		        dom, qt);
		return (-1);
	}
	if (edns) {
		unsigned char *p;
		if (buffer_len + EDNSLEN >= PACKETSZ) {
			fprintf(stderr, "Failed to add OPT to query packet\n");
			return (-1);
		}
		packet_buffer[11] = 1;
		p = &packet_buffer[buffer_len];
		*p++ = 0;	/* root name */
		*p++ = 0;
		*p++ = 41;	/* OPT */
		*p++ = 16;	
		*p++ = 0;	/* UDP payload size (4K) */
		*p++ = 0;	/* extended rcode */
		*p++ = 0;	/* version */
		if (dnssec)
			*p++ = 0x80;	/* upper flag bits - DO set */
		else
			*p++ = 0;	/* upper flag bits */
		*p++ = 0;	/* lower flag bit */
		*p++ = 0;
		*p++ = 0;	/* rdlen == 0 */
		buffer_len += EDNSLEN;
	}

	packet_buffer[0] = id_ptr[0];
	packet_buffer[1] = id_ptr[1];

	bytes_sent = sendto(query_socket, packet_buffer, buffer_len, 0,
			    server_ai->ai_addr, server_ai->ai_addrlen);
	if (bytes_sent == -1) {
		fprintf(stderr, "Failed to send query packet: %s %d\n",
		        dom, qt);
		return (-1);
	}

	if (bytes_sent != buffer_len)
		fprintf(stderr, "Warning: incomplete packet sent: %s %d\n",
		        dom, qt);

	return (0);
}

/*
 * send_query:
 *   Send a query based on a line of input
 */
void
send_query(char *query_desc) {
	static unsigned short int use_query_id = 0;
	static int qname_len = MAX_DOMAIN_LEN;
	static char domain[MAX_DOMAIN_LEN + 1];
	char serveraddr[NI_MAXHOST];
	int query_type;
	unsigned int count;

	use_query_id++;

	if (parse_query(query_desc, domain, qname_len, &query_type) == -1) {
		fprintf(stderr, "Error parsing query: %s\n", query_desc);
		return;
	}

	if (dispatch_query(use_query_id, domain, query_type) == -1) {
		char *addrstr;

		if (getnameinfo(server_ai->ai_addr, server_ai->ai_addrlen,
				serveraddr, sizeof(serveraddr), NULL, 0,
				NI_NUMERICHOST) == 0) {
			addrstr = serveraddr;
		} else
			addrstr = "???"; /* XXX: this should not happen */
		fprintf(stderr, "Error sending query to %s: %s\n",
			addrstr, query_desc);
		return;
	}

	if (setup_phase == TRUE) {
		set_timenow(&time_of_first_query);
		time_of_first_query_sec = (double)time_of_first_query.tv_sec +
			((double)time_of_first_query.tv_usec / 1000000.0);
		setup_phase = FALSE;
		if (getnameinfo(server_ai->ai_addr, server_ai->ai_addrlen,
				serveraddr, sizeof(serveraddr), NULL, 0,
				NI_NUMERICHOST) != 0) {
			fprintf(stderr, "Error printing server address\n");
			return;
		}
		printf("[Status] Sending queries (beginning with %s)\n",
		       serveraddr);
	}

	/* Find the first slot in status[] that is not in use */
	for (count = 0; (status[count].in_use == TRUE)
	     && (count < max_queries_outstanding); count++);

	if (status[count].in_use == TRUE) {
		fprintf(stderr, "Unexpected error: We have run out of "
			"status[] space!\n");
		return;
	}

	/* Register the query in status[] */
	status[count].in_use = TRUE;
	status[count].id = use_query_id;
	if (verbose)
		status[count].desc = strdup(query_desc);
	set_timenow(&status[count].sent_timestamp);

	if (num_queries_sent_interval == 0)
		set_timenow(&time_of_first_query_interval);

	num_queries_sent++;
	num_queries_sent_interval++;
	num_queries_outstanding++;
}

void
register_rtt(struct timeval *timestamp) {
	int i;
	int oldquery = FALSE;
	struct timeval now;
	double rtt;

	set_timenow(&now);
	rtt = difftv(now, *timestamp);

	if (difftv(*timestamp, time_of_first_query_interval) < 0)
		oldquery = TRUE;

	if (rtt_max < 0 || rtt_max < rtt)
		rtt_max = rtt;

	if (rtt_min < 0 || rtt_min > rtt)
		rtt_min = rtt;

	rtt_total += rtt;

	if (!oldquery) {
		if (rtt_max_interval < 0 || rtt_max_interval < rtt)
			rtt_max_interval = rtt;

		if (rtt_min_interval < 0 || rtt_min_interval > rtt)
			rtt_min_interval = rtt;

		rtt_total_interval += rtt;
	}

	if (rttarray == NULL)
		return;

	i = (int)(rtt * (1000000.0 / rttarray_unit));
	if (i < rttarray_size) {
		rttarray[i]++;
		if (!oldquery)
			rttarray_interval[i]++;
	} else {
		fprintf(stderr, "Warning: RTT is out of range: %.6lf\n",
			rtt);
		rtt_overflows++;
		if (!oldquery)
			rtt_overflows_interval++;
	}
}

/*
 * register_response:
 *   Register receipt of a query
 *
 *   Removes (sets in_use = FALSE) the record for the given query id in
 *   status[] if any exists.
 */
void
register_response(unsigned short int id, unsigned int rcode) {
	unsigned int ct = 0;
	int found = FALSE;
	struct timeval now;
	double rtt;

	for (; (ct < query_status_allocated) && (found == FALSE); ct++) {
		if ((status[ct].in_use == TRUE) && (status[ct].id == id)) {
			status[ct].in_use = FALSE;
			num_queries_outstanding--;
			found = TRUE;

			register_rtt(&status[ct].sent_timestamp);

			if (status[ct].desc) {
				printf("> %s %s\n", rcode_strings[rcode],
				       status[ct].desc);
				free(status[ct].desc);
			}
			if (countrcodes)
				rcodecounts[rcode]++;
		}
	}

	if (found == FALSE) {
		if (target_qps > 0) {
			num_queries_possiblydelayed++;
			num_queries_possiblydelayed_interval++;
		} else {
			fprintf(stderr, "Warning: Received a response with an "
				"unexpected (maybe timed out) id: %u\n", id);
		}
	}
}

/*
 * process_single_response:
 *   Receive from the given socket & process an invididual response packet.
 *   Remove it from the list of open queries (status[]) and decrement the
 *   number of outstanding queries if it matches an open query.
 */
void
process_single_response(int sockfd) {
	struct sockaddr_storage from_addr_ss;
	struct sockaddr *from_addr;
	static unsigned char in_buf[MAX_BUFFER_LEN];
	int numbytes, addr_len, resp_id;
	int flags;

	memset(&from_addr_ss, 0, sizeof(from_addr_ss));
	from_addr = (struct sockaddr *)&from_addr_ss;
	addr_len = sizeof(from_addr_ss);

	if ((numbytes = recvfrom(sockfd, in_buf, MAX_BUFFER_LEN,
	     0, from_addr, &addr_len)) == -1) {
		fprintf(stderr, "Error receiving datagram\n");
		return;
	}

	resp_id = get_uint16(in_buf);
	flags = get_uint16(in_buf + 2);

	register_response(resp_id, flags & 0xF);
}

/*
 * data_available:
 *   Is there data available on the given file descriptor?
 *
 *   Return TRUE if there is
 *   Return FALSE otherwise
 */
int
data_available(double wait) {
	fd_set read_fds;
	struct timeval tv;
	int retval;
	int available = FALSE;
	int maxfd = -1;

	/* Set list of file descriptors */
	FD_ZERO(&read_fds);
	if (socket4 != -1) {
		FD_SET(socket4, &read_fds);
		maxfd = socket4;
	}
	if (socket6 != -1) {
		FD_SET(socket6, &read_fds);
		if (maxfd == -1 || maxfd < socket6)
			maxfd = socket6;
	}

	if ((wait > 0.0) && (wait < (double)LONG_MAX)) {
		tv.tv_sec = (long)floor(wait);
		tv.tv_usec = (long)(1000000.0 * (wait - floor(wait)));
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}

	retval = select(maxfd + 1, &read_fds, NULL, NULL, &tv);

	if (socket4 != -1 && FD_ISSET(socket4, &read_fds)) {
		available = TRUE;
		process_single_response(socket4);
	}
	if (socket6 != -1 && FD_ISSET(socket6, &read_fds)) {
		available = TRUE;
		process_single_response(socket6);
	}

	return (available);
}

/*
 * process_responses:
 *   Go through any/all received responses and remove them from the list of
 *   open queries (set in_use = FALSE for their entry in status[]), also
 *   decrementing the number of outstanding queries.
 */
void
process_responses(int adjust_rate) {
	double wait;
	struct timeval now, waituntil;
	double first_packet_wait = RESPONSE_BLOCKING_WAIT_TIME;
	unsigned int outstanding = queries_outstanding();

	if (adjust_rate == TRUE) {
		double u;

		u = time_of_first_query_sec +
			query_interval * num_queries_sent;
		waituntil.tv_sec = (long)floor(u);
		waituntil.tv_usec = (long)(1000000.0 * (u - waituntil.tv_sec));

		/*
		 * Wait until a response arrives or the specified limit is
		 * reached.
		 */
		while (1) {
			set_timenow(&now);
			wait = difftv(waituntil, now);
			if (wait <= 0)
				wait = 0.0;
			if (data_available(wait) != TRUE)
				break;

			/*
			 * We have reached the limit.  Read as many responses
			 * as possible without waiting, and exit.
			 */
			if (wait == 0) {
				while (data_available(0.0) == TRUE)
					;
				break;
			}
		}
	} else {
		/*
		 * Don't block waiting for packets at all if we aren't
		 * looking for any responses or if we are now able to send new
		 * queries.
		 */
		if ((outstanding == 0) ||
		    (outstanding < max_queries_outstanding)) {
			first_packet_wait = 0.0;
		}

		if (data_available(first_packet_wait) == TRUE) {
			while (data_available(0.0) == TRUE)
				;
		}
	}
}

/*
 * retire_old_queries:
 *   Go through the list of open queries (status[]) and remove any queries
 *   (i.e. set in_use = FALSE) which are older than the timeout, decrementing
 *   the number of queries outstanding for each one removed.
 */
void
retire_old_queries(int sending) {
	unsigned int count = 0;
	struct timeval curr_time;
	double timeout = query_timeout;
	int timeout_reduced = FALSE;

	/*
	 * If we have target qps and would not be able to send any packets
	 * due to buffer full, check whether we are behind the schedule.
	 * If we are, purge some queries more aggressively.
	 */
	if (target_qps > 0 && sending == TRUE && count == 0 &&
	    queries_outstanding() == max_queries_outstanding) {
		struct timeval next, now;
		double n;

		n = time_of_first_query_sec +
			query_interval * num_queries_sent;
		next.tv_sec = (long)floor(n);
		next.tv_usec = (long)(1000000.0 * (n - next.tv_sec));

		set_timenow(&now);
		if (difftv(next, now) <= 0) {
			timeout_reduced = TRUE;
			timeout = 0.001; /* XXX: ad-hoc value */
		}
	}

	set_timenow(&curr_time);

	for (; count < query_status_allocated; count++) {

		if ((status[count].in_use == TRUE)
		    && (difftv(curr_time, status[count].sent_timestamp)
		    >= (double)timeout)) {

			status[count].in_use = FALSE;
			num_queries_outstanding--;
			num_queries_timed_out++;
			num_queries_timed_out_interval++;

			if (timeout_reduced == FALSE) {
				if (status[count].desc) {
					printf("> T %s\n", status[count].desc);
					free(status[count].desc);
				} else {
					printf("[Timeout] Query timed out: "
					       "msg id %u\n",
					       status[count].id);
				}
			}
		}
	}
}

/*
 * print_histogram
 *   Print RTT histogram to the specified file in the gnuplot format
 */
void
print_histogram(unsigned int total) {
	int i;
	double ratio;
	FILE *fp;

	if (rtt_histogram_file == NULL || rttarray == NULL)
		return;

	fp = fopen((const char *)rtt_histogram_file, "w+");
	if (fp == NULL) {
		fprintf(stderr, "Error opening RTT histogram file: %s\n",
			rtt_histogram_file);
		return;
	}

	for (i = 0; i < rttarray_size; i++) {
		ratio = ((double)rttarray[i] / (double)total) * 100;
		fprintf(fp, "%.6lf %.3lf\n",
			(double)(i * rttarray_unit) +
			(double)rttarray_unit / 2,
			ratio);
	}

	(void)fclose(fp);
}

/*
 * print_statistics:
 *   Print out statistics based on the results of the test
 */
void
print_statistics(int intermediate, unsigned int sent, unsigned int timed_out,
		 unsigned int possibly_delayed,
		 struct timeval *first_query,
		 struct timeval *program_start,
		 struct timeval *end_perf, struct timeval *end_query,
		 double rmax, double rmin, double rtotal,
		 unsigned int roverflows, unsigned int *rarray)
{
	unsigned int num_queries_completed;
	double per_lost, per_completed, per_lost2, per_completed2; 
	double run_time, queries_per_sec, queries_per_sec2;
	double queries_per_sec_total;
	double rtt_average, rtt_stddev;
	struct timeval start_time;

	num_queries_completed = sent - timed_out;

	if (num_queries_completed == 0) {
		per_lost = 0.0;
		per_completed = 0.0;

		per_lost2 = 0.0;
		per_completed2 = 0.0;
	} else {
		per_lost = (100.0 * (double)timed_out) / (double)sent;
		per_completed = 100.0 - per_lost;

		per_lost2 = (100.0 * (double)(timed_out - possibly_delayed))
			/ (double)sent;
		per_completed2 = 100 - per_lost2;
	}

	if (sent == 0) {
		start_time.tv_sec = program_start->tv_sec;
		start_time.tv_usec = program_start->tv_usec;
		run_time = 0.0;
		queries_per_sec = 0.0;
		queries_per_sec2 = 0.0;
		queries_per_sec_total = 0.0;
	} else {
		start_time.tv_sec = first_query->tv_sec;
		start_time.tv_usec = first_query->tv_usec;
		run_time = difftv(*end_perf, *first_query);
		queries_per_sec = (double)num_queries_completed / run_time;
		queries_per_sec2 = (double)(num_queries_completed +
					    possibly_delayed) / run_time;

		queries_per_sec_total = (double)sent /
			difftv(*end_query, *first_query);
	}

	if (num_queries_completed > 0) {
		int i;
		double sum = 0;

		rtt_average = rtt_total / (double)num_queries_completed;
		for (i = 0; i < rttarray_size; i++) {
			if (rarray[i] != 0) {
				double mean, diff;

				mean = (double)(i * rttarray_unit) +
				(double)rttarray_unit / 2;
				diff = rtt_average - (mean / 1000000.0);
				sum += (diff * diff) * rarray[i];
			}
		}
		rtt_stddev = sqrt(sum / (double)num_queries_completed);
	} else {
		rtt_average = 0.0;
		rtt_stddev = 0.0;
	}

	printf("\n");

	printf("%sStatistics:\n", intermediate ? "Intermediate " : "");

	printf("\n");

	if (!intermediate) {
		printf("  Parse input file:     %s\n",
		       ((run_only_once == TRUE) ? "once" : "multiple times"));
		if (use_timelimit)
			printf("  Run time limit:       %u seconds\n",
			       run_timelimit);
		if (run_only_once == FALSE)
			printf("  Ran through file:     %u times\n",
			       runs_through_file);
		else
			printf("  Ended due to:         reaching %s\n",
			       ((runs_through_file == 0) ? "time limit"
				: "end of file"));

		printf("\n");
	}

	printf("  Queries sent:         %u queries\n", sent);
	printf("  Queries completed:    %u queries\n", num_queries_completed);
	printf("  Queries lost:         %u queries\n", timed_out);
	printf("  Queries delayed(?):   %u queries\n", possibly_delayed);

	printf("\n");

	printf("  RTT max:         	%3.6lf sec\n", rmax);
	printf("  RTT min:              %3.6lf sec\n", rmin);
	printf("  RTT average:          %3.6lf sec\n", rtt_average);
	printf("  RTT std deviation:    %3.6lf sec\n", rtt_stddev);
	printf("  RTT out of range:     %u queries\n", roverflows);

	if (!intermediate)	/* XXX should we print this case also? */
		print_histogram(num_queries_completed);

	printf("\n");

	if (countrcodes) {
		unsigned int i;

		for (i = 0; i < 16; i++) {
			if (rcodecounts[i] == 0)
				continue;
			printf("  Returned %8s:    %u queries\n",
			       rcode_strings[i], rcodecounts[i]);
		}
		printf("\n");
	}

	printf("  Percentage completed: %6.2lf%%\n", per_completed);
	if (possibly_delayed > 0)
		printf("     (w/ delayed qrys): %6.2lf%%\n", per_completed2);
	printf("  Percentage lost:      %6.2lf%%\n", per_lost);
	if (possibly_delayed > 0)
		printf("    (w/o delayed qrys): %6.2lf%%\n", per_lost2);

	printf("\n");

	printf("  Started at:           %s",
	       ctime((const time_t *)&start_time.tv_sec));
	printf("  Finished at:          %s",
	       ctime((const time_t *)&end_perf->tv_sec));
	printf("  Ran for:              %.6lf seconds\n", run_time);

	printf("\n");

	printf("  Queries per second:   %.6lf qps\n", queries_per_sec);
	if (possibly_delayed > 0) {
		printf("   (w/ delayed qrys):   %.6lf qps\n",
		       queries_per_sec2);
	}
	if (target_qps > 0) {
		printf("  Total QPS/target:     %.6lf/%d qps\n",
		       queries_per_sec_total, target_qps);		
	}

	printf("\n");
}

void
print_interval_statistics() {
	struct timeval time_now;

	if (use_timelimit == FALSE)
		return;

	if (setup_phase == TRUE)
		return;

	if (print_interval == 0)
		return;

	if (timelimit_reached() == TRUE)
		return;

	set_timenow(&time_now);
	if (difftv(time_now, time_of_first_query_interval)
	    <= (double)print_interval)
		return;

	/* Don't count currently outstanding queries */
	num_queries_sent_interval -= queries_outstanding();
	print_statistics(TRUE, num_queries_sent_interval,
			 num_queries_timed_out_interval,
			 num_queries_possiblydelayed_interval,
			 &time_of_first_query_interval,
			 &time_of_first_query_interval, &time_now, &time_now,
			 rtt_max_interval, rtt_min_interval,
			 rtt_total_interval, rtt_overflows_interval,
			 rttarray_interval);

	/* Reset intermediate counters */
	num_queries_sent_interval = 0;
	num_queries_timed_out_interval = 0;
	num_queries_possiblydelayed_interval = 0;
	rtt_max_interval = -1;
	rtt_min_interval = -1;
	rtt_total_interval = 0.0;
	rtt_overflows_interval = 0;
	if (rttarray_interval != NULL) {
		memset(rttarray_interval, 0,
		       sizeof(rttarray_interval[0]) * rttarray_size);
	}
}

/*
 * queryperf Program Mainline
 */
int
main(int argc, char **argv) {
	int adjust_rate;
	int sending = FALSE;
	int got_eof = FALSE;
	int input_length = MAX_INPUT_LEN;
	char input_line[MAX_INPUT_LEN + 1];

	set_timenow(&time_of_program_start);
	time_of_first_query.tv_sec = 0;
	time_of_first_query.tv_usec = 0;
	time_of_first_query_interval.tv_sec = 0;
	time_of_first_query_interval.tv_usec = 0;
	time_of_end_of_run.tv_sec = 0;
	time_of_end_of_run.tv_usec = 0;

	input_line[0] = '\0';

	show_startup_info();

	if (setup(argc, argv) == -1)
		return (-1);

	printf("[Status] Processing input data\n");

	while ((sending = keep_sending(&got_eof)) == TRUE ||
	       queries_outstanding() > 0) {
		print_interval_statistics();
		adjust_rate = FALSE;

		while ((sending = keep_sending(&got_eof)) == TRUE &&
		       queries_outstanding() < max_queries_outstanding) {
			int len = next_input_line(input_line, input_length);
			if (len == 0) {
				got_eof = TRUE;
			} else {
				/* Zap the trailing newline */
				if (input_line[len - 1] == '\n')
					input_line[len - 1] = '\0';

				/*
				 * TODO: Should test if we got a whole line
				 * and flush to the next \n in input if not
				 * here... Add this later. Only do the next
				 * few lines if we got a whole line, else
				 * print a warning. Alternative: Make the
				 * max line size really big. BAD! :)
				 */

				if (input_line[0] == CONFIG_CHAR)
					update_config(input_line);
				else {
					send_query(input_line);
					if (target_qps > 0 &&
					    (num_queries_sent %
					     max_queries_outstanding) == 0) {
						adjust_rate = TRUE;
					}
				}
			}
		}

		process_responses(adjust_rate);
		retire_old_queries(sending);
	}

	set_timenow(&time_of_end_of_run);

	printf("[Status] Testing complete\n");

	close_socket();
	close_datafile();

	print_statistics(FALSE, num_queries_sent, num_queries_timed_out,
			 num_queries_possiblydelayed,
			 &time_of_first_query, &time_of_program_start,
			 &time_of_end_of_run, &time_of_stop_sending,
			 rtt_max, rtt_min, rtt_total, rtt_overflows, rttarray);

	return (0);
}
