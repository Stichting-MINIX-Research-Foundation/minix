/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 3
 *      Module: fstime.c   SID: 3.5 5/15/91 19:30:19
 *
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *      Ben Smith, Rick Grehan or Tom Yager
 *      ben@bytepb.byte.com   rick_g@bytepb.byte.com   tyager@bytepb.byte.com
 *
 *******************************************************************************
 *  Modification Log:
 * $Header: fstime.c,v 3.4 87/06/22 14:23:05 kjmcdonell Beta $
 * 10/19/89 - rewrote timing calcs and added clock check (Ben Smith)
 * 10/26/90 - simplify timing, change defaults (Tom Yager)
 * 11/16/90 - added better error handling and changed output format (Ben Smith)
 * 11/17/90 - changed the whole thing around (Ben Smith)
 * 2/22/91 - change a few style elements and improved error handling (Ben Smith)
 * 4/17/91 - incorporated suggestions from Seckin Unlu (seckin@sumac.intel.com)
 * 4/17/91 - limited size of file, will rewind when reaches end of file
 * 7/95    - fixed mishandling of read() and write() return codes
 *           Carl Emilio Prelz <fluido@telepac.pt>
 * 12/95   - Massive changes.  Made sleep time proportional increase with run
 *           time; added fsbuffer and fsdisk variants; added partial counting
 *           of partial reads/writes (was *full* credit); added dual syncs.
 *           David C Niemi <niemi@tux.org>
 * 10/22/97 - code cleanup to remove ANSI C compiler warnings
 *           Andy Kahn <kahn@zk3.dec.com>
 *  9/24/07 - Separate out the read and write tests;
 *           output the actual time used in the results.
 *           Ian Smith <johantheghost at yahoo period com>
 ******************************************************************************/
char SCCSid[] = "@(#) @(#)fstime.c:3.5 -- 5/15/91 19:30:19";

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#define SECONDS 10

#define MAX_BUFSIZE 8192

/* This must be set to the smallest BUFSIZE or 1024, whichever is smaller */
#define COUNTSIZE 256
#define HALFCOUNT (COUNTSIZE/2)         /* Half of COUNTSIZE */

#define FNAME0  "dummy0"
#define FNAME1  "dummy1"

#ifndef MINIX
extern void sync(void);
#else
extern int sync(void);
#endif

int w_test(int timeSecs);
int r_test(int timeSecs);
int c_test(int timeSecs);

long read_score = 1, write_score = 1, copy_score = 1;

/****************** GLOBALS ***************************/

/* The buffer size for the tests. */
int bufsize = 1024;

/*
 * The max number of 1024-byte blocks in the file.
 * Don't limit it much, so that memory buffering
 * can be overcome.
 */
int max_blocks = 2000;

/* The max number of BUFSIZE blocks in the file. */
int max_buffs = 2000;

/* Countable units per 1024 bytes */
int count_per_k;

/* Countable units per bufsize */
int count_per_buf;

/* The actual buffer. */
/* char *buf = 0; */
/* Let's carry on using a static buffer for this, like older versions
 * of the code did.  It turns out that if you use a malloc buffer,
 * it goes 50% slower on reads, when using a 4k buffer -- at least on
 * my OpenSUSE 10.2 system.
 * What up wit dat?
 */
char buf[MAX_BUFSIZE];

int                     f;
int                     g;
int                     i;
void                    stop_count(int);
void                    clean_up(int);
int                     sigalarm = 0;

/******************** MAIN ****************************/

