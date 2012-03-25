/* 
 * decode.c for mdb -- decodes a Minix system call
 */
#include "mdb.h"
#ifdef SYSCALLS_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#define ptrace mdbtrace
#include <sys/ptrace.h>
#include <minix/type.h>
#include <minix/callnr.h>
#include "proto.h"

static void get_message(message *m, unsigned bx);
static void get_data(char *s, unsigned bx, int cnt);

static message sent;
static message recv;
static unsigned saved_addr;
static int last_call;

#define NOSYS		0
#define NOP		1

#define _M1		0x0100
#define _M2		0x0200
#define _M3		0x0400
#define _M4		0x0800

#define _M13		0x0500

#define M1_I1		(_M1|1)
#define M1_I2		(_M1|2)
#define M1_I3		(_M1|4)
#define M1_P1		(_M1|8)
#define M1_P2		(_M1|16)
#define M1_P3		(_M1|32)

#define M2_I1		(_M2|1)
#define M2_I2		(_M2|2)
#define M2_I3		(_M2|4)
#define M2_L1		(_M2|8)
#define M2_L2		(_M2|16)
#define M2_P1		(_M2|32)

#define M3_I1		(_M3|1)
#define M3_I2		(_M3|2)
#define M3_P1		(_M3|4)
#define M3_C1		(_M3|8)

#define M4_L1		(_M4|1)
#define M4_L2		(_M4|2)
#define M4_L3		(_M4|4)
#define M4_L4		(_M4|8)
#define M4_L5		(_M4|16)

#define M13_OPEN	(_M13|1)

#define M1_I12		(M1_I1|M1_I2)
#define M1_NAME1	(M1_I1|M1_P1)
#define M1_NAME2	(M1_I2|M1_P2)
#define M1_2NAMES	(M1_I1|M1_P1|M1_I2|M1_P2)
#define M1_SIGACTION	(M1_I2|M1_P1|M1_P2|M1_P3)

#define M2_IOCTL	(M2_I1|M2_I3|M2_L1|M2_L2)
#define M2_4P		(M2_I1|M2_I2|M2_L1|M2_L2)
#define M2_SIGRETURN	(M2_I2|M2_L1|M2_P1)
#define M2_SIGPROC	(M2_I1|M2_L1)
#define M2_UTIME	(M2_I1|M2_I2|M2_L1|M2_L2|M2_P1)

#define M3_LOAD 	(M3_I1|M3_C1)

