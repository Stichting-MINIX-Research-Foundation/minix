/*	$NetBSD: ipc_cmd.c,v 1.1.1.2 2008/05/18 14:31:25 aymeric Exp $ */

#include "config.h"

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <limits.h>
#include <stdio.h>

#include "../common/common.h"

#include "ip.h"

static int ipc_unmarshall_a __P((IPVIWIN *, IP_BUF*, IPFunc));
static int ipc_unmarshall_12 __P((IPVIWIN *, IP_BUF*, IPFunc));
static int ipc_unmarshall __P((IPVIWIN *, IP_BUF*, IPFunc));
static int ipc_unmarshall_ab1 __P((IPVIWIN *, IP_BUF*, IPFunc));
static int ipc_unmarshall_1a __P((IPVIWIN *, IP_BUF*, IPFunc));
static int ipc_unmarshall_1 __P((IPVIWIN *, IP_BUF*, IPFunc));
static int ipc_unmarshall_123 __P((IPVIWIN *, IP_BUF*, IPFunc));

#define OFFSET(t,m) ((size_t)&((t *)0)->m)

IPFUNLIST const ipfuns[] = {
/* SI_ADDSTR */
    {"a",   ipc_unmarshall_a,	OFFSET(IPSIOPS, addstr)},
/* SI_ATTRIBUTE */
    {"12",  ipc_unmarshall_12,	OFFSET(IPSIOPS, attribute)},
/* SI_BELL */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, bell)},
/* SI_BUSY_OFF */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, busy_off)},
/* SI_BUSY_ON */
    {"a",   ipc_unmarshall_a,	OFFSET(IPSIOPS, busy_on)},
/* SI_CLRTOEOL */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, clrtoeol)},
/* SI_DELETELN */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, deleteln)},
/* SI_DISCARD */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, discard)},
/* SI_EDITOPT */
    {"ab1", ipc_unmarshall_ab1,	OFFSET(IPSIOPS, editopt)},
/* SI_INSERTLN */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, insertln)},
/* SI_MOVE */
    {"12",  ipc_unmarshall_12,	OFFSET(IPSIOPS, move)},
/* SI_QUIT */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, quit)},
/* SI_REDRAW */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, redraw)},
/* SI_REFRESH */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, refresh)},
/* SI_RENAME */
    {"a",   ipc_unmarshall_a,	OFFSET(IPSIOPS, rename)},
/* SI_REPLY */
    {"1a",  NULL,		0},
/* SI_REWRITE */
    {"1",   ipc_unmarshall_1,	OFFSET(IPSIOPS, rewrite)},
/* SI_SCROLLBAR */
    {"123", ipc_unmarshall_123,	OFFSET(IPSIOPS, scrollbar)},
/* SI_SELECT */
    {"a",   ipc_unmarshall_a,	OFFSET(IPSIOPS, select)},
/* SI_SPLIT */
    {"",    ipc_unmarshall,	OFFSET(IPSIOPS, split)},
/* SI_WADDSTR */
    {"a",   ipc_unmarshall_a,	OFFSET(IPSIOPS, waddstr)},
/* SI_EVENT_SUP */
};

static int
ipc_unmarshall_a(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return ((IPFunc_a)func)(ipvi, ipb->str1, ipb->len1);
}

static int
ipc_unmarshall_12(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return ((IPFunc_12)func)(ipvi, ipb->val1, ipb->val2);
}

static int
ipc_unmarshall(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return func(ipvi);
}

static int
ipc_unmarshall_ab1(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return ((IPFunc_ab1)func)(ipvi, ipb->str1, ipb->len1, ipb->str2, ipb->len2, ipb->val1);
}

static int
ipc_unmarshall_1a(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return ((IPFunc_1a)func)(ipvi, ipb->val1, ipb->str1, ipb->len1);
}

static int
ipc_unmarshall_1(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return ((IPFunc_1)func)(ipvi, ipb->val1);
}

static int
ipc_unmarshall_123(IPVIWIN *ipvi, IP_BUF *ipb, IPFunc func)
{
    return ((IPFunc_123)func)(ipvi, ipb->val1, ipb->val2, ipb->val3);
}