int main(int argc, char *argv[])
{
    /* The number of seconds to run for. */
    int                     seconds = SECONDS;

    /* The type of test to run. */
    char test = 'c';

    int status;
    int i;

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
                case 'c':
                case 'r':
                case 'w':
                    test = argv[i][1];
                    break;
                case 'b':
                    bufsize = atoi(argv[++i]);
                    break;
                case 'm':
                    max_blocks = atoi(argv[++i]);
                    break;
                case 't':
                    seconds = atoi(argv[++i]);
                    break;
                case 'd':
                    if (chdir(argv[++i]) < 0) {
                        perror("fstime: chdir");
                        exit(1);
                    }
                    break;
                default:
                    fprintf(stderr, "Usage: fstime [-c|-r|-w] [-b <bufsize>] [-m <max_blocks>] [-t <seconds>]\n");
                    exit(2);
            }
        } else {
            fprintf(stderr, "Usage: fstime [-c|-r|-w] [-b <bufsize>] [-m <max_blocks>] [-t <seconds>]\n");
            exit(2);
        }
    }

    if (bufsize < COUNTSIZE || bufsize > MAX_BUFSIZE) {
        fprintf(stderr, "fstime: buffer size must be in range %d-%d\n",
                COUNTSIZE, 1024*1024);
        exit(3);
    }
    if (max_blocks < 1 || max_blocks > 1024*1024) {
        fprintf(stderr, "fstime: max blocks must be in range %d-%d\n",
                1, 1024*1024);
        exit(3);
    }
    if (seconds < 1 || seconds > 3600) {
        fprintf(stderr, "fstime: time must be in range %d-%d seconds\n",
                1, 3600);
        exit(3);
    }

    max_buffs = max_blocks * 1024 / bufsize;
    count_per_k = 1024 / COUNTSIZE;
    count_per_buf = bufsize / COUNTSIZE;

    /*
    if ((buf = malloc(bufsize)) == 0) {
        fprintf(stderr, "fstime: failed to malloc %d bytes\n", bufsize);
        exit(4);
    }
    */

    if((f = creat(FNAME0, 0600)) == -1) {
            perror("fstime: creat");
            exit(1);
    }
    close(f);

    if((g = creat(FNAME1, 0600)) == -1) {
            perror("fstime: creat");
            exit(1);
    }
    close(g);

    if( (f = open(FNAME0, 2)) == -1) {
            perror("fstime: open");
            exit(1);
    }
    if( ( g = open(FNAME1, 2)) == -1 ) {
            perror("fstime: open");
            exit(1);
    }

    /* fill buffer */
    for (i=0; i < bufsize; ++i)
            buf[i] = i & 0xff;

    signal(SIGKILL,clean_up);

    /*
     * Run the selected test.
     * When I got here, this program ran full 30-second tests for
     * write, read, and copy, outputting the results for each.  BUT
     * only the copy results are actually used in the benchmark index.
     * With multiple iterations and three sets of FS tests, that amounted
     * to about 10 minutes of wasted time per run.
     *
     * So, I've made the test selectable.  Except that the read and write
     * passes are used to create the test file and calibrate the rates used
     * to tweak the results of the copy test.  So, for copy tests, we do
     * a few seconds of write and read to prime the pump.
     *
     * Note that this will also pull the file into the FS cache on any
     * modern system prior to the copy test.  Whether this is good or
     * bad is a matter of perspective, but it's how it was when I got
     * here.
     *
     * Ian Smith <johantheghost at yahoo period com> 21 Sep 2007
     */
    switch (test) {
    case 'w':
        status = w_test(seconds);
        break;
    case 'r':
        w_test(2);
        status = r_test(seconds);
        break;
    case 'c':
        w_test(2);
        r_test(2);
        status = c_test(seconds);
        break;
    default:
        fprintf(stderr, "fstime: unknown test \'%c\'\n", test);
        exit(6);
    }
    if (status) {
        clean_up(0);
        exit(1);
    }

    clean_up(0);
    exit(0);
}


static double getFloatTime(void)
{
        struct timeval t;

        gettimeofday(&t, 0);
        return (double) t.tv_sec + (double) t.tv_usec / 1000000.0;
}


/*
 * Run the write test for the time given in seconds.
 */
int w_test(int timeSecs)
{
        unsigned long counted = 0L;
        unsigned long tmp;
        long f_blocks;
        double start, end;
        extern int sigalarm;

        /* Sync and let it settle */
        sync();
        sleep(2);
        sync();
        sleep(2);

        /* Set an alarm. */
        sigalarm = 0;
        signal(SIGALRM, stop_count);
        alarm(timeSecs);

        start = getFloatTime();

        while (!sigalarm) {
                for(f_blocks=0; f_blocks < max_buffs; ++f_blocks) {
                        if ((tmp=write(f, buf, bufsize)) != bufsize) {
                                if (errno != EINTR) {
                                        perror("fstime: write");
                                        return(-1);
                                }
                                stop_count(0);
                                counted += ((tmp+HALFCOUNT)/COUNTSIZE);
                        } else
                                counted += count_per_buf;
                }
                lseek(f, 0L, 0); /* rewind */
        }

        /* stop clock */
        end = getFloatTime();
        write_score = (long) ((double) counted / ((end - start) * count_per_k));
        printf("Write done: %ld in %.4f, score %ld\n",
                            counted, end - start, write_score);

        /*
         * Output the test results. Use the true time.
         */
        fprintf(stderr, "COUNT|%ld|0|KBps\n", write_score);
        fprintf(stderr, "TIME|%.1f\n", end - start);

        return(0);
}

