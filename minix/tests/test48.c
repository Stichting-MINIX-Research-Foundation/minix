#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int max_error = 3;
#include "common.h"

#define err() e(__LINE__)

static void printstr(const char *s)
{
	if (s)
		printf("\"%s\"", s); 
	else 
		printf("NULL");
}

static void test_getaddrinfo_err(
	int n, 
	const char *nodename, 
	const char *servname, 
	int passhints, 
	int flags, 
	int family, 
	int socktype,
	const char *exp_result, 
	const char *result)
{
	printf("error %d: getaddrinfo(", n);
	printstr(nodename);
	printf(", ");
	printstr(servname);
	printf(", ");
	if (passhints) 
		printf("{ 0x%x, %d, %d }", flags, family, socktype); 
	else 
		printf("NULL");

	printf("); result: ");
	printstr(result);
	printf("; expected: ");
	printstr(exp_result);
	printf("\n");
	err();
}

/* yes, this is ugly, but not as ugly as repeating it all every time */
#define TEST_GETADDRINFO_ERR_PARAMS \
	nodename, servname, passhints, flags, family, socktype

static void test_getaddrinfo_err_nr(
	int n, 
	const char *nodename, 
	const char *servname, 
	int passhints, 
	int flags, 
	int family, 
	int socktype,
	int exp_result, 
	int result)
{
	char exp_result_s[23], result_s[23];

	/* convert result to string */
	snprintf(exp_result_s, sizeof(exp_result_s), "%d/0x%x", 
		exp_result, exp_result);
	snprintf(result_s, sizeof(result_s), "%d/0x%x", result, result);
	test_getaddrinfo_err(n, TEST_GETADDRINFO_ERR_PARAMS, 
		exp_result_s, result_s);
}

static void test_getnameinfo_err(
	int n, 
	unsigned long ipaddr,
	unsigned short port,
	socklen_t nodelen,
	socklen_t servicelen,
	int flags, 
	const char *exp_result, 
	const char *result)
{
	printf("error %d: getnameinfo(0x%.8x, %d, %d, %d, 0x%x); result: ", 
		n, ntohl(ipaddr), ntohs(port), nodelen, servicelen, flags);
	printstr(result);
	printf("; expected: ");
	printstr(exp_result);
	printf("\n");
	err();
}

/* yes, this is ugly, but not as ugly as repeating it all every time */
#define TEST_GETNAMEINFO_ERR_PARAMS ipaddr, port, nodelen, servicelen, flags

static void test_getnameinfo_err_nr(
	int n, 
	unsigned long ipaddr,
	unsigned short port,
	socklen_t nodelen,
	socklen_t servicelen,
	int flags, 
	int exp_result, 
	int result)
{
	char exp_result_s[23], result_s[23];

	/* convert result to string */
	snprintf(exp_result_s, sizeof(exp_result_s), "%d/0x%x", 
		exp_result, exp_result);
	snprintf(result_s, sizeof(result_s), "%d/0x%x", result, result);
	test_getnameinfo_err(n, TEST_GETNAMEINFO_ERR_PARAMS, 
		exp_result_s, result_s);
}

