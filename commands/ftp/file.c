/* file.c
 *
 * This file is part of ftp.
 *
 *
 * 01/25/96 Initial Release	Michael Temari, <temari@ix.netcom.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "ftp.h"
#include "file.h"
#include "net.h"

_PROTOTYPE(static char *dir, (char *path, int full));
_PROTOTYPE(static int asciisend, (int fd, int fdout));
_PROTOTYPE(static int binarysend, (int fd, int fdout));
_PROTOTYPE(static int asciirecv, (int fd, int fdin));
_PROTOTYPE(static int binaryrecv, (int fd, int fdin));
_PROTOTYPE(static int asciisize, (int fd, off_t *filesize));
_PROTOTYPE(static off_t asciisetsize, (int fd, off_t filesize));

static char buffer[512 << sizeof(char *)];
static char bufout[512 << sizeof(char *)];
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
	sprintf(cmd, "ls %s > %s", path, name);

   system(cmd);

   return(name);
}

static int asciisend(fd, fdout)
int fd;
int fdout;
{
int s, len;
char c;
char *p;
char *op, *ope;
unsigned long total=0L;

   if(atty) {
	printf("Sent ");
	fflush(stdout);
   }

   op = bufout;
   ope = bufout + sizeof(bufout) - 3;

   while((s = read(fd, buffer, sizeof(buffer))) > 0) {
	total += (long)s;
	p = buffer;
	while(s-- > 0) {
		c = *p++;
		if(c == '\r') {
			*op++ = '\r';
			total++;
		}
		*op++ = c;
		if(op >= ope) {
			write(fdout, bufout, op - bufout);
			op = bufout;
		}
	}
	if(atty) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(op > bufout)
	write(fdout, bufout, op - bufout);
   if(atty) {
	printf("\n");
	fflush(stdout);
   }

   return(s);
}

static int binarysend(fd, fdout)
int fd;
int fdout;
{
int s;
unsigned long total=0L;

   if(atty) {
	printf("Sent ");
	fflush(stdout);
   }

   while((s = read(fd, buffer, sizeof(buffer))) > 0) {
	write(fdout, buffer, s);
	total += (long)s;
	if(atty) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(atty) {
	printf("\n");
	fflush(stdout);
   }

   return(s);
}

int sendfile(fd, fdout)
int fd;
int fdout;
{
int s;

   switch(type) {
	case TYPE_A:
		s = asciisend(fd, fdout);
		break;
	default:
		s = binarysend(fd, fdout);
   }

   if(s < 0)
	return(-1);
   else
	return(0);
}

static int asciirecv(fd, fdin)
int fd;
int fdin;
{
int s, len;
int gotcr;
char c;
char *p;
char *op, *ope;
unsigned long total=0L;

   if(isatty && fd > 2) {
	printf("Received ");
	fflush(stdout);
   }
   gotcr = 0;
   op = bufout; ope = bufout + sizeof(bufout) - 3;
   while((s = read(fdin, buffer, sizeof(buffer))) > 0) {
	p = buffer;
	total += (long)s;
	while(s-- > 0) {
		c = *p++;
		if(gotcr) {
			gotcr = 0;
			if(c != '\n')
				*op++ = '\r';
		}
		if(c == '\r')
			gotcr = 1;
		else
			*op++ = c;
		if(op >= ope) {
			write(fd, bufout, op - bufout);
			op = bufout;
		}
	}
	if(atty && fd > 2) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(gotcr)
	*op++ = '\r';
   if(op > bufout)
	write(fd, bufout, op - bufout);
   if(atty && fd > 2) {
	printf("\n");
	fflush(stdout);
   }
   return(s);
}

static binaryrecv(fd, fdin)
int fd;
int fdin;
{
int s;
unsigned long total=0L;

   if(atty && fd > 2) {
	printf("Received ");
	fflush(stdout);
   }
   while((s = read(fdin, buffer, sizeof(buffer))) > 0) {
	write(fd, buffer, s);
	total += (long)s;
	if(atty && fd > 2) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(atty && fd > 2) {
	printf("\n");
	fflush(stdout);
   }
   return(s);
}

int recvfile(fd, fdin)
int fd;
int fdin;
{
int s;

   switch(type) {
	case TYPE_A:
		s = asciirecv(fd, fdin);
		break;
	default:
		s = binaryrecv(fd, fdin);
   }

   if(s < 0)
	return(-1);
   else
	return(0);
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
	readline("Path: ", line2, sizeof(line2));
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
	readline("Directory: ", line2, sizeof(line2));
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
	readline("Directory: ", line2, sizeof(line2));
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
	readline("File: ", line2, sizeof(line2));
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
	readline("File: ", line2, sizeof(line2));
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
	readline("File: ", line2, sizeof(line2));
	file = line2;
   }

   return(DOcommand("SIZE", file));
}

int DOstat()
{
char *file;

   if(cmdargc < 2)
	if(!linkopen) {
		printf("You must \"OPEN\" a connection first.\n");
		return(0);
	} else
		return(DOcommand("STAT", ""));

   if(DOcmdcheck())
	return(0);

   file = cmdargv[1];

   if(cmdargc < 2) {
	readline("File: ", line2, sizeof(line2));
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
	readline("Remote File: ", line2, sizeof(line2));
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
	readline("Remote File: ", line2, sizeof(line2));
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

   sprintf(restart, "%lu", filesize);

   s = DOcommand("REST", restart);

   if(s != 350) {
   	close(fd);
   	return(s);
   }

   s = DOdata("RETR", file, RETR, fd);

   close(fd);

   return(s);
}

int DOMretr()
{
char *files;
int fd, s;
FILE *fp;
char name[32];

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   if(cmdargc < 2) {
	readline("Files: ", line2, sizeof(line2));
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

   if(s == 226) {
	fp = fopen(name, "r");
	unlink(name);
	if(fp == (FILE *)NULL) {
		printf("Unable to open file listing.\n");
		return(0);
	}
	while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
		line2[strlen(line2)-1] = '\0';
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
	readline("Local File: ", line2, sizeof(line2));
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
	readline("Local File: ", line2, sizeof(line2));
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
	readline("Local File: ", line2, sizeof(line2));
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

   sprintf(restart, "%lu", rmtsize);

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
	readline("Local File: ", line2, sizeof(line2));
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
int fd, s;
FILE *fp;

   if(DOcmdcheck())
	return(0);

   files = cmdargv[1];

   if(cmdargc < 2) {
	readline("Files: ", line2, sizeof(line2));
	files = line2;
   }

   name = dir(files, 0);

   fp = fopen(name, "r");

   if(fp == (FILE *)NULL) {
	printf("Unable to open listing file.\n");
	return(0);
   }

   while(fgets(line2, sizeof(line2), fp) != (char *)NULL) {
	line2[strlen(line2)-1] = '\0';
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

   return(1);
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
