
#include <lib.h>
#define mount	_mount
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <minix/syslib.h>

#define OK	0

PUBLIC int mount(special, name, rwflag)
char *name, *special;
int rwflag;
{
  int r;
  struct stat stat_buf;
  message m;
  
  m.RS_CMD_ADDR = "/sbin/mfs";
  if (stat(m.RS_CMD_ADDR, &stat_buf) == -1) {
	/* /sbin/mfs does not exist, try /bin/mfs as well */
	m.RS_CMD_ADDR = "/bin/mfs";
	if (stat(m.RS_CMD_ADDR, &stat_buf) == -1) {
		/* /bin/mfs does not exist either, give up */
		return -1;
	  }
  } 
  
  if (m.RS_CMD_ADDR) {
	m.RS_CMD_LEN = strlen(m.RS_CMD_ADDR);
	m.RS_DEV_MAJOR = 0;
	m.RS_PERIOD = 0;
	r= _taskcall(RS_PROC_NR, RS_UP, &m);
	if (r != OK) {
		errno= -r;
		return -1;
	} 
	/* copy endpointnumber */
	m.m1_p3 = (char*)(unsigned long)m.RS_ENDPOINT;   
  }
  
  m.m1_i1 = strlen(special) + 1;
  m.m1_i2 = strlen(name) + 1;
  m.m1_i3 = rwflag;
  m.m1_p1 = special;
  m.m1_p2 = name;
  return(_syscall(FS, MOUNT, &m));
}
