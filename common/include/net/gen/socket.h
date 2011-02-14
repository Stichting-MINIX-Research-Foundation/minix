/*
server/ip/gen/socket.h
*/

#ifndef __SERVER__IP__GEN__SOCKET_H__
#define __SERVER__IP__GEN__SOCKET_H__

/* From SunOS: /usr/include/sys/socketh */

/*
 * Address families.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_UNIX		1		/* local to host (pipes, portals) */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_NBS		7		/* nbs protocols */
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */
#define	AF_DECnet	12		/* DECnet */
#define	AF_DLI		13		/* Direct data link interface */
#define	AF_LAT		14		/* LAT */
#define	AF_HYLINK	15		/* NSC Hyperchannel */
#define	AF_APPLETALK	16		/* Apple Talk */

#define	AF_NIT		17		/* Network Interface Tap */
#define	AF_802		18		/* IEEE 802.2, also ISO 8802 */
#define	AF_OSI		19		/* umbrella for all families used
					 * by OSI (e.g. protosw lookup) */
#define	AF_X25		20		/* CCITT X.25 in particular */
#define	AF_OSINET	21		/* AFI = 47, IDI = 4 */
#define	AF_GOSIP	22		/* U.S. Government OSI */
#define	AF_INET6	23		/* IP version 6 */

#define	AF_MAX		23

#endif /* __SERVER__IP__GEN__SOCKET_H__ */