struct decode_system {
	int syscall;
	unsigned int sflag;
	unsigned int rflag;
	char *name;
} decode[NCALLS] = {  	
	0,		NOSYS,		NOP,	NULL,
	EXIT,		M1_I1,		NOP,	"EXIT",
	FORK,		NOP,		NOP,	"FORK",
	READ,		M1_I12,		NOP,	"READ",
	WRITE,		M1_I12,		NOP,	"WRITE",
 	OPEN,		M13_OPEN,	NOP,	"OPEN",
	CLOSE,		M1_I1,		NOP,	"CLOSE",
	WAIT,		NOP,		M2_I1,	"WAIT",
	CREAT,		M3_LOAD,	NOP,	"CREAT",
	LINK,		M1_2NAMES,	NOP,	"LINK",
	UNLINK,		M3_LOAD,	NOP,	"UNLINK",
	WAITPID,	M1_I1,		M2_I1,	"WAITPID",
	CHDIR,		M3_LOAD,	NOP,	"CHDIR",
	TIME,		NOP,		M2_L1,	"TIME",
	MKNOD,		M1_NAME1,	NOP,	"MKNOD",
	CHMOD,		M3_LOAD,	NOP,	"CHMOD",
	CHOWN,		M1_NAME1,	NOP,	"CHOWN",
	BRK,		M1_P1,		M2_P1,	"BRK",
	STAT,		M1_NAME1,	NOP,	"STAT",
	LSEEK,		M1_I1,		NOP,	"LSEEK",
	MINIX_GETPID,	NOP,		NOP,	"MINIX_GETPID",
	MOUNT,		M1_2NAMES,	NOP,	"MOUNT",
	UMOUNT,		M3_LOAD,	NOP,	"UMOUNT",
	SETUID,		M1_I1,		NOP,	"SETUID",
	GETUID,		NOP,		NOP,	"GETUID",
	STIME,		M2_L1,		NOP,	"STIME",
	PTRACE,		M2_4P,		NOP,	"PTRACE",
	ALARM,		M1_I1,		NOP,	"ALARM",
	FSTAT,		M1_I1,		NOP,	"FSTAT",
	PAUSE,		NOP,		NOP,	"PAUSE",
	UTIME,		M2_UTIME,	NOP,	"UTIME",
	31,		NOSYS,		NOP,	NULL,
	32,		NOSYS,		NOP,	NULL,
	ACCESS,		M3_LOAD,	NOP,	"ACCESS",
	34,		NOSYS,		NOP,	NULL,
	35,		NOSYS,		NOP,	NULL,
	SYNC,		NOP,		NOP,	"SYNC",
	KILL,		M1_I12,		NOP,	"KILL",
	RENAME,		M1_2NAMES,	NOP,	"RENAME",
	MKDIR,		M1_NAME1,	NOP,	"MKDIR",
	RMDIR,		M3_LOAD,	NOP,	"RMDIR",
	DUP,		NOP,		NOP,	"DUP",
	PIPE,		NOP,		M1_I12,	"PIPE",
	TIMES,		M4_L5,		NOP,	"TIMES",
	44,		NOSYS,		NOP,	NULL,
	45,		NOSYS,		NOP,	NULL,
	SETGID,		M1_I1,		NOP,	"SETGID",
	GETGID,		NOP,		NOP,	"GETGID",
	SIGNAL,		NOP,		NOP,	"SIGNAL",
	49,		NOSYS,		NOP,	NULL,
	50,		NOSYS,		NOP,	NULL,
	51,		NOSYS,		NOP,	NULL,
	52,		NOSYS,		NOP,	NULL,
	53,		NOSYS,		NOP,	NULL,
	IOCTL,		M2_IOCTL,	M2_IOCTL,	"IOCTL",
	FCNTL,		M1_I12,		NOP,	"FCNTL",
#if	ENABLE_SYMLINK
	RDLINK,		M1_NAME1,	NOP,	"RDLINK",
	SLINK,		M1_NAME1,	NOP,	"SLINK",
	LSTAT,		M1_NAME1,	NOP,	"LSTAT",
#else
	56,		NOSYS,		NOP,	NULL,
	57,		NOSYS,		NOP,	NULL,
	58,		NOSYS,		NOP,	NULL,
#endif
	EXEC,		M1_NAME1,	NOP,	"EXEC",
	UMASK,		M1_I1,		NOP,	"UMASK",
	CHROOT,		M3_LOAD,	NOP,	"CHROOT",
	SETSID,		NOP,		NOP,	"SETSID",
	GETPGRP,	NOP,		NOP,	"GETPGRP",
	KSIG,		NOSYS,		NOP,	"KSIG",
	UNPAUSE,	NOSYS,		NOP,	"UNPAUSE",
	66,		NOSYS,		NOP,	NULL,
	REVIVE,		NOSYS,		NOP,	"REVIVE",
	TASK_REPLY,	NOSYS,		NOP,	"TASK_REPLY",
   	69,		NOSYS,		NOP,	NULL,
	70,		NOSYS,		NOP,	NULL,
	SIGACTION,	M1_SIGACTION,	NOP,	"SIGACTION",
	SIGSUSPEND,	M2_L1,		NOP,	"SIGSUSPEND",
	SIGPENDING,	NOP,		M2_L1,	"SIGPENDING",
	SIGPROCMASK,	M2_SIGPROC,	NOP,	"SIGPROCMASK",
	SIGRETURN,	M2_SIGRETURN,	NOP,	"SIGRETURN",
	REBOOT,		M1_I1,		NOP,	"REBOOT"
};

static void get_message(m,bx)
message *m;
unsigned bx;
{
  unsigned addr;
  int i;
  long buffer[ MESS_SIZE/4 + 1 ];

  addr = bx;  
  for (i = 0; i< sizeof(buffer)/4; i++)
	buffer[i] = ptrace(T_GETDATA,curpid,
		(long) (addr+i*4) ,0L);

  memcpy(m,buffer,MESS_SIZE);

}

static void get_data(s, bx, cnt)
char *s;
unsigned bx;
int cnt;
{
  unsigned addr;
  int i,nl;
  long buffer[PATH_MAX/4 + 1];

  addr = bx;
  nl = (cnt / 4) + 1;  
  for (i = 0; i< nl; i++)
	buffer[i] = ptrace(T_GETDATA, curpid, (long) (addr+i*4) ,0L);

  memcpy(s, buffer, cnt);
}


