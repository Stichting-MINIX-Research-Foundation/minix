#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * getaddrinfo and freeaddrinfo are based on
 * http://www.opengroup.org/onlinepubs/009695399/functions/getaddrinfo.html
 */
void freeaddrinfo(struct addrinfo *ai)
{
	struct addrinfo *next;

	while (ai)
	{
		/* preserve next pointer */
		next = ai->ai_next;

		/* free each member of the struct and the struct itself */
		if (ai->ai_addr) free(ai->ai_addr);
		if (ai->ai_canonname) free(ai->ai_canonname);
		free(ai);

		/* continue to free the next element of the linked list */
		ai = next;
	}
}

static int getaddrinfo_parse_hints(
	const struct addrinfo *hints, 
	int *flags,
	int *socktype,
	int *protocol)
{
	assert(flags);
	assert(socktype);

	/* 
	 * if hints is not specified, no flags are specified and all 
	 * socktypes must be returned 
	 */
	if (!hints)
	{
		*flags = 0;
		*socktype = 0;
		*protocol = 0;
		return 0;
	}

	/* check hints correctness */
	if (hints->ai_addrlen || hints->ai_addr || 
		hints->ai_canonname || hints->ai_next)
	{
		errno = EINVAL;
		return EAI_SYSTEM;
	}

	/* check flags */
	*flags = hints->ai_flags;
	if (*flags & ~(AI_PASSIVE | AI_CANONNAME | 
		AI_NUMERICHOST | AI_NUMERICSERV))
		return EAI_BADFLAGS;

	/* only support IPv4 */
	if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
		return EAI_FAMILY;

	/* only support SOCK_STREAM and SOCK_DGRAM */
	*socktype = hints->ai_socktype;
	switch (*socktype)
	{
		case 0:
		case SOCK_STREAM:
		case SOCK_DGRAM: break;
		default: return EAI_SOCKTYPE;
	}

	/* get protocol */
	*protocol = hints->ai_protocol;

	return 0;
}

static int getaddrinfo_resolve_hostname(
	const char *nodename,
	int flags,
	char ***addr_list,
	const char **canonname)
{
	static struct in_addr addr;
	static char *addr_array[2];
	struct hostent *hostent;

	assert(addr_list);
	assert(canonname);

	/* if no hostname is specified, use local address */
	if (!nodename)
	{
		if ((flags & AI_PASSIVE) == AI_PASSIVE)
			addr.s_addr = htonl(INADDR_ANY);
		else
			addr.s_addr = htonl(INADDR_LOOPBACK);

		addr_array[0] = (char *) &addr;
		addr_array[1] = NULL;
		*addr_list = addr_array;
		*canonname = "localhost";
		return 0;
	}

	if (!*nodename)
		return EAI_NONAME;

	/* convert literal IP address */
	addr.s_addr = inet_addr(nodename);
	if (addr.s_addr != (in_addr_t) -1)
	{
		addr_array[0] = (char *) &addr;
		addr_array[1] = NULL;
		*addr_list = addr_array;
		*canonname = NULL;
		return 0;
	}

	/* AI_NUMERICHOST avoids DNS lookup */
	if ((flags & AI_NUMERICHOST) == AI_NUMERICHOST)
		return EAI_NONAME;

	/* attempt DNS lookup */
	hostent = gethostbyname(nodename);
	if (!hostent)
		switch(h_errno)
		{
			case HOST_NOT_FOUND: return EAI_NONAME;
			case NO_DATA:        return EAI_FAIL;
			case NO_RECOVERY:    return EAI_FAIL;
			case TRY_AGAIN:      return EAI_AGAIN;
			default:             assert(0); return EAI_FAIL;
		}

	/* assumption: only IPv4 addresses returned */
	assert(hostent->h_addrtype == AF_INET);
	assert(hostent->h_length == sizeof(addr));
	*addr_list = hostent->h_addr_list;
	*canonname = hostent->h_name;
	return 0;
}

