/* file.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <Michael@TemWare.Com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <utime.h>
#include <net/hton.h>

#include "ftp.h"
#include "file.h"
#include "net.h"

static char *dir(char *path, int full);
static int asciisize(int fd, off_t *filesize);
static off_t asciisetsize(int fd, off_t filesize);
static int cloneit(char *file, int mode);

#if (__WORD_SIZE == 4)
static char buffer[8192];
#else
static char buffer[2048];
#endif
static char line2[512];

static char *dir(path, full)
char *path;
int full;
{
char cmd[128];
static char name[32];

   tmpnam(name);

   if(full)
	sprintf(cmd, "ls -l %s > %s", path, name);
   else
	sprintf(cmd, "ls -dA %s > %s", path, name);

   system(cmd);

   return(name);
}

int DOascii()
{
int s;

   if(DOcmdcheck())
	return(0);

   s = DOcommand("TYPE", "A");

   type = TYPE_A;

   return(s);
}

int DObinary()
{
int s;

   if(DOcmdcheck())
	return(0);

   s = DOcommand("TYPE", "I");

   type = TYPE_I;

   return(s);
}

int DOblock()
{
int s;

   if(DOcmdcheck())
	return(0);

   s = DOcommand("MODE", "B");

   mode = MODE_B;

   return(s);
}

int DOstream()
{
int s;

   if(DOcmdcheck())
	return(0);

   s = DOcommand("MODE", "S");

   mode = MODE_S;

   return(s);
}

int DOpwd()
{
int s;

   if(DOcmdcheck())
	return(0);

   s = DOcommand("PWD", "");

   if(s == 500 || s == 502)
	s = DOcommand("XPWD", "");

   return(s);
}

int DOcd()
{
char *path;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Path: ", line2, sizeof(line2)) < 0)
		return(-1);
	path = line2;
   }

   if(!strcmp(path, ".."))
   	s = DOcommand("CDUP", "");
   else
   	s = DOcommand("CWD", path);

   if(s == 500 || s == 502) {
	if(!strcmp(path, ".."))
		s = DOcommand("XCUP", "");
	else
		s = DOcommand("XCWD", path);
   }

   return(s);
}

int DOmkdir()
{
char *path;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Directory: ", line2, sizeof(line2)) < 0)
		return(-1);
	path = line2;
   }

   s = DOcommand("MKD", path);

   if(s == 500 || s == 502)
	s = DOcommand("XMKD", path);

   return(s);
}

int DOrmdir()
{
char *path;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Directory: ", line2, sizeof(line2)) < 0)
		return(-1);
	path = line2;
   }

   s = DOcommand("RMD", path);

   if(s == 500 || s == 502)
	s = DOcommand("XRMD", path);

   return(s);
}

int DOdelete()
{
char *file;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   return(DOcommand("DELE", file));
}

int DOmdtm()
{
char *file;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   return(DOcommand("MDTM", file));
}

int DOsize()
{
char *file;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   return(DOcommand("SIZE", file));
}

int DOstat()
{
char *file;

   if(cmdargc < 2) {
	if(!linkopen) {
		printf("You must \"OPEN\" a connection first.\n");
		return(0);
	} else {
		return(DOcommand("STAT", ""));
	}
   }
   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   return(DOcommand("STAT", file));
}

int DOlist()
{
char *path;
char *local;
int fd;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2)
	path = "";

   if(cmdargc < 3)
	local = "";
   else
	local = cmdargv[2];

   if(*local == '\0')
	fd = 1;
   else
	fd = open(local, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", local, strerror(errno));
	return(0);
   }

   s = DOdata("LIST", path, RETR, fd);

   if(fd > 2)
	close(fd);

   return(s);
}

int DOnlst()
{
char *path;
char *local;
int fd;
int s;

   if(DOcmdcheck())
	return(0);

   path = cmdargv[1];

   if(cmdargc < 2)
	path = "";

   if(cmdargc < 3)
	local = "";
   else
	local = cmdargv[2];

   if(*local == '\0')
	fd = 1;
   else
	fd = open(local, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", local, strerror(errno));
	return(0);
   }

   s = DOdata("NLST", path, RETR, fd);

   if(fd > 2)
	close(fd);

   return(s);
}

int DOretr()
{
char *file, *localfile;
int fd;
int s;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Remote File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   if(cmdargc < 3)
	localfile = file;
   else
	localfile = cmdargv[2];

   fd = open(localfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", localfile, strerror(errno));
	return(0);
   }

   s = DOdata("RETR", file, RETR, fd);

   close(fd);

   return(s);
}

int DOrretr()
{
char *file, *localfile;
int fd;
int s;
off_t filesize;
char restart[16];

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Remote File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   if(cmdargc < 3)
	localfile = file;
   else
	localfile = cmdargv[2];

   fd = open(localfile, O_RDWR);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", localfile, strerror(errno));
	return(0);
   }

   if(type == TYPE_A) {
   	if(asciisize(fd, &filesize)) {
   		printf("Could not determine ascii file size of %s\n", localfile);
   		close(fd);
   		return(0);
   	}
   } else
	filesize = lseek(fd, 0, SEEK_END);

   sprintf(restart, "%u", filesize);

   s = DOcommand("REST", restart);

   if(s != 350) {
   	close(fd);
   	return(s);
   }

   s = DOdata("RETR", file, RETR, fd);

   close(fd);

   return(s);
}

char *ttime(time_t t)
{
struct tm *tm;
static char tbuf[16];

   tm = localtime(&t);

   sprintf(tbuf, "%04d%02d%02d%02d%02d.%02d",
	tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
	tm->tm_hour, tm->tm_min, tm->tm_sec);

   return(tbuf);
}

static int cloneit(file, mode)
char *file;
int mode;
{
int opr;
int s;
int ss;
struct stat st;
static unsigned short lcrc;
static unsigned short ccrc;
static unsigned long csize;
static char ft;
static unsigned long maj;
static unsigned long min;
static unsigned long uid;
static unsigned long gid;
static unsigned long fmode;
static unsigned long size;
static unsigned long mtime;
struct utimbuf ut;
unsigned short crc(char *fname);

   if(mode == 1) {
	/* see if file exists locally */
	ss = stat(file, &st);

	opr = printreply;
	printreply = 0;
	s = DOcommand("SITE FDET", file);
	printreply = opr;

	if((s / 100) != 2)
   		return(-1);

   	sscanf(reply, "%*d %c%lu%lu%lu%lu%lu%lu%lu",
   		&ft, &maj, &min, &uid, &gid, &fmode, &size, &mtime);

   	if(ft == 'f') {
		opr = printreply;
		printreply = 0;
		s = DOcommand("SITE CCRC", file);
		printreply = opr;
		if((s / 100) != 2)
   			return(-1);

   		sscanf(reply, "%*hu %*s%u%lu", &ccrc, &csize);
   		if(ss < 0) return(-1);
   		lcrc = crc(file);
   		if(size != csize || size != st.st_size || ccrc != lcrc)
   			return(-1);
   	} else
   	if(ss < 0 && ft == 'd') {
   		s = mkdir(file, fmode);
   		printf("mkdir %s\n", file);
   	} else
	if((ss < 0) && (ft == 'b' || ft == 'c' || ft == 'p')) {
		s = mknod(file, fmode, maj << 8 | min);
		printf("mknod %s %lu %lu\n", file, maj, min);
	} else
		return(0);
   }
   ss = stat(file, &st);
   if(ss < 0)
   	return(-1);
   if(st.st_uid != uid || st.st_gid != gid) {
   	s = chown(file, uid, gid);
   	printf("chown %lu:%lu %s\n", uid, gid, file);
   }
   if(st.st_mode != fmode) {
   	s = chmod(file, fmode);
   	printf("chmod %04lo %s\n", fmode, file);
   }
   if(st.st_mtime != mtime) {
   	ut.actime = mtime;
   	ut.modtime = mtime;
   	s = utime(file, &ut);
   	printf("touch -m -t %s %s\n", ttime(mtime), file);
   }

   return(0);
}

