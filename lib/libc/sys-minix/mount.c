#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <string.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <minix/syslib.h>
#include <minix/rs.h>
#include <paths.h>
#include <unistd.h>
#define OK	0

#ifdef __weak_alias
__weak_alias(mount, _mount)
__weak_alias(umount, _umount)
__weak_alias(umount2, _umount2)
#endif

#define FSDEFAULT "mfs"

static char fspath[] = "/sbin/:/usr/pkg/bin/"; /* Must include trailing '/' */

static int rs_down(char *label)
{
	char cmd[200];
	if(strlen(_PATH_SERVICE)+strlen(label)+50 >= sizeof(cmd))
		return -1;
	sprintf(cmd, _PATH_SERVICE " down '%s'", label);
	return system(cmd);
}

char *find_rslabel(char *args_line);
int mount(special, name, mountflags, type, args)
char *name, *special, *type, *args;
int mountflags;
{
  int r;
  message m;
  struct stat statbuf;
  char label[16];
  char path[PATH_MAX];
  char cmd[200];
  char *p;
  char *rslabel;
  int reuse = 0;
  int use_existing = 0;

  /* Default values. */
  if (type == NULL) type = __UNCONST(FSDEFAULT);
  if (args == NULL) args = __UNCONST("");
  reuse = 0;
  memset(path, '\0', sizeof(path));

  /* Check mount flags */
  if(mountflags & MS_REUSE) {
	reuse = 1;
	mountflags &= ~MS_REUSE; /* Temporary: turn off to not confuse VFS */
  }
  
  if(mountflags & MS_EXISTING) {
	use_existing = 1;
	mountflags &= ~MS_EXISTING; /* Temporary: turn off to not confuse VFS */
  }

  /* Make a label for the file system process. This label must be unique and
   * may currently not exceed 16 characters including terminating null. For
   * requests with an associated block device, we use the last path component
   * name of the block special file (truncated to 12 characters, which is
   * hopefully enough). For requests with no associated block device, we use
   * the device number and inode of the mount point, in hexadecimal form.
   */
  if (!use_existing) {
	if (special) {
		p = strrchr(special, '/');
		p = p ? p + 1 : special;
		if (strchr(p, '\'')) {
			errno = EINVAL;
			return -1;
		}
		sprintf(label, "fs_%.12s", p);
	} else {
		/* check for a rslabel option in the arguments and try to use 
		 * that. 
		 */
		rslabel = find_rslabel(args);
		if (rslabel != NULL){
			snprintf(label,16,"%s",rslabel);
			free(rslabel);
		} else {
			if (stat(name, &statbuf) < 0) return -1;
			sprintf(label, "fs_%04x%llx", statbuf.st_dev, statbuf.st_ino);
		}
	}
  } else {
		/* label to long? */
		if (strlen(type) < 16) {
			sprintf(label, "%s", type);
		} else {
			errno = ENOMEM;
			return -1;
		}
  }

  /* Tell VFS that we are passing in a 16-byte label. */
  mountflags |= MS_LABEL16;

  /* Sanity check on user input. */
  if(strchr(args, '\'')) {
  	errno = EINVAL;
	return -1;
  }
  /* start the fs-server if not using existing one */
  if (!use_existing) {
	/* See if the given type is even remotely valid. */

	char *testpath;
	testpath = strtok(fspath, ":");

	do {
		if (strlen(testpath) + strlen(type) >= sizeof(path)) {
			errno = E2BIG;
			return(-1);
		}

		strcpy(path, testpath);
		strcat(path, type);

		if (access(path, F_OK) == 0) break;

	} while ((testpath = strtok(NULL, ":")) != NULL);

	if (testpath == NULL) {
		/* We were not able to find type somewhere in "fspath" */
		errno = EINVAL;
		return(-1);
	}

	if (strlen(_PATH_SERVICE) + strlen(path) + strlen(label) +
	    strlen(args) + 50 >= sizeof(cmd)) {
		errno = E2BIG;
		return -1;
	}

	sprintf(cmd, _PATH_SERVICE " %sup %s -label '%s' -args '%s %s %s%s'",
		reuse ? "-r ": "", path, label, special, name,
		args[0] ? "-o " : "", args);

	if ((r = system(cmd)) != 0) {
		fprintf(stderr, "mount: couldn't run %s\n", cmd);
		errno = r;
		return -1;
	}
  }
  
  /* Now perform mount(). */
  m.m1_i1 = special ? strlen(special) + 1 : 0;
  m.m1_i2 = strlen(name) + 1;
  m.m1_i3 = mountflags;
  m.m1_p1 = special;
  m.m1_p2 = name;
  m.m1_p3 = label;
  r = _syscall(VFS_PROC_NR, MOUNT, &m);

  if (r != OK && !use_existing) {
	/* If mount() failed, tell RS to shutdown FS process.
	 * No error check - won't do anything with this error anyway.
	 */
	rs_down(label);
  }

  return r;
}

int umount(name)
const char *name;
{
	return umount2(name, 0);
}

int umount2(name, flags)
const char *name;
int flags;
{
  message m;
  int r;


  _loadname(name, &m);
  r = _syscall(VFS_PROC_NR, UMOUNT, &m);

  /* don't shut down the driver when exist flag is set */	
  if (!(flags & MS_EXISTING)) {
	  if (r == OK) {
		/* VFS returns the label of the unmounted file system in the reply.
		* As of writing, the size of the m3_ca1 field is 16 bytes.
		*/
		rs_down(m.m3_ca1);
	}
  }

  return r;
}

char *find_rslabel(char *args_line)
{
  /**
   * Find and return the rslabel as given as optional
   * agument to the mount command e.g. 
   *  mount -o rslabel=bla 
   * or
   *  mount -o rw,rslabel=bla 
   * or as found in fstab
   **/
  char *buf, *input,*saveptr;
  buf = input = saveptr = NULL;

  if (args_line == NULL) return NULL;

  /* copy the input args_line we are going to modify it*/
  input = strndup(args_line,20);
  if (input == NULL) /* EOM */
	return NULL; /* it is not that bad to not find a label */
	
  /* locate rslabel= in the input */
  buf = strstr(input,"rslabel=");
  if (buf == NULL) {
	free(input);
	return NULL;
  }

  /* tokenise on "," starting from rslabel (e.g null terminate )*/
  buf = strtok_r(buf,",",&saveptr);
  /* tokenise the result again using = and take the second entry */
  buf = strtok_r(buf,"=",&saveptr);
  buf = strtok_r(NULL,"=",&saveptr);
  /* buf is now either NULL if there was no second token or 
   * the value we are searchig for 
   */
  if (buf != NULL)
	buf = strdup(buf); 
  free(input);
  return buf;
}

