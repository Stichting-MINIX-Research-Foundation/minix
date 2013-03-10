/*	$NetBSD: getopt.c,v 1.8 2006/07/09 21:39:48 wiz Exp $	*/

/*
 * This material, written by Henry Spencer, was released by him
 * into the public domain and is thus not subject to any copyright.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: getopt.c,v 1.8 2006/07/09 21:39:48 wiz Exp $");
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int c;
	int status = 0;

	optind = 2;	/* Past the program name and the option letters. */
	while ((c = getopt(argc, argv, argv[1])) != -1)
		switch (c) {
		case '?':
			status = 1;	/* getopt routine gave message */
			break;
		default:
			if (optarg != NULL)
				printf(" -%c %s", c, optarg);
			else
				printf(" -%c", c);
			break;
		}
	printf(" --");
	for (; optind < argc; optind++)
		printf(" %s", argv[optind]);
	printf("\n");
	exit(status);
}