void decode_result()
{

   /* Update message */
   get_message(&recv,saved_addr);
   Printf("result=%d\n", recv.m_type);

   if (last_call < 0 || last_call >NCALLS) {
	Printf("Bad call in decode_result\n");
	return;
   }	 

   switch (decode[last_call].rflag) {
   case NOP:	
		return; 
		break;
   case M1_I12:
		Printf("m1_l1=%d m1_i2=%d ",recv.m1_i1,recv.m1_i2);
		break;
   case M2_IOCTL:
		decode_ioctl('R',&recv);
		break;
   case M2_P1:	
		Printf("m2_p1=%lx ",(unsigned long)recv.m2_p1);
		break;
   case M2_L1:	
		Printf("m2_l1=%lx ",recv.m2_l1);
		break;
   case M2_I1:
		Printf("m2_i1=%x ",recv.m2_i1);
		break;
   default:	
		Printf("rflag=%d ",decode[last_call].rflag);
		break;
   }
   Printf("\n");	
}


void decode_message(bx)
unsigned bx;
{
int t; 
int slen;
unsigned int flag;
char path[PATH_MAX];

   /* Save address of message */
   saved_addr = bx;
   get_message(&sent,bx);

   t = sent.m_type;
   
   if ( t <= 0 || t >= NCALLS ) {
	Printf("Bad call - not in range\n");
	last_call = 0;
	return;
   }

   flag = decode[t].sflag;

   if ( flag == NOSYS) {
	Printf("Bad call - not in system\n");
	last_call = 0;
	return;
   }
   else 
  	last_call = t; 

   Printf(" type %s (%d) ", decode[last_call].name, last_call);

   switch (flag) {
   case NOP:	
		break;
   case M1_I1:	
   case M1_I12:	
		Printf("i1=%d ",sent.m1_i1);
		if ( flag == M1_I1) break;
   case M1_I2:	
		Printf("i2=%d ",sent.m1_i2);
		break;
   case M1_P1:	
		Printf("p1=%lx ",(unsigned long)sent.m1_p1);
		break;
   case M1_NAME1:
   case M1_2NAMES:
		slen = sent.m1_i1;
		get_data(path, (unsigned long) sent.m1_p1, slen);
		path[slen] = '\0';
		Printf("s1=%s ",path);
		if ( flag == M1_NAME1) break;
		slen = sent.m1_i2;
		get_data(path, (unsigned long) sent.m1_p2, slen);
		path[slen] = '\0';
		Printf("s2=%s ",path);
		break;
   case M2_UTIME:
		if ( sent.m2_i1 == 0 )
			slen = sent.m2_i2;
		else
			slen = sent.m2_i1;
		get_data(path, (unsigned long) sent.m2_p1, slen);
		path[slen] = '\0';
		Printf("p1=%s ",path);
		if ( sent.m2_i1 != 0 )
			Printf("l1=%lx l2=%lx ",sent.m2_l1,sent.m2_l2);
		break;
   case M1_SIGACTION:
		Printf("m1_i2=%d p1=%lx p2=%lx p3=%lx\n",
			sent.m1_i2,
			(unsigned long)sent.m1_p1,
			(unsigned long)sent.m1_p2,
			(unsigned long)sent.m1_p3);
		break;
   case M2_4P:	Printf("m2_i1=%d m2_i2=%d m2_l1=%lx m2_l2=%lx ",
			sent.m2_i1,sent.m2_i2,sent.m2_l1,sent.m2_l2);
		break;
   case M2_L1:
		Printf("m2_l1=%ld ",sent.m2_l1);
		break;
   case M2_IOCTL:
		decode_ioctl('S',&sent);
		break;
   case M2_SIGRETURN:
		Printf("m2_i2=%d l1=%lx p1=%lx ",
			sent.m2_i2,sent.m2_l1,
			(unsigned long)sent.m1_p1);
		break;
   case M2_SIGPROC:
		Printf("m2_i1=%d l1=%lx ", sent.m2_i1,sent.m2_l1);
		break;
   case M13_OPEN:
		if (sent.m1_i2 & O_CREAT) {
			slen = sent.m1_i1;
			get_data(path, (unsigned long) sent.m1_p1, slen);
			path[slen] = '\0';
			Printf("s1=%s ",path);
			break;
		} 		
		/* fall to M3_LOAD */
   case M3_LOAD:
		slen = sent.m3_i1;
		if ( slen <= M3_STRING) 
			strncpy(path,sent.m3_ca1,M3_STRING);
		else
			get_data(path, (unsigned long) sent.m3_ca1, slen);
		path[slen] = '\0';
		Printf("m3_name=%s ",path);
		break;
   case M4_L5:
		Printf("m4_l5=%ld ",sent.m4_l5);
		break;
   default:	Printf("sflag=%d ",decode[last_call].sflag);
		break;
   }
   Printf("\n");
}

#endif /* SYSCALLS_SUPPORT */
