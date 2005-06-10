/*
netinet/in.h
*/

#ifndef _NETINET__IN_H
#define _NETINET__IN_H

/* Can we include <stdint.h> here or do we need an additional header that is
 * safe to include?
 */
#include <stdint.h>

/* Open Group Base Specifications Issue 6 (not complete) */
typedef uint32_t in_addr_t;

struct in_addr
{
	in_addr_t	s_addr;
};

#endif /* _NETINET__IN_H */
