
#include <lib.h>
#define mount	_mount
#define umount	_umount
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <minix/syslib.h>
#include <minix/rs.h>
#include <minix/paths.h>
#define OK	0

#define MFSNAME "mfs"
#define MFSPATH "/sbin/"

PRIVATE int rs_down(char *label)
{
	char cmd[200];
	message m;
	if(strlen(_PATH_SERVICE)+strlen(label)+50 >= sizeof(cmd))
		return -1;
	sprintf(cmd, _PATH_SERVICE " down %s", label);
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
  if(strlen(dev)+strlen(MFSNAME)+3 >= sizeof(label))
	return NULL;
  sprintf(label, MFSNAME "_%s", dev);
  return label;
}

PUBLIC int mount(special, name, rwflag)
char *name, *special;
int rwflag;
{
  int r;
  message m;
  struct rs_start rs_start;
  char *label;
  char cmd[200];
  FILE *pipe;
  int ep;

  /* Make MFS process label for RS from special name. */
  if(!(label=makelabel(special))) {
	errno = E2BIG;
	return -1;
  }

  if(strlen(_PATH_SERVICE)+strlen(MFSPATH)+strlen(MFSNAME)+
     strlen(label)+50 >= sizeof(cmd)) {
	errno = E2BIG;
	return -1;
  }

  sprintf(cmd, _PATH_SERVICE " up " MFSPATH MFSNAME 
	" -label \"%s\" -config " _PATH_DRIVERS_CONF " -printep yes",
	label);

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
  m.m1_i3 = rwflag;
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
	errno = E2BIG;
	return -1;
  }

  _loadname(name, &m);
  r = _syscall(FS, UMOUNT, &m);

  if(r == OK) {
	rs_down(label);
  }

  return r;
}
