#ifndef SYS_UN_H
#define SYS_UN_H

#include <stdint.h>

#ifndef _SA_FAMILY_T
#define _SA_FAMILY_T
/* Should match corresponding typedef in <sys/socket.h> */
typedef uint8_t		sa_family_t;
#endif /* _SA_FAMILY_T */

#define UNIX_PATH_MAX 127

struct sockaddr_un
{
	sa_family_t	sun_family;
	char		sun_path[UNIX_PATH_MAX];
};

#include <string.h>

/* Compute the actual length of a struct sockaddr_un pointed
 * to by 'unp'. sun_path must be NULL terminated. Length does
 * not include the NULL byte. This is not a POSIX standard
 * definition, but BSD and Linux have it, so it is here for 
 * compatibility.
 */
#define SUN_LEN(unp) \
((size_t)((sizeof(*(unp)) - sizeof((unp)->sun_path)) + strlen((unp)->sun_path)))

#endif
