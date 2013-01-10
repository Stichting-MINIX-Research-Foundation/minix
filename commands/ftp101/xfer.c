/* xfer.c Copyright 1992-2000 by Michael Temari All Rights Reserved
 *
 * This file is part of ftp.
 *
 *
 * 03/14/00 Initial Release	Michael Temari, <Michael@TemWare.Com>
 *
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
#include "xfer.h"

static int asciisend(int fd, int fdout);
static int binarysend(int fd, int fdout);
static int asciirecv(int fd, int fdin);
static int binaryrecv(int fd, int fdin);

#if (__WORD_SIZE == 4)
static char buffer[8192];
static char bufout[8192];
#else
static char buffer[2048];
static char bufout[2048];
#endif

static int asciisend(fd, fdout)
int fd;
int fdout;
{
int s;
char c;
char *p;
char *op, *ope;
unsigned long total=0L;
char block[3];

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
		if(c == '\n') {
			*op++ = '\r';
			total++;
		}
		*op++ = c;
		if(op >= ope) {
			if(mode == MODE_B) {
				block[0] = '\0';
				*(u16_t *)&block[1] = htons(op - bufout);
				write(fdout, block, sizeof(block));
			}
			write(fdout, bufout, op - bufout);
			op = bufout;
		}
	}
	if(atty) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(op > bufout) {
   	if(mode == MODE_B) {
		block[0] = MODE_B_EOF;
		*(u16_t *)&block[1] = htons(op - bufout);
		write(fdout, block, sizeof(block));
	}
	write(fdout, bufout, op - bufout);
   } else if(mode == MODE_B) {
	block[0] = MODE_B_EOF;
	*(u16_t *)&block[1] = htons(0);
	write(fdout, block, sizeof(block));
   	s = 0;
   }
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
char block[3];

   if(atty) {
	printf("Sent ");
	fflush(stdout);
   }

   while((s = read(fd, buffer, sizeof(buffer))) > 0) {
   	if(mode == MODE_B) {
		block[0] = MODE_B_EOF;
		*(u16_t *)&block[1] = htons(s);
		write(fdout, block, sizeof(block));
	}
	write(fdout, buffer, s);
	total += (long)s;
	if(atty) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
   }
   if(mode == MODE_B) {
	block[0] = MODE_B_EOF;
	*(u16_t *)&block[1] = htons(0);
	write(fdout, block, sizeof(block));
   	s = 0;
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
int s;
int gotcr;
char c;
char *p;
char *op, *ope;
unsigned long total=0L;
char block[3];
unsigned short cnt;

   if(isatty && fd > 2) {
	printf("Received ");
	fflush(stdout);
   }
   gotcr = 0;
   op = bufout; ope = bufout + sizeof(bufout) - 3;
   cnt = 0;
   while(1) {
   	if(mode != MODE_B)
   		cnt = sizeof(buffer);
   	else
   		if(cnt == 0) {
   			s = read(fdin, block, sizeof(block));
   			cnt = ntohs(*(u16_t *)&block[1]);
   			s = 0;
   			if(cnt == 0 && block[0] & MODE_B_EOF)
   				break;
   		}
	s = read(fdin, buffer, cnt > sizeof(buffer) ? sizeof(buffer) : cnt);
	if(s <= 0) break;
	cnt -= s;
	total += (long)s;
	p = buffer;
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
	if(cnt == 0 && mode == MODE_B && block[0] & MODE_B_EOF) {
			s = 0;
			break;
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
   if((mode == MODE_B && cnt != 0) || s != 0)
   	return(-1);
   else
   	return(0);
}

static int binaryrecv(fd, fdin)
int fd;
int fdin;
{
int s;
unsigned long total=0L;
char block[3];
unsigned short cnt;

   if(atty && fd > 2) {
	printf("Received ");
	fflush(stdout);
   }
   cnt = 0;
   while(1) {
   	if(mode != MODE_B)
   		cnt = sizeof(buffer);
   	else
	   	if(cnt == 0) {
   			s = read(fdin, block, sizeof(block));
   			cnt = ntohs(*(u16_t *)&block[1]);
   			s = 0;
			if(cnt == 0 && block[0] & MODE_B_EOF)
				break;
		}
	s = read(fdin, buffer, cnt > sizeof(buffer) ? sizeof(buffer) : cnt);
	if(s <= 0) break;
	cnt -= s;
	total += (long)s;
	write(fd, buffer, s);
	if(atty && fd > 2) {
		printf("%8lu bytes\b\b\b\b\b\b\b\b\b\b\b\b\b\b", total);
		fflush(stdout);
	}
	if(cnt == 0 && mode == MODE_B && block[0] & MODE_B_EOF) {
		s = 0;
		break;
	}
   }
   if(atty && fd > 2) {
	printf("\n");
	fflush(stdout);
   }
   if((mode == MODE_B && cnt != 0) || s != 0)
   	return(-1);
   else
   	return(0);
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