static void test_getaddrinfo(
	const char *nodename, 
	int nodename_numerical,
	const char *servname, 
	int servname_numerical,
	int passhints, 
	int flags, 
	int family,
	int socktype,
	int exp_results,
	unsigned long exp_ip,
	int exp_canonname,
	unsigned short exp_port)
{
	struct addrinfo *ai, *ai_cur;
	struct addrinfo hints;
	struct sockaddr_in *sockaddr_in;
	int ai_count_dgram, ai_count_stream, r;

	/* some parameters are only meaningful with hints */
	assert(passhints || !flags);
	assert(passhints || family == AF_UNSPEC);
	assert(passhints || !socktype);

	/* a combination of parameters don't make sense to test */
	if (nodename == NULL && servname == NULL) return;
	if (nodename == NULL && (flags & AI_NUMERICHOST)) return;
	if (servname == NULL && (flags & AI_NUMERICSERV)) return;

	/* initialize hints */
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = flags;
	hints.ai_family = family;
	hints.ai_socktype = socktype;

	/* perform query and test result */
	ai = (struct addrinfo *) 0xDEADBEEF;
	r = getaddrinfo(nodename, servname, passhints ? &hints : NULL, &ai);
	if (r < 0 || r >= 32 || !((1 << r) & exp_results))
		test_getaddrinfo_err_nr(1, TEST_GETADDRINFO_ERR_PARAMS, exp_results, r);

	if (r)
		return;
	
	/* the function succeeded; do the results make sense? */
	ai_cur = ai;
	ai_count_dgram = 0;
	ai_count_stream = 0;
	while (ai_cur)
	{
		/* test result fields */
		if (ai_cur->ai_family != AF_INET) 
			test_getaddrinfo_err_nr(2, TEST_GETADDRINFO_ERR_PARAMS,
				AF_INET, ai_cur->ai_family);

		if (socktype && ai_cur->ai_socktype != socktype) 
			test_getaddrinfo_err_nr(3, TEST_GETADDRINFO_ERR_PARAMS,
				socktype, ai_cur->ai_socktype);

		switch (ai_cur->ai_socktype)
		{
			case SOCK_DGRAM:  ai_count_dgram++;  break;
			case SOCK_STREAM: ai_count_stream++; break;
		}

		/* do address and port match? */
		if (ai_cur->ai_addrlen != sizeof(struct sockaddr_in)) 
			test_getaddrinfo_err_nr(4, TEST_GETADDRINFO_ERR_PARAMS, 
				sizeof(struct sockaddr_in), 
				ai_cur->ai_addrlen);
		else
		{
			sockaddr_in = (struct sockaddr_in *) ai_cur->ai_addr;
			if (sockaddr_in->sin_addr.s_addr != exp_ip) 
				test_getaddrinfo_err_nr(5,
					TEST_GETADDRINFO_ERR_PARAMS, 
					ntohl(exp_ip), 
					ntohl(sockaddr_in->sin_addr.s_addr));

			if (sockaddr_in->sin_port != exp_port) 
				test_getaddrinfo_err_nr(6, 
					TEST_GETADDRINFO_ERR_PARAMS, 
					ntohs(exp_port), 
					ntohs(sockaddr_in->sin_port));
		}

		/* If a hostname is numeric, there can't be a canonical name.
		 * Instead, the returned canonname (if requested) will be
		 * identical to the supplied hostname */
		if (nodename != NULL && nodename_numerical &&
		    (flags & AI_CANONNAME)) {
			if (strncmp(ai_cur->ai_canonname, nodename,
					strlen(nodename)))
			test_getaddrinfo_err(11,
				TEST_GETADDRINFO_ERR_PARAMS,
				nodename, ai_cur->ai_canonname);
		} else {
			/* is canonical supplied? */
			if (exp_canonname && nodename &&
			    (!ai_cur->ai_canonname || !*ai_cur->ai_canonname))
				test_getaddrinfo_err(7,
					TEST_GETADDRINFO_ERR_PARAMS,
					"(anything)", ai_cur->ai_canonname);

			if (!exp_canonname && ai_cur->ai_canonname)
				test_getaddrinfo_err(8,
					TEST_GETADDRINFO_ERR_PARAMS,
					NULL, ai_cur->ai_canonname);
	
		}
		/* move to next result */
		ai_cur = ai_cur->ai_next;
	}
	
	/* If socket type is non-zero, make sure we got what we wanted. Else
	 * any result is okay. */
	if (socktype) {
		if (ai_count_dgram != ((socktype == SOCK_STREAM) ? 0 : 1))
			test_getaddrinfo_err_nr(9, TEST_GETADDRINFO_ERR_PARAMS,
			(socktype == SOCK_STREAM) ? 0 : 1, ai_count_dgram);

		if (ai_count_stream != ((socktype == SOCK_DGRAM) ? 0 : 1))
			test_getaddrinfo_err_nr(10, TEST_GETADDRINFO_ERR_PARAMS,
			(socktype == SOCK_DGRAM) ? 0 : 1, ai_count_stream);
	}

	/* clean up */
	freeaddrinfo(ai);
}

static void memsetl(void *s, unsigned long c, size_t n)
{
	unsigned char *p = (unsigned char *) s;
	size_t i;

	for (i = 0; i < n; i++)
		p[i] = c >> (8 * (i % sizeof(c)));
}

