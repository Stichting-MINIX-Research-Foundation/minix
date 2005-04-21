/* The <utime.h> header is used for the utime() system call. */

#ifndef _UTIME_H
#define _UTIME_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

struct utimbuf {
  time_t actime;		/* access time */
  time_t modtime;		/* modification time */
};

/* Function Prototypes. */
_PROTOTYPE( int utime, (const char *_path, const struct utimbuf *_times)     );

#endif /* _UTIME_H */
