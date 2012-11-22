/* <minix/mount.h>
 * definitions for mount(2) 
 */

#ifndef _MOUNT_H
#define _MOUNT_H

#define MS_RDONLY	0x001	/* Mount device read only */
#define MS_REUSE	0x002	/* Tell RS to try reusing binary from memory */
#define MS_LABEL16	0x004	/* Mount message points to 16-byte label */
#define MS_EXISTING	0x008	/* Tell mount to use already running server */
#define MNTNAMELEN	16	/* Length of fs type name including nul */
#define MNTFLAGLEN	64	/* Length of flags string including nul */

int mount(char *_spec, char *_name, int _mountflags, char *type, char
	*args);
int umount(const char *_name);
int umount2(const char *_name, int flags);

#endif /* _MOUNT_H */
