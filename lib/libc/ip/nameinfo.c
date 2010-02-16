#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

static size_t first_component_len(const char *s)
{
	const char *first = s;

	/* find the first dot or end of string */
	while (*s && *s != '.')
		s++;

	/* return length */
	return s - first;
}

static int getnameinfo_get_host(const struct sockaddr_in *sockaddr, 
       char *node, socklen_t nodelen, int flags)
{
	struct hostent *hostent;
	const char *ipaddr;

	/* perform look-up */
	if ((flags & NI_NUMERICHOST) != NI_NUMERICHOST)
	{
		hostent = gethostbyaddr(
			(char *) &sockaddr->sin_addr, 
			sizeof(sockaddr->sin_addr), 
			AF_INET);

		if (hostent && hostent->h_name)
		{
			/* return the hostname that was found */
			if (nodelen <= strlen(hostent->h_name))
				return EAI_OVERFLOW;

			strcpy(node, hostent->h_name);
			return 0;
		}
	}

	if ((flags & NI_NAMEREQD) == NI_NAMEREQD)
		return EAI_NONAME;

	/* basic implementation to provide numeric values */
	ipaddr = inet_ntoa(sockaddr->sin_addr);
	if (nodelen <= strlen(ipaddr))
		return EAI_OVERFLOW;

	strcpy(node, ipaddr);
	return 0;
}

static int getnameinfo_get_serv(const struct sockaddr_in *sockaddr, 
       char *service, socklen_t servicelen, int flags)
{
	struct servent *servent;
	unsigned short port;

	/* perform look-up */
	if ((flags & NI_NUMERICSERV) != NI_NUMERICSERV)
	{
		servent = getservbyport(sockaddr->sin_port, 
			((flags & NI_DGRAM) == NI_DGRAM) ? "udp" : "tcp");
		if (servent && servent->s_name)
		{
			/* return the service name that was found */
			if (strlen(servent->s_name) >= servicelen)
				return EAI_OVERFLOW;

			strcpy(service, servent->s_name);
			return 0;
		}
	}

	/* return port number */
	port = ntohs(sockaddr->sin_port);
	if (snprintf(service, servicelen, "%u", port) >= servicelen)
		return EAI_OVERFLOW;

	return 0;
}

/*
 * getnameinfo is based on
 * http://www.opengroup.org/onlinepubs/009695399/functions/getnameinfo.html
 */
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
       char *node, socklen_t nodelen, char *service,
       socklen_t servicelen, int flags)
{
	int r;
	const struct sockaddr_in *sockaddr;

	/* 
	 * The following flags are really supported:
	 * - NI_NUMERICHOST
	 * - NI_NAMEREQD 
	 * - NI_NUMERICSERV 
	 * - NI_DGRAM
	 *
	 * The following flag is not supported:
	 * - NI_NUMERICSCOPE
	 *
	 * The following flags could have been supported but is not implemented:
	 * - NI_NOFQDN
	 */

	/* check for invalid parameters; only support IPv4 */
	if (sa == NULL)
	{
		errno = EINVAL;
		return EAI_SYSTEM;
	}
	
	if (sa->sa_family != AF_INET || salen != sizeof(struct sockaddr_in))
		return EAI_FAMILY;

	if (flags & ~(NI_NUMERICHOST | NI_NAMEREQD | NI_NUMERICSERV | NI_DGRAM))
		return EAI_BADFLAGS;

	if ((!node || !nodelen) && (!service || !servicelen))
		return EAI_NONAME;

	/* look up host */
	sockaddr = (const struct sockaddr_in *) sa;
	if (node && nodelen > 0)
	{
		r = getnameinfo_get_host(sockaddr, node, nodelen, flags);
		if (r)
			return r;
	}

	/* look up service */
	if (service && servicelen > 0)
	{
		r = getnameinfo_get_serv(sockaddr, service, servicelen, flags);
		if (r)
			return r;
	}

	return 0;
}
