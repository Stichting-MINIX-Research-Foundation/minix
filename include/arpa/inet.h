/*
arpa/inet.h
*/

#ifndef _ARPA__INET_H
#define _ARPA__INET_H

/* Open Group Base Specifications Issue 6 (not complete): */

#ifndef _STRUCT_IN_ADDR
#define _STRUCT_IN_ADDR
/* Has to match corresponding declaration in <netinet/in.h> */
struct in_addr
{
	in_addr_t	s_addr;
};
#endif


char *inet_ntoa(struct in_addr in);

#endif /* _ARPA__INET_H */
