#ifndef _SYS_UN_H_
#define _SYS_UN_H_

#include <sys/ansi.h>
#include <sys/featuretest.h>
#include <sys/types.h>

#ifndef sa_family_t
typedef __sa_family_t	sa_family_t;
#define sa_family_t	__sa_family_t
#endif

#define UNIX_PATH_MAX 127

/*
 * Definitions for UNIX IPC domain.
 */
struct	sockaddr_un {
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

#endif /* _SYS_UN_H_ */
