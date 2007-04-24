/* readall - read a whole device fast		Author: Andy Tanenbaum */

/* Readall reads all the blocks on a device as fast as it can.  If it hits
 * an error, it stops reading in large units and reads one block at a time.
 * It reports on all errors it finds.
 *
 * If the -b flag is given, the output is a shell script that can be run
 * to mark all the bad blocks.
 *
 * If the -t flag is given, only the total numbers of blocks is reported.
 *
 * Examples of usage:
 *	readall /dev/hd1		# read /dev/hd1
 *	readall -b /dev/hd2		# prepare bad block list on stdout
 *	readall -t /dev/ram		# report size of ram disk
 */

#include <sys/types.h>
#include <sys/ioc_disk.h>
#include <minix/partition.h>
#include <minix/u64.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define CHUNK    25		/* max number of blocks read at once */
#define BLOCK_SIZE	1024	/* size of a block */
#define RESUME  200		/* # good reads before going back to CHUNK */
#define DIVISOR	 1000		/* how often to print statistics */
#define STORE  4096		/* save this many bad blocks for summary */

int chunk = CHUNK;		/* current number of blocks being read */
long goodies;			/* incremented on good reads */
long errors;			/* number of errors so far */
int normal = 1;			/* set unless -b flag is given */
int total = 0;			/* unset unless -t flag is given */
char *name;			/* name of special file being read */

char a[CHUNK * BLOCK_SIZE];	/* read buffer */
long rotten[STORE];		/* list of bad blocks */

_PROTOTYPE(int main, (int argc, char **argv));
static _PROTOTYPE(void output, (long blocks_read));

/* print pretty progress meter with remaining no. of blocks and ETA on
 * stderr
 */
void
prettyprogress(long b, long nblocks, time_t starttime)
{
  /* print progress indication */
  time_t spent, now;
  long bpsec;
  time(&now);
  spent = now - starttime;
  if(spent > 0 && (bpsec = b / spent) > 0) {
  	int len, i;
  	long secremain, minremain, hremain;
	  secremain = (nblocks - b) / bpsec;
	  minremain = (secremain / 60) % 60;
	  hremain = secremain / 3600;
  	len = fprintf(stderr, "Remain %ld blocks. ETA: %d:%02d:%02d  [",
  		nblocks - b,
  		hremain, minremain, secremain % 60);
#define WIDTH 77
  	len = WIDTH - len;
  	for(i = 0; i < (b * (len-1) / nblocks); i++) 
  		fprintf(stderr, "=");
 	fprintf(stderr, "|");
  	for(; i < len-2; i++) 
  		fprintf(stderr, "-");
  	fprintf(stderr, "]\r");
  	fflush(stderr);
  }

  return;
}

int main(argc, argv)
int argc;
char *argv[];
{
  struct partition entry;
  int fd, s, i, badprinted;
  long b = 0, nblocks;
  char *p;
  time_t starttime;

  if (argc != 2 && argc != 3) {
	fprintf(stderr, "Usage: readall [-b | -t] file\n");
	exit(1);
  }
  i = 1;

  p = argv[1];
  if (*p == '-' && *(p + 1) == 'b' && *(p + 2) == '\0') {
	normal = 0;
	i++;
	name = argv[i];
  }
  if (*p == '-' && *(p + 1) == 't' && *(p + 2) == '\0') {
	normal = 0;
	total = 1;
	i++;
	name = argv[i];
  }
  fd = open(argv[i], O_RDONLY);
  if (fd < 0) {
  	perror(argv[i]);
	fprintf(stderr, "%s is not readable\n", argv[i]);
	exit(1);
  }

  /* Get size of file */
  if(ioctl(fd, DIOCGETP, &entry) < 0) {
  	perror("ioctl DIOCGETP");
  	return 1;
  }
  nblocks = div64u(entry.size, BLOCK_SIZE);

  time(&starttime);
  /* Read the entire file. Try it in large chunks, but if an error
   * occurs, go to single reads for a while. */
  while (1) {
	if(lseek64(fd, mul64u(BLOCK_SIZE, b), SEEK_SET, NULL) < 0) {
		perror("lseek64");
		return 1;
	}
	s = read(fd, a, BLOCK_SIZE * chunk);
	if (s == BLOCK_SIZE * chunk) {
		/* Normal read, no errors. */
		b += chunk;
		goodies++;
		if (chunk == 1) {
			if (goodies >= RESUME && b % DIVISOR == 0)
				chunk = CHUNK;
		}
  		if(b % DIVISOR == 0 && !normal) {
  			prettyprogress(b, nblocks, starttime);
  		}
	} else if (s < 0) {
		/* I/O error. */
		if (chunk != 1) {
			chunk = 1;	/* regress to single block mode */
			continue;
		}
		if (errors == STORE) {
			fprintf(stderr,
			 "\n%ld Bad blocks is too many.  Exiting\n",
				errors);
			exit(1);
		}
		rotten[(int) errors] = b;	/* log the error */
		b += chunk;
		errors++;
	} else {
		/* End of file. */
		b += s / BLOCK_SIZE;
		if (normal) {
			output(b);
			fprintf(stderr, "\n");
		} else fprintf(stderr, "\r%*s\n", -WIDTH, "Done scanning.");
		if (total) printf("%8ld\n", b);
		if ((errors == 0) || total) exit(0);
		badprinted = 0;
		if (normal) printf("Summary of bad blocks\n");

		/* Print summary of bad blocks, possibly as shell script. */
		for (i = 0; i < errors; i++) {
			if (normal == 0 && badprinted == 0) {
				printf("badblocks %s ", name);
				badprinted = 1;
			}
			printf("%6ld ", rotten[i]);
			if ((i + 1) % 7 == 0) {
				printf("\n");
				badprinted = 0;
			}
		}
		printf("\n");
		exit(0);
	}
	if (normal && b % DIVISOR == 0) output(b);
  }
}

static void output(blocks_read)
long blocks_read;
{
  fprintf(stderr, "%8ld blocks read, %5ld errors\r", blocks_read, errors);
  fflush(stderr);
}
