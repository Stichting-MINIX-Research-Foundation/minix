/* <minix/mount.h>
 * definitions for mount(2) 
 */

#ifndef _MOUNT_H
#define _MOUNT_H

/* Service flags. These are not passed to VFS. */
#define MS_REUSE	0x001	/* Tell RS to try reusing binary from memory */
#define MS_EXISTING	0x002	/* Tell mount to use already running server */

#define MNT_LABEL_LEN	16	/* Length of fs label including nul */

/* Legacy definitions. */
#define MNTNAMELEN	16	/* Length of fs type name including nul */
#define MNTFLAGLEN	64	/* Length of flags string including nul */

int mount(char *_spec, char *_name, int _mountflags, int srvflags, char *type,
	char *args);
int umount(const char *_name, int srvflags);

#endif /* _MOUNT_H */
