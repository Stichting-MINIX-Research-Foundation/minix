#ifdef OS_linux

#ifdef HAVE_SYS_SYSMACROS_H

#include <sys/sysmacros.h>
#ifndef MAJOR
#define MAJOR(dev) major(dev)
#endif  /* MAJOR not defined */
#ifndef MINOR
#define MINOR(dev) minor(dev)
#endif  /* MINOR not defined */

#else
 
#include <linux/fs.h>        /* get MAJOR/MINOR from Linux kernel */
#ifndef major
#define major(x) MAJOR(x)
#endif

#endif /* HAVE_SYS_SYSMACROS_H */

#include <linux/fd.h>
#include <linux/fdreg.h>
#include <linux/major.h>


typedef struct floppy_raw_cmd RawRequest_t;

UNUSED(static inline void RR_INIT(struct floppy_raw_cmd *request))
{
	request->data = 0;
	request->length = 0;
	request->cmd_count = 9;
	request->flags = FD_RAW_INTR | FD_RAW_NEED_SEEK | FD_RAW_NEED_DISK
#ifdef FD_RAW_SOFTFAILUE
		| FD_RAW_SOFTFAILURE | FD_RAW_STOP_IF_FAILURE
#endif
		;
	request->cmd[1] = 0;
	request->cmd[6] = 0;
	request->cmd[7] = 0x1b;
	request->cmd[8] = 0xff;
	request->reply_count = 0;
}

UNUSED(static inline void RR_SETRATE(struct floppy_raw_cmd *request, int rate))
{
	request->rate = rate;
}

UNUSED(static inline void RR_SETDRIVE(struct floppy_raw_cmd *request,int drive))
{
	request->cmd[1] = (request->cmd[1] & ~3) | (drive & 3);
}

UNUSED(static inline void RR_SETTRACK(struct floppy_raw_cmd *request,int track))
{
	request->cmd[2] = track;
}

UNUSED(static inline void RR_SETPTRACK(struct floppy_raw_cmd *request,
				       int track))
{
	request->track = track;
}

UNUSED(static inline void RR_SETHEAD(struct floppy_raw_cmd *request, int head))
{
	if(head)
		request->cmd[1] |= 4;
	else
		request->cmd[1] &= ~4;
	request->cmd[3] = head;
}

UNUSED(static inline void RR_SETSECTOR(struct floppy_raw_cmd *request, 
				       int sector))
{
	request->cmd[4] = sector;
	request->cmd[6] = sector-1;
}

UNUSED(static inline void RR_SETSIZECODE(struct floppy_raw_cmd *request, 
					 int sizecode))
{
	request->cmd[5] = sizecode;
	request->cmd[6]++;
	request->length += 128 << sizecode;
}

#if 0
static inline void RR_SETEND(struct floppy_raw_cmd *request, int end)
{
	request->cmd[6] = end;
}
#endif

UNUSED(static inline void RR_SETDIRECTION(struct floppy_raw_cmd *request, 
					  int direction))
{
	if(direction == MT_READ) {
		request->flags |= FD_RAW_READ;
		request->cmd[0] = FD_READ & ~0x80;
	} else {
		request->flags |= FD_RAW_WRITE;
		request->cmd[0] = FD_WRITE & ~0x80;
	}
}


UNUSED(static inline void RR_SETDATA(struct floppy_raw_cmd *request, 
				     caddr_t data))
{
	request->data = data;
}


#if 0
static inline void RR_SETLENGTH(struct floppy_raw_cmd *request, int length)
{
	request->length += length;
}
#endif

UNUSED(static inline void RR_SETCONT(struct floppy_raw_cmd *request))
{
#ifdef FD_RAW_MORE
	request->flags |= FD_RAW_MORE;
#endif
}


UNUSED(static inline int RR_SIZECODE(struct floppy_raw_cmd *request))
{
	return request->cmd[5];
}



UNUSED(static inline int RR_TRACK(struct floppy_raw_cmd *request))
{
	return request->cmd[2];
}


UNUSED(static inline int GET_DRIVE(int fd))
{
	struct stat statbuf;

	if (fstat(fd, &statbuf) < 0 ){
		perror("stat");
		return -1;
	}
	  
	if (!S_ISBLK(statbuf.st_mode) ||
	    MAJOR(statbuf.st_rdev) != FLOPPY_MAJOR)
		return -1;
	
	return MINOR( statbuf.st_rdev );
}



/* void print_message(RawRequest_t *raw_cmd,char *message);*/
int send_one_cmd(int fd, RawRequest_t *raw_cmd, const char *message);
int analyze_one_reply(RawRequest_t *raw_cmd, int *bytes, int do_print);


#endif
