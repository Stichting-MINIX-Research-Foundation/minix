
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
  struct stat stat_buf;
  message m;
  
  m.RS_CMD_ADDR = "/sbin/mfs";
  if (stat(m.RS_CMD_ADDR, &stat_buf) == -1) {
	  printf("MOUNT: /sbin/mfs doesn't exist\n");
	  m.RS_CMD_ADDR = 0;
  }
  
  if (!m.RS_CMD_ADDR) {
	  m.RS_CMD_ADDR = "/bin/mfs";
	  if (stat(m.RS_CMD_ADDR, &stat_buf) == -1) {
		  printf("MOUNT: /bin/mfs doesn't exist\n");
		  m.RS_CMD_ADDR = 0;
	  }
  } 
  
  if (m.RS_CMD_ADDR) { 
	  if (!(stat_buf.st_mode & S_IFREG)) {
		  printf("MOUNT: FS binary is not a regular file\n");
	  }
	  m.RS_CMD_LEN = strlen(m.RS_CMD_ADDR);
	  m.RS_DEV_MAJOR = 0;
	  m.RS_PERIOD = 0;
	  if (OK != _taskcall(RS_PROC_NR, RS_UP, &m)) {
		  printf("MOUNT: error sending request to RS\n");
	  } 
	  else {
		  /* copy endpointnumber */
		  m.m1_p3 = (char*)(unsigned long)m.RS_ENDPOINT;   
	  }
  }
  
  m.m1_i1 = strlen(special) + 1;
  m.m1_i2 = strlen(name) + 1;
  m.m1_i3 = rwflag;
  m.m1_p1 = special;
  m.m1_p2 = name;
  return(_syscall(FS, MOUNT, &m));
}