static void test_getnameinfo(
	unsigned long ipaddr, 
	unsigned short port,
	const char *exp_node,
	socklen_t nodelen, 
	const char *exp_service,
	socklen_t servicelen, 
	int flags,
	int exp_results)
{
	struct sockaddr_in sockaddr;
	char node[256], service[256];
	int r;

	/* avoid buffer overflows */
	assert(nodelen <= sizeof(node));
	assert(servicelen <= sizeof(service));

	/* perform query and test result */
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = ipaddr;
	sockaddr.sin_port = port;
	memsetl(node, 0xDEADBEEF, nodelen);
	memsetl(service, 0xDEADBEEF, servicelen);
	r = getnameinfo((struct sockaddr *) &sockaddr, sizeof(sockaddr),
		node, nodelen, service, servicelen, flags);

	if (r < 0 || r >= 32 || !((1 << r) & exp_results))
		test_getnameinfo_err_nr(1, TEST_GETNAMEINFO_ERR_PARAMS, 
			exp_results, r);

	if (r)
		return;

	/* check results */
	if (nodelen && strcmp(exp_node, node) != 0)
		test_getnameinfo_err(2, TEST_GETNAMEINFO_ERR_PARAMS, 
			exp_node, node);

	if (servicelen && strcmp(exp_service, service) != 0)
		test_getnameinfo_err(2, TEST_GETNAMEINFO_ERR_PARAMS, 
			exp_service, service);
}

static struct
{
	const char *nodename;
	unsigned long ipaddr;
	int numeric;
	int canonname;
	int need_network;
	int exp_result;
} hosts[] = {
	{ NULL,             0x7f000001, 1, 1, 0, 0                 },
	{ "0.0.0.0",        0x00000000, 1, 0, 0, 0                 },
	{ "0.0.0.255",      0x000000ff, 1, 0, 0, 0                 },
	{ "0.0.255.0",      0x0000ff00, 1, 0, 0, 0                 },
	{ "0.255.0.0",      0x00ff0000, 1, 0, 0, 0                 },
	{ "255.0.0.0",      0xff000000, 1, 0, 0, 0                 },
	{ "127.0.0.1",      0x7f000001, 1, 0, 0, 0                 },
	{ "localhost",      0x7f000001, 0, 1, 0, 0,                },
	{ "static.minix3.org",     0xC023C00A, 0, 1, 1, 0,                },
	{ "",               0x00000000, 1, 0, 0, (1<<EAI_NONAME)|(1<<EAI_FAIL)|(1<<EAI_NODATA)},
	{ "256.256.256.256",0x00000000, 1, 0, 0, (1<<EAI_NONAME)|(1<<EAI_FAIL)|(1<<EAI_NODATA)},
	{ "minix3.example.com",     0x00000000, 0, 0, 1, (1<<EAI_NONAME)|(1<<EAI_FAIL)|(1<<EAI_NODATA)}};

static struct
{
	const char *servname;
	unsigned short port;
	int numeric;
	int socktype;
	int exp_result;
} services[] = {
	{ NULL,        0, 1, 0,           0                  },
	{ "0",         0, 1, 0,           0                  },
	{ "1",         1, 1, 0,           0                  },
	{ "32767", 32767, 1, 0,           0                  },
	{ "32768", 32768, 1, 0,           0                  },
	{ "65535", 65535, 1, 0,           0                  },
	{ "echo",      7, 0, 0,           0                  },
	{ "ftp",      21, 0, 0, 0                  },
	{ "tftp",     69, 0, 0, 0                  },
	{ "-1",        0, 1, 0,           (1<<EAI_NONAME) | (1<<EAI_SERVICE) },
	{ "",          0, 1, 0,           (1<<EAI_NONAME) | (1<<EAI_SERVICE) },
	{ "65537",     0, 1, 0,           (1 << EAI_SERVICE) },
	{ "XXX",       0, 0, 0,           (1 << EAI_SERVICE) }};

static struct 
{
	int value;
	int exp_result;	
} families[] = { 
	{ AF_UNSPEC,               0                 },
	{ AF_INET,                 0                 },
	{ AF_UNSPEC + AF_INET + 1, (1 << EAI_FAMILY)    }};

static struct 
{
	int value;
	int exp_result;	
} socktypes[] = { 
	{ 0,                            0                   },
	{ SOCK_STREAM,                  0                   },
	{ SOCK_DGRAM,                   0                   },
	{ SOCK_STREAM + SOCK_DGRAM + 1,
		(1 << EAI_SOCKTYPE) | (1 << EAI_FAIL) | (1 << EAI_NONAME) }};

#define LENGTH(a) (sizeof((a)) / sizeof((a)[0]))

