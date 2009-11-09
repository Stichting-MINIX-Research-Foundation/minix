
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
	message m;
	if(strlen(_PATH_SERVICE)+strlen(label)+50 >= sizeof(cmd))
		return -1;
	sprintf(cmd, _PATH_SERVICE " down '%s'", label);
	return system(cmd);
}

PRIVATE char *makelabel(_CONST char *special)
{
  static char label[40];
  _CONST char *dev;

  /* Make label name. */
  dev = strrchr(special, '/');
  if(dev) dev++;
  else dev = special;
  if(strchr(dev, '\'') != NULL) {
  	errno = EINVAL;
  	return NULL;
  }
  if(strlen(dev)+4 >= sizeof(label)) {
  	errno = E2BIG;
	return NULL;
  }
  sprintf(label, "fs_%s", dev);
  return label;
}

PUBLIC int mount(special, name, mountflags, type, args)
char *name, *special, *type, *args;
int mountflags;
{
  int r;
  message m;
  struct rs_start rs_start;
  struct stat statbuf;
  char *label;
  char path[60];
  char cmd[200];
  FILE *pipe;
  int ep;
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

  /* Make FS process label for RS from special name. */
  if(!(label=makelabel(special))) {
	return -1;
  }

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
     strlen(_PATH_DRIVERS_CONF)+strlen(args)+50 >= sizeof(cmd)) {
	errno = E2BIG;
	return -1;
  }



  sprintf(cmd, _PATH_SERVICE " %sup %s -label '%s' -config " _PATH_DRIVERS_CONF
	" -args '%s%s' -printep yes",
	  reuse ? "-r ": "", path, label, args[0] ? "-o " : "", args);

  if(!(pipe = popen(cmd, "r"))) {
	fprintf(stderr, "mount: couldn't run %s\n", cmd);
	return -1;
  }
  
  if(fscanf(pipe, "%d", &ep) != 1 || ep <= 0) {
	fprintf(stderr, "mount: couldn't parse endpoint from %s\n", cmd);
	errno = EINVAL;
	pclose(pipe);
	return -1;
  }
  pclose(pipe);
  
  /* Now perform mount(). */
  m.m1_i1 = strlen(special) + 1;
  m.m1_i2 = strlen(name) + 1;
  m.m1_i3 = mountflags;
  m.m1_p1 = special;
  m.m1_p2 = name;
  m.m1_p3 = (char*) ep;
  r = _syscall(FS, MOUNT, &m);

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
  char *label;
  int r;

  /* Make MFS process label for RS from special name. */
  if(!(label=makelabel(name))) {
	return -1;
  }

  _loadname(name, &m);
  r = _syscall(FS, UMOUNT, &m);

  if(r == OK) {
	rs_down(label);
  }

  return r;
}
