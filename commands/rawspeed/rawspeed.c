/*	rawspeed 1.15 - Measure speed of a device.	Author: Kees J. Bot
 *								26 Apr 1992
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#if !__minix
#include <sys/time.h>
#endif
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

#define SECTOR_SIZE		512
#define BLK_MAX_SECTORS		(sizeof(int) == 2 ? 32 : 64)
#define CHR_MAX_SECTORS		(sizeof(int) == 2 ? 63 : 512)
#define ABS_MAX_SECTORS		(INT_MAX / SECTOR_SIZE)

#define USEC	(!__minix || __minix_vmd)

/* Any good random number generator around? */
#if __minix_vmd || __linux
#define	rand	random
#define srand	srandom
#endif

#if __sun && __svr4__
#define rand	lrand48
#define srand	srand48
#endif

void report(const char *label)
{
	fprintf(stderr, "rawspeed: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

void usage(void)
{
	fprintf(stderr,
"Usage: rawspeed [-u unit] [-m max] [-t seconds] [-c] [-r limit] device\n");
	fprintf(stderr,
"       -u unit = best sector multiple (default 2)\n");
	fprintf(stderr,
"       -m max = read multiples of unit upto 'max' (default %d raw, %d file)\n",
					CHR_MAX_SECTORS, BLK_MAX_SECTORS);
	fprintf(stderr,
"       -t seconds = time to run test (default 10)\n");
	fprintf(stderr,
"       -c = cache test: rewind after each read or write of max size\n");
	fprintf(stderr,
"       -r limit = random seeks upto sector 'limit' before reading or writing\n");
	exit(1);
}

int done= 0;

void timeout(int sig)
{
	done= 1;
}

int main(int argc, char **argv)
{
	int i, fd, n= 0, unit= -1, max= -1, cache= 0;
	int size, seconds= 10;
	long tenthsec;
#if USEC
	struct timeval start_time, end_time;
	struct timezone dummy;
#else
	time_t start_time;
#endif
	off_t nbytes= 0, wbytes= 0, randlimit= 0;
	char *device, *chunk;
	struct stat st;
	off_t nseeks= 0;

	for (i= 1; i < argc && argv[i][0] == '-' && argv[i][1] != 0; i++) {
		char *opt;

		if (argv[i][1] == '-' && argv[i][2] == 0) { i++; break; }

		for (opt= argv[i]+1; *opt != 0; opt++) {
			switch (*opt) {
			case 'w':
				if (i == argc) usage();
				wbytes= atol(argv[++i]) * 1024;
				if (wbytes <= 0) usage();
				break;
			case 'm':
				if (i == argc) usage();
				max= atoi(argv[++i]);
				if (max <= 0 || max > ABS_MAX_SECTORS)
					usage();
				break;
			case 'u':
				if (i == argc) usage();
				unit= atoi(argv[++i]);
				if (unit <= 0 || unit > ABS_MAX_SECTORS)
					usage();
				break;
			case 't':
				if (i == argc) usage();
				seconds= atoi(argv[++i]);
				if (seconds <= 0) usage();
				break;
			case 'c':
				cache= 1;
				break;
			case 'r':
				if (i == argc) usage();
				randlimit= atol(argv[++i]);
				if (randlimit <= 0) usage();
				break;
			default:
				usage();
			}
		}
	}

	if (i != argc - 1) usage();

	if (strcmp(argv[i], "-") == 0) {
		fd= wbytes == 0 ? 0 : 1;
		device= "";
	} else {
		device= argv[i];
		if ((fd= open(device,
			wbytes == 0 ? O_RDONLY : O_WRONLY | O_CREAT, 0666)) < 0)
				fatal(device);
	}
	if (max < 0) {
		if (fstat(fd, &st) >= 0 && S_ISCHR(st.st_mode))
			max= CHR_MAX_SECTORS;
		else
			max= BLK_MAX_SECTORS;
	}

	if (unit < 0) unit= max > 1 ? 2 : 1;
	unit*= max / unit;
	if (unit == 0) usage();
	size= unit * SECTOR_SIZE;
	randlimit/= unit;

	if ((chunk= malloc((size_t) size)) == nil) {
		fprintf(stderr, "rawspeed: can't grab %d bytes: %s\n",
			size, strerror(errno));
		exit(1);
	}

	/* Touch the pages to get real memory sending other processes to swap.
	 */
	memset((void *) chunk, 0, (size_t) size);

	/* Clean the cache. */
	sync();

	signal(SIGALRM, timeout);
	signal(SIGINT, timeout);
#if USEC
	gettimeofday(&start_time, &dummy);
	if (randlimit != 0) srand((int) (start_time.tv_sec & INT_MAX));
#else
	start_time= time((time_t *) nil);
	if (randlimit != 0) srand((int) (start_time & INT_MAX));
#endif
	alarm(seconds);

	if (wbytes > 0) {
		while (!done && (n= write(fd, chunk, size)) > 0
						&& (nbytes+= n) < wbytes) {
			if (cache && lseek(fd, (off_t) 0, SEEK_SET) == -1)
				fatal(device);
			if (randlimit != 0) {
				if (lseek(fd, (off_t)
					(rand() % randlimit * size),
							SEEK_SET) == -1)
					fatal(device);
				nseeks++;
			}
		}
		sync();
	} else {
		while (!done && (n= read(fd, chunk, size)) > 0) {
			nbytes+= n;
			if (cache && lseek(fd, (off_t) 0, SEEK_SET) == -1)
				fatal(device);
			if (randlimit != 0) {
				if (lseek(fd, (off_t)
					(rand() % randlimit * size),
							SEEK_SET) == -1)
					fatal(device);
				nseeks++;
			}
		}
	}

#if USEC
	gettimeofday(&end_time, &dummy);
	tenthsec= (end_time.tv_sec - start_time.tv_sec) * 10
		+ (end_time.tv_usec - start_time.tv_usec) / 100000;
#else
	tenthsec= (time((time_t *) 0) - start_time) * 10;
#endif
	if (n < 0 && errno == EINTR) n= 0;
	if (n < 0) report(device);

	if (nbytes > 0) {
		off_t kBpts;

		fprintf(stderr, "%d kB / %ld.%ld s = ",
			(nbytes + 512) / 1024,
			tenthsec / 10, tenthsec % 10);
		if (tenthsec < 5)
			fprintf(stderr, "infinite\n");
		else {
			if (nbytes > LONG_MAX / 100) {
				seconds = (tenthsec + 5) / 10;
				kBpts= (nbytes + 512L * seconds)
							/ (1024L * seconds);
				fprintf(stderr, "%d kB/s\n", kBpts);
			} else {
				kBpts= (100 * nbytes + 512L * tenthsec)
							/ (1024L * tenthsec);
				fprintf(stderr, "%d.%d kB/s\n",
					kBpts/10, kBpts%10);
			}
		}
	}
	if (randlimit != 0 && tenthsec >= 5) {
		int rpm, disc= 0;
		off_t tenthms;

		tenthms= (tenthsec * 1000 + nseeks/2) / nseeks;

		fprintf(stderr,
			"%d seeks / %ld.%ld s = %ld seeks/s = %d.%d ms/seek\n",
			nseeks, tenthsec / 10, tenthsec % 10,
			(nseeks * 10 + tenthsec/2) / tenthsec,
			tenthms / 10, tenthms % 10);

		for (rpm= 3600; rpm <= 7200; rpm+= 1800) {
			int rotms = (10000L / 2 * 60 + rpm/2) / rpm;

			if (tenthms <= rotms) continue;

			if (!disc) {
				fprintf(stderr,
					"discarding av. rotational delay:\n  ");
				disc= 1;
			} else {
				fprintf(stderr, ", ");
			}
			fprintf(stderr, "%d.%d ms (%d rpm)",
				(tenthms - rotms) / 10, (tenthms - rotms) % 10,
				rpm);
		}
		if (disc) fputc('\n', stdout);
	}
	return n < 0 ? 1 : 0;
}