static void test_getaddrinfo_all(int use_network)
{
	int flag_PASSIVE, flag_CANONNAME, flag_NUMERICHOST, flag_NUMERICSERV;
	int exp_results, flags, i, j, k, l, passhints;
	unsigned long ipaddr;

	/* loop through various parameter values */
	for (i = 0; i < LENGTH(hosts);     i++)
	for (j = 0; j < LENGTH(services);  j++)
	for (k = 0; k < LENGTH(families);  k++)
	for (l = 0; l < LENGTH(socktypes); l++)
	for (flag_PASSIVE     = 0; flag_PASSIVE < 2;     flag_PASSIVE++)
	for (flag_CANONNAME   = 0; flag_CANONNAME < 2;   flag_CANONNAME++)
	for (flag_NUMERICHOST = 0; flag_NUMERICHOST < 2; flag_NUMERICHOST++)
	for (flag_NUMERICSERV = 0; flag_NUMERICSERV < 2; flag_NUMERICSERV++)
	for (passhints = 0; passhints < 2; passhints++)
	{
		/* skip tests that need but cannot use network */
		if (!use_network && hosts[i].need_network)
			continue;

		/* determine flags */
		flags = (flag_PASSIVE     ? AI_PASSIVE : 0) |
			(flag_CANONNAME   ? AI_CANONNAME : 0) |
			(flag_NUMERICHOST ? AI_NUMERICHOST : 0) |
			(flag_NUMERICSERV ? AI_NUMERICSERV : 0);

		/* some options require hints */
		if (families[k].value != AF_UNSPEC ||
		    socktypes[l].value != 0 || flags)  {
			passhints = 1;
		}

		/* flags may influence IP address */
		ipaddr = hosts[i].ipaddr;
		if (!hosts[i].nodename && flag_PASSIVE)
			ipaddr = INADDR_ANY;

		/* determine expected result */
		exp_results = 
			hosts[i].exp_result |
			services[j].exp_result |
			families[k].exp_result |
			socktypes[l].exp_result;
		if (!hosts[i].nodename && !services[j].servname)
			exp_results |= (1 << EAI_NONAME);

		if (flag_NUMERICHOST && !hosts[i].numeric)
			exp_results |= (1 << EAI_NONAME);

		if (flag_NUMERICSERV && !services[j].numeric)
			exp_results |= (1 << EAI_NONAME);

		/* When we don't pass hints, getaddrinfo will find suitable
		 * settings for us. If we do pass hints, there might be
		 * conflicts.
		 */
		if (passhints) {
			/* Can't have conflicting socket types */
			if (services[j].socktype &&
			    socktypes[l].value &&
			    socktypes[l].value != services[j].socktype) {
				exp_results |= (1 << EAI_SERVICE);
			}
		}

		/* with no reason for failure, we demand success */
		if (!exp_results)
			exp_results |= (1 << 0);

		/* test getaddrinfo function */
		test_getaddrinfo(
			hosts[i].nodename,
			hosts[i].numeric,
			services[j].servname,
			services[j].numeric,
			passhints,
			flags,
			families[k].value,
			socktypes[l].value,
			exp_results,
			htonl(ipaddr),
			flag_CANONNAME && hosts[i].canonname,
			htons(services[j].port));
	}
}

static struct
{
	const char *nodename;
	const char *nodenum;
	unsigned long ipaddr;
	int havename;
} ipaddrs[] = {
	{ "0.0.0.0",    "0.0.0.0",      0x00000000, 0 },
	{ "0.0.0.255",  "0.0.0.255",    0x000000ff, 0 },
	{ "0.0.255.0",  "0.0.255.0",    0x0000ff00, 0 },
	{ "0.255.0.0",  "0.255.0.0",    0x00ff0000, 0 },
	{ "255.0.0.0",  "255.0.0.0",    0xff000000, 0 },
	{ "localhost",  "127.0.0.1",    0x7f000001, 1 },
	/* no reverse DNS unfortunately */
	/* { "minix3.org", "130.37.20.20", 0x82251414, 1 } */};

static struct
{
	const char *servname;
	const char *servnum;
	unsigned short port;
	int socktype;
	struct servent *se_tcp; /* getservbyport() s_name on this port with "tcp" */
	struct servent *se_udp; /* getservbyport() s_name on this port with "udp" */
} ports[] = {
	{ "0",      "0",         0, 0           },
	{ "tcpmux", "1",         1, SOCK_STREAM },
	{ "32767",  "32767", 32767, 0           },
	{ "32768",  "32768", 32768, 0           },
	{ "65535",  "65535", 65535, 0           },
	{ "echo",   "7",         7, 0           },
	{ "ftp",    "21",       21, SOCK_STREAM },
	{ "tftp",   "69",       69, SOCK_DGRAM  }};

static int buflens[] = { 0, 1, 2, 3, 4, 5, 6, 9, 10, 11, 255 };

