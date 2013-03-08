/* vol - break stdin into volumes	Author: Andy Tanenbaum */

/* This program reads standard input and writes it onto diskettes, pausing
 * at the start of each one.  It's main use is for saving files that are
 * larger than a single diskette.  Vol just writes its standard input onto
 * a diskette, and prompts for a new one when it is full.  This mechanism
 * is transparent to the process producing vol's standard input. For example,
 *	tar cf - . | vol -w 360 /dev/fd0
 * puts the tar output as as many diskettes as needed.  To read them back in,
 * use
 *	vol -r 360 /dev/fd0 | tar xf -
 *
 * Changed 17 Nov 1993 by Kees J. Bot to handle buffering to slow devices.
 * Changed 27 Jul 1994 by Kees J. Bot to auto discover data direction + -rw.
 * Changed 19 Sep 1995 by Kees J. Bot to do better buffering to tapes.
 */

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <minix/partition.h>
#include <minix/u64.h>

/* Preferred block size to variable block length tapes, block devices or files.
 */
#define VAR_BLKSIZ	   8192

/* Required block size multiple of fixed block size tapes (usually updated by
 * 'mt status' data) and character devices.
 */
#define FIX_BLKSIZ	    512

/* Maximum multiple block size. */
#if __minix_vmd
#define MULT_MAX	1048576
#else
#define MULT_MAX	((ssize_t) (SSIZE_MAX < 65536L ? SSIZE_MAX : 65536L))
#endif

char *buffer = NULL;
size_t block_size = 0, mult_max = 0;
size_t buffer_size;
long volume_size;
char *str_vol_size;
int rflag = 0, wflag = 0, oneflag = 0, variable = 0;

int main(int argc, char **argv);
void usage(void);
long str2size(char *name, char *str, long min, long max, int assume_kb);
void tape_inquire(char *name, int fd);
void allocate_buffer(void);
void diskio(int fd1, int fd2, char *file1, char *file2);

int main(argc, argv)
int argc;
char *argv[];
{
  int volume = 1, fd, tty, i, init, autovolsize;
  char *p, *name;
  struct stat stb;
  struct part_geom part;
  char key;

  /* Fetch and verify the arguments. */
  i = 1;
  while (i < argc && argv[i][0] == '-') {
	p = argv[i++] + 1;
	if (p[0] == '-' && p[1] == 0) {
		/* -- */
		i++;
		break;
	}
	while (*p != '\0') {
		switch (*p++) {
		case 'r':
		case 'u':
			rflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case '1':
			oneflag = 1;
			break;
		case 'b':
			if (*p == 0) {
				if (i == argc) usage();
				p = argv[i++];
			}
			block_size = str2size("block", p,
						1L, (long) SSIZE_MAX, 0);
			p= "";
			break;
		case 'm':
			if (*p == 0) {
				if (i == argc) usage();
				p = argv[i++];
			}
			mult_max = str2size("maximum", p,
						1L, (long) SSIZE_MAX, 0);
			p= "";
			break;
		default:
			usage();
		}
	}
  }
  if (i < argc - 1) {
	str_vol_size = argv[i++];
	volume_size = str2size("volume", str_vol_size, 1L, LONG_MAX, 1);
	autovolsize = 0;
  } else {
	volume_size = 0;	/* unlimited (long tape) or use DIOCGETP */
	autovolsize = 1;
  }

  if (i >= argc) usage();
  name = argv[i];

  if (!rflag && !wflag) {
	/* Auto direction.  If there is a terminal at one side then data is
	 * to go out at the other side.
	 */
	if (isatty(0)) rflag = 1;
	if (isatty(1)) wflag = 1;
  }

  if (rflag == wflag) {
	fprintf(stderr, "vol: should %s be read or written?\n", name);
	usage();
  }

  if (stat(name, &stb) < 0) {
	fprintf(stderr, "vol: %s: %s\n", name, strerror(errno));
	exit(1);
  }
  if (!S_ISBLK(stb.st_mode) && !S_ISCHR(stb.st_mode)) {
	fprintf(stderr, "vol: %s is not a device\n", name);
	exit(1);
  }
  variable = !S_ISCHR(stb.st_mode);

  if (!oneflag) {
	tty = open("/dev/tty", O_RDONLY);
	if (tty < 0) {
		fprintf(stderr, "vol: cannot open /dev/tty\n");
		exit(1);
	}
  }

  /* Buffer initializations are yet to be done. */
  init = 0;

  while (1) {
	sleep(1);
	if (oneflag) {
		if (volume != 1) {
			if (rflag) exit(0);
			fprintf(stderr,
				"vol: can't continue, volume is full\n");
			exit(1);
		}
	} else {
		fprintf(stderr,
			"\007Please insert %sput volume %d and hit return\n",
			rflag ? "in" : "out", volume);
		while (read(tty, &key, sizeof(key)) == 1 && key != '\n') {}
	}

	/* Open the special file. */
	fd = open(name, rflag ? O_RDONLY : O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "vol: %s: %s\n", name, strerror(errno));
		exit(1);
	}

	if (!init) {
		/* Ask for the tape block size and allocate a buffer. */
		if (S_ISCHR(stb.st_mode)) tape_inquire(name, fd);
		allocate_buffer();
		init = 1;
	}

	if (autovolsize) {
		/* Ask the driver how big the volume is. */
		if (ioctl(fd, DIOCGETP, &part) < 0) {
			autovolsize = 0;
		} else {
			volume_size = cv64ul(part.size);
		}
	}

	/* Read or write the requisite number of blocks. */
	if (rflag) {
		diskio(fd, 1, name, "stdout");	/* vol -r | tar xf - */
	} else {
		diskio(0, fd, "stdin", name);	/* tar cf - | vol -w */
	}
	close(fd);
	volume++;
  }
}