int DOMretr()
{
char *files;
int fd, s;
char *p;
FILE *fp;
char name[32];

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Files: ", line2, sizeof(line2)) < 0)
		return(-1);
	files = line2;
   }

   tmpnam(name);

   fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", name, strerror(errno));
	return(0);
   }

   s = DOdata("NLST", files, RETR, fd);

   close(fd);

   if(s == 226 || s == 250) {
	fp = fopen(name, "r");
	unlink(name);
	if(fp == (FILE *)NULL) {
		printf("Unable to open file listing.\n");
		return(0);
	}
	while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
		p = line2 + strlen(line2) - 1;
		if(p >= line2 && (*p == '\r' || *p == '\n')) *p-- = '\0';
		if(p >= line2 && (*p == '\r' || *p == '\n')) *p-- = '\0';
		printf("Retrieving file: %s\n", line2); fflush(stdout);
		fd = open(line2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if(fd < 0)
			printf("Unable to open local file %s\n", line2);
		else {
			s = DOdata("RETR", line2, RETR, fd);
			close(fd);
			if(s < 0) break;
		}
	}
	fclose(fp);
   } else
	unlink(name);

   return(s);
}

int DOappe()
{
char *file, *remotefile;
int fd;
int s;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Local File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   s = DOdata("APPE", remotefile, STOR, fd);

   close(fd);

   return(s);
}