int getaddrinfo_resolve_servname(
	const char *servname,
	int flags, 
	int socktype, 
	unsigned short *port, 
	int *protocol)
{
	char *endptr;
	long port_long;
	struct protoent	*protoent;
	struct servent *servent;

	assert(port);
	assert(protocol);

	/* if not specified, set port and protocol to zero */
	if (!servname)
	{
		*port = 0;
		*protocol = 0;
		return 0;
	}

	if (!*servname)
		return EAI_SERVICE;

	/* try to parse port number */
	port_long = strtol(servname, &endptr, 0);
	if (!*endptr)
	{
		/* check whether port is within range */
		if (port_long < 0 || port_long > (unsigned short) ~0)
			return EAI_SERVICE;

		*port = htons(port_long);
		*protocol = 0;
		return 0;
	}

	/* AI_NUMERICSERV avoids lookup */
	if ((flags & AI_NUMERICSERV) == AI_NUMERICSERV)
		return EAI_SERVICE;

	/* look up port number */
	servent = getservbyname(servname, 
		(socktype == SOCK_STREAM) ? "tcp" : "udp");
	if (!servent) 
		return EAI_SERVICE;

	*port = servent->s_port;

	/* determine protocol number */
	protoent = getprotobyname(servent->s_proto);
	*protocol = protoent ? protoent->p_proto : 0;
	return 0;
}

int getaddrinfo(
	const char *nodename,
	const char *servname,
	const struct addrinfo *hints,
	struct addrinfo **res)
{
	struct addrinfo *addrinfo, **addrinfo_p;
	char **addr_list;
	const char *canonname;
	int flags, i, protocol, protocol_spec, r, result;
	unsigned short port;
	struct sockaddr_in *sockaddr;
	int socktype, socktype_spec;

	/* 
	 * The following flags are supported:
	 * - AI_CANONNAME
	 * - AI_PASSIVE
	 * - AI_NUMERICHOST 
	 * - AI_NUMERICSERV 
	 *
	 * The following flags not supported due to lack of IPv6 support:
	 * - AI_ADDRCONFIG 
	 * - AI_ALL 
	 * - AI_V4MAPPED 
	 */

	/* check arguments */
	if ((!nodename && !servname) || !res)
		return EAI_NONAME;

	/* parse hints */
	if ((r = getaddrinfo_parse_hints(hints, &flags, &socktype_spec, &protocol_spec))) 
		return r;

	/* resolve hostname */
	if ((r = getaddrinfo_resolve_hostname(nodename, flags, &addr_list, &canonname)))
		return r;

	/* return a result record for each address */
	addrinfo_p = res;
	*addrinfo_p = NULL;
	result = EAI_NONAME;
	while (*addr_list)
	{
		/* return a result record for each socktype */
		for (i = 0; i < 2; i++)
		{
			/* should current socktype be selected? */
			socktype = (i == 0) ? SOCK_STREAM : SOCK_DGRAM;
			if (socktype_spec != 0 && socktype_spec != socktype)
				continue;

			/* resolve port */
			if ((r = getaddrinfo_resolve_servname(servname, flags, socktype, &port, &protocol)))
			{
				freeaddrinfo(*res);
				return r;
			}

			/* enforce matching protocol */
			if (!protocol)
				protocol = protocol_spec;
			else if (protocol_spec && protocol != protocol_spec)
				continue;

			/* allocate result */
			*addrinfo_p = addrinfo = (struct addrinfo *) calloc(1, sizeof(struct addrinfo));
			if (!addrinfo)
			{
				freeaddrinfo(*res);
				return EAI_MEMORY;
			}

			sockaddr = (struct sockaddr_in *) calloc(1, sizeof(struct sockaddr_in));
			if (!sockaddr)
			{
				freeaddrinfo(*res);
				return EAI_MEMORY;
			}

			/* provide information in result */
			addrinfo->ai_family = AF_INET;
			addrinfo->ai_socktype = socktype;
			addrinfo->ai_protocol = protocol;
			addrinfo->ai_addrlen = sizeof(*sockaddr);
			addrinfo->ai_addr = (struct sockaddr *) sockaddr;
			sockaddr->sin_family = AF_INET;
			sockaddr->sin_port = port;
			memcpy(&sockaddr->sin_addr, *addr_list, sizeof(sockaddr->sin_addr));
			if (((flags & AI_CANONNAME) == AI_CANONNAME) && canonname)
			{
				addrinfo->ai_canonname = strdup(canonname);
				if (!addrinfo->ai_canonname)
				{
					freeaddrinfo(*res);
					return EAI_MEMORY;
				}
			}
			result = 0;

			/* chain next result to the current one */
			addrinfo_p = &addrinfo->ai_next;
		}

		/* move on to next address */
		addr_list++;
	}

	/* found anything meaningful? */
	return result;
}
