#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ERRORS 8

static int errct;

static void quit(void)
{
	if (errct > 0)
	{
		printf("%d errors\n", errct);
		exit(1);
	}
	else
	{
		printf("ok\n");
		exit(0);
	}
}

static void err(void)
{
	if (++errct >= MAX_ERRORS)
	{
		printf("aborted, too many errors\n");
		quit();
	}
}

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
	const char *servname, 
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

	/* initialize hints */
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = flags;
	hints.ai_family = family;
	hints.ai_socktype = socktype;
	hints.ai_family = family;

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

		/* is canonical supplied? */
		if (exp_canonname && 
			(!ai_cur->ai_canonname || !*ai_cur->ai_canonname))
			test_getaddrinfo_err(7, 
				TEST_GETADDRINFO_ERR_PARAMS, 
				"(anything)", ai_cur->ai_canonname);

		if (!exp_canonname && ai_cur->ai_canonname)
			test_getaddrinfo_err(8, 
				TEST_GETADDRINFO_ERR_PARAMS, 
				NULL, ai_cur->ai_canonname);
	
		/* move to next result */
		ai_cur = ai_cur->ai_next;
	}
	
	/* check number of results */
	if (ai_count_dgram != ((socktype == SOCK_STREAM) ? 0 : 1))
		test_getaddrinfo_err_nr(9, TEST_GETADDRINFO_ERR_PARAMS, 
			(socktype == SOCK_STREAM) ? 0 : 1, ai_count_dgram);

	if (ai_count_stream != ((socktype == SOCK_DGRAM) ? 0 : 1))
		test_getaddrinfo_err_nr(10, TEST_GETADDRINFO_ERR_PARAMS, 
			(socktype == SOCK_DGRAM) ? 0 : 1, ai_count_stream);

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

void test_getnameinfo(
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
	{ NULL,              0x7f000001, 1, 1, 0, 0                 },
	{ "0.0.0.0",         0x00000000, 1, 0, 0, 0,                },
	{ "0.0.0.255",       0x000000ff, 1, 0, 0, 0,                },
	{ "0.0.255.0",       0x0000ff00, 1, 0, 0, 0,                },
	{ "0.255.0.0",       0x00ff0000, 1, 0, 0, 0,                },
	{ "255.0.0.0",       0xff000000, 1, 0, 0, 0,                },
	{ "127.0.0.1",       0x7f000001, 1, 0, 0, 0,                },
	{ "localhost",       0x7f000001, 0, 1, 0, 0,                },
	{ "minix3.org",      0x82251414, 0, 1, 1, 0,                },
	{ "",                0x00000000, 1, 0, 0, (1 << EAI_NONAME) },
	{ "256.256.256.256", 0x00000000, 1, 0, 0, (1 << EAI_NONAME) },
	{ "minix3.xxx",      0x00000000, 0, 0, 1, (1 << EAI_NONAME) }};

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
	{ "ftp",      21, 0, SOCK_STREAM, 0                  },
	{ "tftp",     69, 0, SOCK_DGRAM , 0                  },
	{ "-1",        0, 1, 0,           (1 << EAI_SERVICE) },
	{ "",          0, 1, 0,           (1 << EAI_SERVICE) },
	{ "65537",     0, 1, 0,           (1 << EAI_SERVICE) },
	{ "XXX",       0, 0, 0,           (1 << EAI_SERVICE) }};

static struct 
{
	int value;
	int exp_result;	
} families[] = { 
	{ AF_UNSPEC,               0                 },
	{ AF_INET,                 0                 },
	{ AF_UNSPEC + AF_INET + 1, (1 << EAI_FAMILY) }};

static struct 
{
	int value;
	int exp_result;	
} socktypes[] = { 
	{ 0,                            0                   },
	{ SOCK_STREAM,                  0                   },
	{ SOCK_DGRAM,                   0                   },
	{ SOCK_STREAM + SOCK_DGRAM + 1, (1 << EAI_SOCKTYPE) }};

#define LENGTH(a) (sizeof((a)) / sizeof((a)[0]))

static void test_getaddrinfo_all(int use_network)
{
	int flag_PASSIVE, flag_CANONNAME, flag_NUMERICHOST, flag_NUMERICSERV;
	int exp_results, flags, i, j, k, l, needhints, passhints;
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
	{
		/* skip tests that need but cannot use network */
		if (!use_network && hosts[i].need_network)
			continue;

		/* determine flags */
		flags = (flag_PASSIVE     ? AI_PASSIVE : 0) |
			(flag_CANONNAME   ? AI_CANONNAME : 0) |
			(flag_NUMERICHOST ? AI_NUMERICHOST : 0) |
			(flag_NUMERICSERV ? AI_NUMERICSERV : 0);

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
			exp_results |= (1 << EAI_SERVICE);

		if (services[j].socktype && socktypes[l].value != services[j].socktype)
			exp_results |= (1 << EAI_SERVICE);

		/* with no reason for failure, we demand success */
		if (!exp_results)
			exp_results |= (1 << 0);

		/* some options require hints */
		needhints = (families[k].value != AF_UNSPEC || 
			socktypes[l].value != 0 || flags) ? 1 : 0;
		for (passhints = needhints; passhints < 2; passhints++)
		{
			/* test getaddrinfo function */
			test_getaddrinfo(
				hosts[i].nodename, 
				services[j].servname, 
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
		if (!buflens[k] && !buflens[l])
			exp_results |= (1 << EAI_NONAME);
		
		nodename = flag_NUMERICHOST ? ipaddrs[i].nodenum : ipaddrs[i].nodename;
		if (buflens[k] > 0 && buflens[k] <= strlen(nodename))
			exp_results |= (1 << EAI_OVERFLOW);

		socktypemismatch =
			(flag_DGRAM && ports[j].socktype == SOCK_STREAM) ||
			(!flag_DGRAM && ports[j].socktype == SOCK_DGRAM);
		servname = (flag_NUMERICSERV || socktypemismatch) ? 
			ports[j].servnum : ports[j].servname;
		if (buflens[l] > 0 && buflens[l] <= strlen(servname))
			exp_results |= (1 << EAI_OVERFLOW);

		if (flag_NAMEREQD && (!ipaddrs[i].havename | flag_NUMERICHOST) && buflens[k])
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
	pid_t pid;
	int status;

	/* try to ping minix3.org */
	status = system("ping www.minix3.org > /dev/null 2>&1");
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

	printf("Test 48 ");
	fflush(stdout);

	use_network = can_use_network();
	if (!use_network)
		printf("Warning: no network\n");

	test_getaddrinfo_all(use_network);
	test_getnameinfo_all();

	quit();
	return 0;
}
