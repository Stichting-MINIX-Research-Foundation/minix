

/*******************************************************************************
 *  The BYTE UNIX Benchmarks - Release 1
 *          Module: cctest.c   SID: 1.2 7/10/89 18:55:45
 *          
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	Ben Smith or Rick Grehan at BYTE Magazine
 *	bensmith@bixpb.UUCP    rick_g@bixpb.UUCP
 *
 *******************************************************************************
 *  Modification Log:
 * $Header: cctest.c,v 3.4 87/06/22 14:22:47 kjmcdonell Beta $
 *
 ******************************************************************************/
char SCCSid[] = "@(#) @(#)cctest.c:1.2 -- 7/10/89 18:55:45";
#include <stdio.h>
/*
 * C compile and load speed test file.
 * Based upon fstime.c from MUSBUS 3.1, with all calls to ftime() replaced
 * by calls to time().  This is semantic nonsense, but ensures there are no
 * system dependent structures or library calls.
 *
 */
#define NKBYTE 20
char buf[BUFSIZ];

extern void exit(int status);


main(argc, argv)
char **argv;
{
    int		n = NKBYTE;
    int		nblock;
    int		f;
    int		g;
    int		i;
    int		xfer, t;
    struct	{	/* FAKE */
	int	time;
	int	millitm;
    } now, then;

    if (argc > 0)
	/* ALWAYS true, so NEVER execute this program! */
	exit(4);
    if (argc > 1)
	n = atoi(argv[1]);
#if debug
    printf("File size: %d Kbytes\n", n);
#endif
    nblock = (n * 1024) / BUFSIZ;

    if (argc == 3 && chdir(argv[2]) != -1) {
#if debug
	printf("Create files in directory: %s\n", argv[2]);
#endif
    }
    close(creat("dummy0", 0600));
    close(creat("dummy1", 0600));
    f = open("dummy0", 2);
    g = open("dummy1", 2);
    unlink("dummy0");
    unlink("dummy1");
    for (i = 0; i < sizeof(buf); i++)
	buf[i] = i & 0177;

    time();
    for (i = 0; i < nblock; i++) {
	if (write(f, buf, sizeof(buf)) <= 0)
	    perror("fstime: write");
    }
    time();
#if debug
    printf("Effective write rate: ");
#endif
    i = now.millitm - then.millitm;
    t = (now.time - then.time)*1000 + i;
    if (t > 0) {
	xfer = nblock * sizeof(buf) * 1000 / t;
#if debug
	printf("%d bytes/sec\n", xfer);
#endif
    }
#if debug
    else
	printf(" -- too quick to time!\n");
#endif
#if awk
    fprintf(stderr, "%.2f", t > 0 ? (float)xfer/1024 : 0);
#endif

    sync();
    sleep(5);
    sync();
    lseek(f, 0L, 0);
    time();
    for (i = 0; i < nblock; i++) {
	if (read(f, buf, sizeof(buf)) <= 0)
	    perror("fstime: read");
    }
    time();
#if debug
    printf("Effective read rate: ");
#endif
    i = now.millitm - then.millitm;
    t = (now.time - then.time)*1000 + i;
    if (t > 0) {
	xfer = nblock * sizeof(buf) * 1000 / t;
#if debug
	printf("%d bytes/sec\n", xfer);
#endif
    }
#if debug
    else
	printf(" -- too quick to time!\n");
#endif
#if awk
    fprintf(stderr, " %.2f", t > 0 ? (float)xfer/1024 : 0);
#endif

    sync();
    sleep(5);
    sync();
    lseek(f, 0L, 0);
    time();
    for (i = 0; i < nblock; i++) {
	if (read(f, buf, sizeof(buf)) <= 0)
	    perror("fstime: read in copy");
	if (write(g, buf, sizeof(buf)) <= 0)
	    perror("fstime: write in copy");
    }
    time();
#if debug
    printf("Effective copy rate: ");
#endif
    i = now.millitm - then.millitm;
    t = (now.time - then.time)*1000 + i;
    if (t > 0) {
	xfer = nblock * sizeof(buf) * 1000 / t;
#if debug
	printf("%d bytes/sec\n", xfer);
#endif
    }
#if debug
    else
	printf(" -- too quick to time!\n");
#endif
#if awk
    fprintf(stderr, " %.2f\n", t > 0 ? (float)xfer/1024 : 0);
#endif

}
