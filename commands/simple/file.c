/* file - report on file type.		Author: Andy Tanenbaum */
/* Magic number detection changed to look-up table 08-Jan-91 - ajm */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define BLOCK_SIZE	1024

#define XBITS 00111		/* rwXrwXrwX (x bits in the mode) */
#define ENGLISH 25		/* cutoff for determining if text is Eng. */
unsigned char buf[BLOCK_SIZE];

struct info {
  int execflag;			/* 1 == ack executable, 2 == gnu executable,
				 * 3 == core */
  unsigned char magic[4];	/* First four bytes of the magic number */
  unsigned char mask[4];	/* Mask to apply when matching */
  char *description;		/* What it means */
} table[] = {
  0x00, 0x1f, 0x9d, 0x8d, 0x00,		0xff, 0xff, 0xff, 0x00,
	"13-bit compressed file",
  0x00, 0x1f, 0x9d, 0x90, 0x00,		0xff, 0xff, 0xff, 0x00,
	"16-bit compressed file",
  0x00, 0x65, 0xff, 0x00, 0x00,		0xff, 0xff, 0x00, 0x00,
	"MINIX-PC bcc archive",
  0x00, 0x2c, 0xff, 0x00, 0x00,		0xff, 0xff, 0x00, 0x00,
	"ACK object archive",
  0x00, 0x65, 0xff, 0x00, 0x00,		0xff, 0xff, 0x00, 0x00,
	"MINIX-PC ack archive",
  0x00, 0x47, 0x6e, 0x75, 0x20,		0xff, 0xff, 0xff, 0xff,
	"MINIX-68k gnu archive",
  0x00, 0x21, 0x3c, 0x61, 0x72,		0xff, 0xff, 0xff, 0xff,
	"MINIX-PC gnu archive",
  0x00, 0x01, 0x02, 0x00, 0x00,		0xff, 0xff, 0x00, 0x00,
	"ACK object file",
  0x00, 0xa3, 0x86, 0x00, 0x00, 	0xff, 0xff, 0x00, 0x00,
	"MINIX-PC bcc object file",
  0x00, 0x00, 0x00, 0x01, 0x07, 	0xff, 0xff, 0xff, 0xff,
	"MINIX-68k gnu object file",
  0x00, 0x07, 0x01, 0x00, 0x00, 	0xff, 0xff, 0xff, 0xff,
	"MINIX-PC gnu object file",
  0x01, 0x01, 0x03, 0x00, 0x04, 	0xff, 0xff, 0x00, 0xff,
	"MINIX-PC 16-bit executable",
  0x01, 0x01, 0x03, 0x00, 0x10, 	0xff, 0xff, 0x00, 0xff,
	"MINIX-PC 32-bit executable",
  0x01, 0x04, 0x10, 0x03, 0x01, 	0xff, 0xff, 0xff, 0xff,
	"MINIX-68k old style executable",
  0x01, 0x01, 0x03, 0x10, 0x0b, 	0xff, 0xff, 0xff, 0xff,
	"MINIX-68k new style executable",
  0x02, 0x0b, 0x01, 0x00, 0x00, 	0xff, 0xff, 0xff, 0xff,
	"MINIX-PC 32-bit gnu executable combined I & D space",
  0x02, 0x00, 0x00, 0x0b, 0x01, 	0xff, 0xff, 0xff, 0xff,
	"MINIX-68k gnu executable",
  0x03, 0x82, 0x12, 0xC4, 0xC0,		0xff, 0xff, 0xff, 0xff,
	"core file",
};

int tabsize = sizeof(table) / sizeof(struct info);
int L_flag;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void file, (char *name));
_PROTOTYPE(void do_strip, (int type));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
/* This program uses some heuristics to try to guess about a file type by
 * looking at its contents.
 */
  int c, i;

  L_flag= 0;
  while ((c= getopt(argc, argv, "L?")) != -1)
  {
	switch(c)
	{
	case 'L':
		L_flag= 1;
		break;
	case '?':
		usage();
	default:
		fprintf(stderr, "file: panic getopt failed\n");
		exit(1);
	}
  }
  if (optind >= argc) usage();
  for (i = optind; i < argc; i++) file(argv[i]);
  return(0);
}

