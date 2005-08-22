/* The <pwd.h> header defines the items in the password file. */

#ifndef _PWD_H
#define _PWD_H

#ifndef _TYPES_H
#include <sys/types.h>
#endif

struct passwd {
  char *pw_name;		/* login name */
  uid_t pw_uid;			/* uid corresponding to the name */
  gid_t pw_gid;			/* gid corresponding to the name */
  char *pw_dir;			/* user's home directory */
  char *pw_shell;		/* name of the user's shell */

  /* The following members are not defined by POSIX. */
  char *pw_passwd;		/* password information */
  char *pw_gecos;		/* just in case you have a GE 645 around */
};

/* Function Prototypes. */
_PROTOTYPE( struct passwd *getpwnam, (const char *_name)		);
_PROTOTYPE( struct passwd *getpwuid, (_mnx_Uid_t _uid)			);

#ifdef _MINIX
_PROTOTYPE( void endpwent, (void)					);
_PROTOTYPE( struct passwd *getpwent, (void)				);
_PROTOTYPE( int setpwent, (void)					);
_PROTOTYPE( void setpwfile, (const char *_file)				);
#endif

#endif /* _PWD_H */
