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

_PROTOTYPE( uint32_t htonl, (uint32_t _hostval)				);
_PROTOTYPE( uint16_t htons, (uint16_t _hostval)				);
_PROTOTYPE( char *inet_ntoa, (struct in_addr _in)			);
_PROTOTYPE( uint32_t ntohl, (uint32_t _netval)				);
_PROTOTYPE( uint16_t ntohs, (uint16_t _netval)				);

#endif /* _ARPA__INET_H */