void file(name)
char *name;
{
  int i, fd, n, mode, nonascii, special, funnypct, etaoins;
  int j;
  long engpct;
  int c;
  struct stat st_buf;

  printf("%s: ", name);

#ifdef S_IFLNK
  if (!L_flag)
	n = lstat(name, &st_buf);
  else
	n = stat(name, &st_buf);
#else
  n = stat(name, &st_buf);
#endif
  if (n < 0) {
	printf("cannot stat\n");
	return;
  }
  mode = st_buf.st_mode;

  /* Check for directories and special files. */
  if (S_ISDIR(mode)) {
	printf("directory\n");
	return;
  }
  if (S_ISCHR(mode)) {
	printf("character special file\n");
	return;
  }
  if (S_ISBLK(mode)) {
	printf("block special file\n");
	return;
  }
  if (S_ISFIFO(mode)) {
	printf("named pipe\n");
	return;
  }
#ifdef S_IFLNK
  if (S_ISLNK(mode)) {
	n= readlink(name, (char *)buf, BLOCK_SIZE);
	if (n == -1)
		printf("cannot readlink\n");
	else
		printf("symbolic link to %.*s\n", n, buf);
	return;
  }
#endif
  if (!S_ISREG(mode)) {
	printf("strange file type %5o\n", mode);
	return;
  }

  /* Open the file, stat it, and read in 1 block. */
  fd = open(name, O_RDONLY);
  if (fd < 0) {
	printf("cannot open\n");
	return;
  }
  n = read(fd, (char *)buf, BLOCK_SIZE);
  if (n < 0) {
	printf("cannot read\n");
	close(fd);
	return;
  }
  if (n == 0) {       /* must check this, for loop will fail otherwise !! */
      printf("empty file\n");
      close(fd);
      return;
  }

  for (i = 0; i < tabsize; i++) {
	for (j = 0; j < 4; j++)
		if ((buf[j] & table[i].mask[j]) != table[i].magic[j])
			break;
	if (j == 4) {
		printf("%s", table[i].description);
		do_strip(table[i].execflag);
		close(fd);
		return;
	}
  }


  /* Check to see if file is a shell script. */
  if (mode & XBITS) {
	/* Not a binary, but executable.  Probably a shell script. */
	printf("shell script\n");
	close(fd);
	return;
  }

  /* Check for ASCII data and certain punctuation. */
  nonascii = 0;
  special = 0;
  etaoins = 0;
  for (i = 0; i < n; i++) {
	c = buf[i];
	if (c & 0200) nonascii++;
	if (c == ';' || c == '{' || c == '}' || c == '#') special++;
	if (c == '*' || c == '<' || c == '>' || c == '/') special++;
	if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
	if (c == 'e' || c == 't' || c == 'a' || c == 'o') etaoins++;
	if (c == 'i' || c == 'n' || c == 's') etaoins++;
  }

  if (nonascii == 0) {
	/* File only contains ASCII characters.  Continue processing. */
	funnypct = 100 * special / n;
	engpct = 100L * (long) etaoins / n;
	if (funnypct > 1) {
		printf("C program\n");
	} else {
		if (engpct > (long) ENGLISH)
			printf("English text\n");
		else
			printf("ASCII text\n");
	}
	close(fd);
	return;
  }

  /* Give up.  Call it data. */
  printf("data\n");
  close(fd);
  return;
}

void do_strip(type)
int type;
{
  if (type == 1) {	/* Non-GNU executable */
	if (buf[2] & 1)
		printf(", UZP");
	if (buf[2] & 2)
		printf(", PAL");
	if (buf[2] & 4)
		printf(", NSYM");
	if (buf[2] & 0x20)
		printf(", sep I&D");
	else
		printf(", comm I&D");
	if (( buf[28] | buf[29] | buf[30] | buf[31]) != 0)
		printf(" not stripped\n");
	else
		printf(" stripped\n");
	return;
  }

  if (type == 2) {	/* GNU format executable */
     if ((buf[16] | buf[17] | buf[18] | buf[19]) != 0)
	 printf(" not stripped\n");
     else
	 printf(" stripped\n");
     return;
  }

  if (type == 3) {	/* Core file in <sys/core.h> format */
	switch(buf[36] & 0xff)
	{
		case 1:	printf(" of i86 executable"); break;
		case 2:	printf(" of i386 executable"); break;
		default:printf(" of unknown executable"); break;
	}
	printf(" '%.32s'\n", buf+4);
	return;
  }

  printf("\n");		/* Not an executable file */
 }

void usage()
{
  printf("Usage: file [-L] name ...\n");
  exit(1);
}
