/* <sys/mount.h>
 * definitions for mount(2) 
 */

#ifndef _MOUNT_H
#define _MOUNT_H

#define MS_RDONLY	0x001	/* Mount device read only */
#define MS_REUSE	0x002	/* Tell RS to try reusing binary from memory */


/* Function Prototypes. */
#ifndef _ANSI_H
#include <ansi.h>
#endif

_PROTOTYPE( int mount, (char *_spec, char *_name, int _mountflags,
                                                char *type, char *args) );
_PROTOTYPE( int umount, (const char *_name)                             );

#endif /* _MOUNT_H */
