/*
sys/un.h
*/

/* Open Group Base Specifications Issue 6 */

#ifndef _SA_FAMILY_T
#define _SA_FAMILY_T
/* Should match corresponding typedef in <sys/socket.h> */
typedef uint8_t		sa_family_t;
#endif /* _SA_FAMILY_T */

struct sockaddr_un
{
	sa_family_t	sun_family;
	char		sun_path[127];
};

/* Note: UNIX domain sockets are not implemented! */
