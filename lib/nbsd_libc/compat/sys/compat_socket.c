/* $NetBSD: compat_socket.c,v 1.1 2006/06/26 21:23:56 mrg Exp $ */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_socket.c,v 1.1 2006/06/26 21:23:56 mrg Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <compat/sys/socket.h>

__warn_references(socket,
    "warning: reference to compatibility socket(); include <sys/socket.h> for correct reference")

int
socket(int domain, int type, int protocol)
{
	int res;

	res = __socket30(domain, type, protocol);
	if (errno == EAFNOSUPPORT)
		errno = EPROTONOSUPPORT;
	return res;
}