int DOstor()
{
char *file, *remotefile;
int fd;
int s;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Local File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   s = DOdata("STOR", remotefile, STOR, fd);

   close(fd);

   return(s);
}

int DOrstor()
{
char *file, *remotefile;
int fd;
int s;
off_t filesize, rmtsize;
char restart[16];

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Local File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   s = DOcommand("SIZE", remotefile);

   if(s != 215)
   	return(s);

   rmtsize = atol(reply+4);

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   if(type == TYPE_A)
   	filesize = asciisetsize(fd, rmtsize);
   else
	filesize = lseek(fd, rmtsize, SEEK_SET);

   if(filesize != rmtsize) {
	printf("Could not set file start of %s\n", file);
   	close(fd);
   	return(0);
   }

   sprintf(restart, "%u", rmtsize);

   s = DOcommand("REST", restart);

   if(s != 350) {
   	close(fd);
   	return(s);
   }

   s = DOdata("STOR", remotefile, STOR, fd);

   close(fd);

   return(s);
}

int DOstou()
{
char *file, *remotefile;
int fd;
int s;

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Local File: ", line2, sizeof(line2)) < 0)
		return(-1);
	file = line2;
   }

   if(cmdargc < 3)
	remotefile = file;
   else
	remotefile = cmdargv[2];

   fd = open(file, O_RDONLY);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", file, strerror(errno));
	return(0);
   }

   s = DOdata("STOU", remotefile, STOR, fd);

   close(fd);

   return(s);
}

int DOMstor()
{
char *files;
char *name;
char *p;
int fd, s;
FILE *fp;

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   if(cmdargc < 2) {
	if(readline("Files: ", line2, sizeof(line2)) < 0)
		return(-1);
	files = line2;
   }

   name = dir(files, 0);

   fp = fopen(name, "r");

   if(fp == (FILE *)NULL) {
	printf("Unable to open listing file.\n");
	return(0);
   }

   while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
	p = line2 + strlen(line2) - 1;
	if(p >= line2 && (*p == '\r' || *p == '\n')) *p-- = '\0';
	if(p >= line2 && (*p == '\r' || *p == '\n')) *p-- = '\0';
	printf("Sending file: %s\n", line2); fflush(stdout);
	fd = open(line2, O_RDONLY);
	if(fd < 0)
		printf("Unable to open local file %s\n", line2);
	else {
		s = DOdata("STOR", line2, STOR, fd);
		close(fd);
		if(s < 0) break;
	}
   }
   fclose(fp);
   unlink(name);

   return(s);
}

static int asciisize(fd, filesize)
int fd;
off_t *filesize;
{
unsigned long count;
char *p, *pp;
int cnt;

   count = 0;

   while((cnt = read(fd, buffer, sizeof(buffer))) > 0) {
	p = buffer; pp = buffer + cnt;
	count += cnt;
	while(p < pp)
		if(*p++ == '\n')
			count++;
   }

   if(cnt == 0) {
	*filesize = count;
	return(0);
   }

   return(-1);
}

static off_t asciisetsize(fd, filesize)
int fd;
off_t filesize;
{
off_t sp;
int s;

   sp = 0;

   while(sp < filesize) {
	s = read(fd, buffer, 1);
	if(s < 0)
		return(-1);
	if(s == 0) break;
	sp++;
	if(*buffer == '\n')
		sp++;
   }

   return(sp);
}

int DOclone()
{
char *files;
int fd, s;
char *p;
FILE *fp;
char name[32];

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   tmpnam(name);

   fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0666);

   if(fd < 0) {
	printf("Could not open local file %s. Error %s\n", name, strerror(errno));
	return(0);
   }

   s = DOdata("NLST", files, RETR, fd);

   close(fd);

   if(s == 226 || s == 250) {
	fp = fopen(name, "r");
	unlink(name);
	if(fp == (FILE *)NULL) {
		printf("Unable to open file listing.\n");
		return(0);
	}
	while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
		p = line2 + strlen(line2) - 1;
		if(p >= line2 && (*p == '\r' || *p == '\n')) *p-- = '\0';
		if(p >= line2 && (*p == '\r' || *p == '\n')) *p-- = '\0';
		cmdargv[1] = line2;
		if(cloneit(line2, 1)) {
			printf("Retrieving file: %s\n", line2); fflush(stdout);
			fd = open(line2, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if(fd < 0)
				printf("Unable to open local file %s\n", line2);
			else {
				s = DOdata("RETR", line2, RETR, fd);
				close(fd);
				if(s < 0) break;
			}
			s = cloneit(line2, 2);
		}
	}
	fclose(fp);
   } else
	unlink(name);

   return(s);
}