void usage()
{
  fprintf(stderr,
	"Usage: vol [-rw1] [-b blocksize] [-m max] [size] block-special\n");
  exit(1);
}

long str2size(name, str, min, max, assume_kb)
char *name;
char *str;
long min, max;
int assume_kb;
{
  /* Convert a string to a size.  The number may be followed by 'm', 'k', 'b'
   * or 'w' to multiply the size as shown below.  If 'assume_kb' is set then
   * kilobytes is the default.
   */
  long size, factor;
  char *ptr;
  int bad;

  errno = 0;
  size = strtol(str, &ptr, 10);
  bad = (errno != 0 || ptr == str || size < min || size > max);
  if (*ptr == 0 && assume_kb) ptr = "k";
  while (!bad && *ptr != 0) {
	switch (*ptr++) {
	case 'm':
	case 'M':
		factor = 1024*1024L; break;
	case 'k':
	case 'K':
		factor = 1024; break;
	case 'b':
	case 'B':
		factor = 512; break;
	case 'w':
	case 'W':
		factor = 2; break;
	default:
		factor = 1; bad = 1;
	}
	if (size <= max / factor) size *= factor; else bad = 1;
  }
  if (bad) {
	fprintf(stderr, "vol: bad %s size '%s'\n", name, str);
	exit(1);
  }
  return size;
}

void tape_inquire(name, fd)
char *name;
int fd;
{
  /* If the device happens to be a tape, then what is its block size? */
  struct mtget mtget;

  if (ioctl(fd, MTIOCGET, &mtget) < 0) {
	if (errno != ENOTTY) {
		fprintf(stderr, "vol: %s: %s\n", name,
					strerror(errno));
		exit(1);
	}
  } else {
	if (mtget.mt_blksize > SSIZE_MAX) {
		fprintf(stderr,
		"vol: %s: tape block size (%lu) is too large to handle\n",
			name, (unsigned long) mtget.mt_blksize);
		exit(1);
	}
	if (mtget.mt_blksize == 0) {
		variable = 1;
	} else {
		/* fixed */
		block_size = mtget.mt_blksize;
	}
  }
}

void allocate_buffer()
{
  /* Set block size and maximum multiple. */
  if (block_size == 0) block_size = variable ? 1 : FIX_BLKSIZ;
  if (mult_max == 0) mult_max = variable ? VAR_BLKSIZ : MULT_MAX;

  /* Stretch the buffer size to the max. */
  buffer_size = mult_max / block_size * block_size;
  if (buffer_size == 0) buffer_size = block_size;

  if (volume_size % block_size != 0) {
	fprintf(stderr,
	"vol: volume size (%s) is not a multiple of the block size (%lu)\n",
		str_vol_size, (unsigned long) block_size);
	exit(1);
  }

  buffer = (char *) malloc(buffer_size);
  if (buffer == NULL) {
	fprintf(stderr, "vol: cannot allocate a %luk buffer\n",
		(unsigned long) buffer_size / 1024);
	exit(1);
  }
}

void diskio(fd1, fd2, file1, file2)
int fd1, fd2;
char *file1, *file2;
{
/* Read 'volume_size' bytes from 'fd1' and write them on 'fd2'.  Watch out for
 * the fact that reads on pipes can return less than the desired data.
 */

  ssize_t n, in_needed, in_count, out_count;
  long needed = volume_size;
  int eof = 0;

  for (;;) {
	if (volume_size == 0) needed = buffer_size;

	if (needed == 0) break;

	in_count = 0;
	in_needed = needed > buffer_size ? buffer_size : needed;
	while (in_count < in_needed) {
		n = in_needed - in_count;
		n = eof ? 0 : read(fd1, buffer + in_count, n);
		if (n == 0) {
			eof = 1;
			if ((n = in_count % block_size) > 0) {
				n = block_size - n;
				memset(buffer + in_count, '\0', n);
				if ((in_count += n) > in_needed)
					in_count = in_needed;
			}
			break;
		}
		if (n < 0) {
			fprintf(stderr, "vol: %s: %s\n",
						file1, strerror(errno));
			exit(1);
		}
		in_count += n;
	}
	if (in_count == 0) exit(0);	/* EOF */
	out_count = 0;
	while (out_count < in_count) {
		n = in_count - out_count;
		n = write(fd2, buffer + out_count, n);
		if (n < 0) {
			fprintf(stderr, "vol: %s: %s\n",
						file2, strerror(errno));
			exit(1);
		}
		out_count += n;
	}
	needed -= in_count;
  }
}
