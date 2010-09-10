
#include <lib.h>
#define mount	_mount
#define umount	_umount
#include <string.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <minix/syslib.h>
#include <minix/rs.h>
#include <minix/paths.h>
#define OK	0

#define FSPATH "/sbin/"
#define FSDEFAULT "mfs"

PRIVATE int rs_down(char *label)
{
	char cmd[200];
	if(strlen(_PATH_SERVICE)+strlen(label)+50 >= sizeof(cmd))
		return -1;
	sprintf(cmd, _PATH_SERVICE " down '%s'", label);
	return system(cmd);
}

PUBLIC int mount(special, name, mountflags, type, args)
char *name, *special, *type, *args;
int mountflags;
{
  int r;
  message m;
  struct stat statbuf;
  char label[16];
  char path[60];
  char cmd[200];
  char *p;
  int reuse;

  /* Default values. */
  if (type == NULL) type = FSDEFAULT;
  if (args == NULL) args = "";
  reuse = 0;

  /* Check mount flags */
  if(mountflags & MS_REUSE) {
	reuse = 1;
	mountflags &= ~MS_REUSE; /* Temporary: turn off to not confuse VFS */
  }

  /* Make a label for the file system process. This label must be unique and
   * may currently not exceed 16 characters including terminating null. For
   * requests with an associated block device, we use the last path component
   * name of the block special file (truncated to 12 characters, which is
   * hopefully enough). For requests with no associated block device, we use
   * the device number and inode of the mount point, in hexadecimal form.
   */
  if (special) {
	p = strrchr(special, '/');
	p = p ? p + 1 : special;
	if (strchr(p, '\'')) {
		errno = EINVAL;
		return -1;
	}
	sprintf(label, "fs_%.12s", p);
  } else {
	if (stat(name, &statbuf) < 0) return -1;
	sprintf(label, "fs_%04x%lx", statbuf.st_dev, statbuf.st_ino);
  }

  /* Tell VFS that we are passing in a 16-byte label. */
  mountflags |= MS_LABEL16;

  /* See if the given type is even remotely valid. */
  if(strlen(FSPATH)+strlen(type) >= sizeof(path)) {
	errno = E2BIG;
	return -1;
  }
  strcpy(path, FSPATH);
  strcat(path, type);
  
  if(stat(path, &statbuf) != 0) {
  	errno = EINVAL;
  	return -1;
  }

  /* Sanity check on user input. */
  if(strchr(args, '\'')) {
  	errno = EINVAL;
	return -1;
  }

  if(strlen(_PATH_SERVICE)+strlen(path)+strlen(label)+
     strlen(args)+50 >= sizeof(cmd)) {
	errno = E2BIG;
	return -1;
  }

  sprintf(cmd, _PATH_SERVICE " %sup %s -label '%s' -args '%s%s'",
	  reuse ? "-r ": "", path, label, args[0] ? "-o " : "", args);

  if((r = system(cmd)) != 0) {
	fprintf(stderr, "mount: couldn't run %s\n", cmd);
	errno = r;
	return -1;
  }
  
  /* Now perform mount(). */
  m.m1_i1 = special ? strlen(special) + 1 : 0;
  m.m1_i2 = strlen(name) + 1;
  m.m1_i3 = mountflags;
  m.m1_p1 = special;
  m.m1_p2 = name;
  m.m1_p3 = label;
  r = _syscall(VFS_PROC_NR, MOUNT, &m);

  if(r != OK) {
	/* If mount() failed, tell RS to shutdown MFS process.
	 * No error check - won't do anything with this error anyway.
	 */
	rs_down(label);
  }

  return r;
}

PUBLIC int umount(name)
_CONST char *name;
{
  message m;
  int r;

  _loadname(name, &m);
  r = _syscall(VFS_PROC_NR, UMOUNT, &m);

  if(r == OK) {
	/* VFS returns the label of the unmounted file system in the reply.
	 * As of writing, the size of the m3_ca1 field is 16 bytes.
	 */
	rs_down(m.m3_ca1);
  }

  return r;
}