/*
 * Run the read test for the time given in seconds.
 */
int r_test(int timeSecs)
{
        unsigned long counted = 0L;
        unsigned long tmp;
        double start, end;
        extern int sigalarm;
        extern int errno;

        /* Sync and let it settle */
        sync();
        sleep(2);
        sync();
        sleep(2);

        /* rewind */
        errno = 0;
        lseek(f, 0L, 0);

        /* Set an alarm. */
        sigalarm = 0;
        signal(SIGALRM, stop_count);
        alarm(timeSecs);

        start = getFloatTime();

        while (!sigalarm) {
                /* read while checking for an error */
                if ((tmp=read(f, buf, bufsize)) != bufsize) {
                        switch(errno) {
                        case 0:
                        case EINVAL:
                                lseek(f, 0L, 0);  /* rewind at end of file */
                                counted += (tmp+HALFCOUNT)/COUNTSIZE;
                                continue;
                        case EINTR:
                                stop_count(0);
                                counted += (tmp+HALFCOUNT)/COUNTSIZE;
                                break;
                        default:
                                perror("fstime: read");
                                return(-1);
                                break;
                        }
                } else
                        counted += count_per_buf;
        }

        /* stop clock */
        end = getFloatTime();
        read_score = (long) ((double) counted / ((end - start) * count_per_k));
        printf("Read done: %ld in %.4f, score %ld\n",
                            counted, end - start, read_score);

        /*
         * Output the test results. Use the true time.
         */
        fprintf(stderr, "COUNT|%ld|0|KBps\n", read_score);
        fprintf(stderr, "TIME|%.1f\n", end - start);

        return(0);
}


/*
 * Run the copy test for the time given in seconds.
 */
int c_test(int timeSecs)
{
        unsigned long counted = 0L;
        unsigned long tmp;
        double start, end;
        extern int sigalarm;

        sync();
        sleep(2);
        sync();
        sleep(1);

        /* rewind */
        errno = 0;
        lseek(f, 0L, 0);

        /* Set an alarm. */
        sigalarm = 0;
        signal(SIGALRM, stop_count);
        alarm(timeSecs);

        start = getFloatTime();

        while (!sigalarm) {
                if ((tmp=read(f, buf, bufsize)) != bufsize) {
                        switch(errno) {
                        case 0:
                        case EINVAL:
                                lseek(f, 0L, 0);  /* rewind at end of file */
                                lseek(g, 0L, 0);  /* rewind the output too */
                                continue;
                        case EINTR:
                                /* part credit for leftover bytes read */
                                counted += ( (tmp * write_score) /
                                        (read_score + write_score)
                                        + HALFCOUNT) / COUNTSIZE;
                                stop_count(0);
                                break;
                        default:
                                perror("fstime: copy read");
                                return(-1);
                                break;
                        }
                } else  {
                        if ((tmp=write(g, buf, bufsize)) != bufsize) {
                                if (errno != EINTR) {
                                        perror("fstime: copy write");
                                        return(-1);
                                }
                                counted += (
                                 /* Full credit for part of buffer written */
                                        tmp +

                                 /* Plus part credit having read full buffer */
                                        ( ((bufsize - tmp) * write_score) /
                                        (read_score + write_score) )
                                        + HALFCOUNT) / COUNTSIZE;
                                stop_count(0);
                        } else
                                counted += count_per_buf;
                }
        }

        /* stop clock */
        end = getFloatTime();
        copy_score = (long) ((double) counted / ((end - start) * count_per_k));
        printf("Copy done: %ld in %.4f, score %ld\n",
                            counted, end - start, copy_score);

        /*
         * Output the test results. Use the true time.
         */
        fprintf(stderr, "COUNT|%ld|0|KBps\n", copy_score);
        fprintf(stderr, "TIME|%.1f\n", end - start);

        return(0);
}

void stop_count(int sig)
{
        extern int sigalarm;
        sigalarm = 1;
}

void clean_up(int sig)
{
        unlink(FNAME0);
        unlink(FNAME1);
}