static void test_getnameinfo_all(void)
{
	int flag_NUMERICHOST, flag_NAMEREQD, flag_NUMERICSERV, flag_DGRAM;
	int exp_results, flags, i, j, k, l, socktypemismatch;
	const char *nodename, *servname;

	/* set ports servent structs */
	for (j = 0; j < LENGTH(ports);   j++) {
		struct servent *se_tcp, *se_udp;

		se_tcp = getservbyport(htons(ports[j].port), "tcp");
		ports[j].se_tcp = se_tcp;

		if(ports[j].se_tcp) {
			ports[j].se_tcp = malloc(sizeof(struct servent));
			memcpy(ports[j].se_tcp, se_tcp, sizeof(*se_tcp));
			assert(ports[j].se_tcp->s_name);
			ports[j].se_tcp->s_name = strdup(ports[j].se_tcp->s_name);
			assert(ports[j].se_tcp->s_name);
		}

		se_udp = getservbyport(htons(ports[j].port), "udp");
		ports[j].se_udp = se_udp;

		if(ports[j].se_udp) {
			ports[j].se_udp = malloc(sizeof(struct servent));
			memcpy(ports[j].se_udp, se_udp, sizeof(*se_udp));
			assert(ports[j].se_udp->s_name);
			ports[j].se_udp->s_name = strdup(ports[j].se_udp->s_name);
			assert(ports[j].se_udp->s_name);
		}
	}

	/* loop through various parameter values */
	for (i = 0; i < LENGTH(ipaddrs); i++)
	for (j = 0; j < LENGTH(ports);   j++)
	for (k = 0; k < LENGTH(buflens); k++)
	for (l = 0; l < LENGTH(buflens); l++)
	for (flag_NUMERICHOST = 0; flag_NUMERICHOST < 2; flag_NUMERICHOST++)
	for (flag_NAMEREQD    = 0; flag_NAMEREQD < 2;    flag_NAMEREQD++)
	for (flag_NUMERICSERV = 0; flag_NUMERICSERV < 2; flag_NUMERICSERV++)
	for (flag_DGRAM       = 0; flag_DGRAM < 2;       flag_DGRAM++)
	{
		/* determine flags */
		flags = (flag_NUMERICHOST ? NI_NUMERICHOST : 0) |
			(flag_NAMEREQD    ? NI_NAMEREQD : 0) |
			(flag_NUMERICSERV ? NI_NUMERICSERV : 0) |
			(flag_DGRAM       ? NI_DGRAM : 0);

		/* determine expected result */
		exp_results = 0;
		
		nodename = flag_NUMERICHOST ? ipaddrs[i].nodenum : ipaddrs[i].nodename;
		if (buflens[k] > 0 && buflens[k] <= strlen(nodename))
			exp_results |= (1 << EAI_OVERFLOW) | (1 << EAI_MEMORY);

		socktypemismatch =
			(flag_DGRAM && ports[j].socktype == SOCK_STREAM) ||
			(!flag_DGRAM && ports[j].socktype == SOCK_DGRAM);

		struct servent *se = flag_DGRAM ? ports[j].se_udp : ports[j].se_tcp;

		servname = (flag_NUMERICSERV) ?
			ports[j].servnum : (se ? se->s_name : ports[j].servname);

		if (buflens[l] > 0 && buflens[l] <= strlen(servname))
			exp_results |= (1 << EAI_OVERFLOW) | (1 << EAI_MEMORY);

		if (flag_NAMEREQD && (!ipaddrs[i].havename || flag_NUMERICHOST) && buflens[k])
			exp_results |= (1 << EAI_NONAME);

		/* with no reason for failure, we demand success */
		if (!exp_results)
			exp_results |= (1 << 0);

		/* perform the test */
		test_getnameinfo(
			htonl(ipaddrs[i].ipaddr), 
			htons(ports[j].port),
			nodename,
			buflens[k], 
			servname,
			buflens[l], 
			flags,
			exp_results);
	}
}

static int can_use_network(void)
{
	int status;

	/* try to ping minix3.org */
	status = system("ping -w 5 www.minix3.org > /dev/null 2>&1");
	if (status == 127)
	{
		printf("cannot execute ping\n");
		err();
	}

	return status == 0;
}

int main(void)
{
	int use_network;

	start(48);

	use_network = can_use_network();
	if (!use_network)
		printf("Warning: no network\n");
	test_getaddrinfo_all(use_network);
	test_getnameinfo_all();

	quit();
	return 0;
}
