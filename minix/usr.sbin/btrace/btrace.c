/* Block trace command line tool - by D.C. van Moolenbroek */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <minix/btrace.h>
#include <minix/u64.h>
#include <sys/ioctl.h>

static btrace_entry buf[BTBUF_SIZE];

static void __dead
usage(void)
{
	fprintf(stderr, "usage:\n"
	    "%s start <device> <nr_entries>\n"
	    "%s stop <device> <file>\n"
	    "%s reset <device>\n"
	    "%s dump <file>\n",
	    getprogname(), getprogname(), getprogname(), getprogname());

	exit(EXIT_FAILURE);
}

static void
btrace_start(char * device, int nr_entries)
{
	int r, ctl, devfd;
	size_t size;

	if ((devfd = open(device, O_RDONLY)) < 0) {
		perror("device open");
		exit(EXIT_FAILURE);
	}

	size = nr_entries;
	if ((r = ioctl(devfd, BIOCTRACEBUF, &size)) < 0) {
		perror("ioctl(BIOCTRACEBUF)");
		exit(EXIT_FAILURE);
	}

	ctl = BTCTL_START;
	if ((r = ioctl(devfd, BIOCTRACECTL, &ctl)) < 0) {
		perror("ioctl(BIOCTRACECTL)");

		size = 0;
		(void)ioctl(devfd, BIOCTRACEBUF, &size);

		exit(EXIT_FAILURE);
	}

	close(devfd);
}

static void
btrace_stop(char * device, char * file)
{
	int r, ctl, devfd, outfd;
	size_t size;

	if ((devfd = open(device, O_RDONLY)) < 0) {
		perror("device open");
		exit(EXIT_FAILURE);
	}

	if ((outfd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) {
		perror("file open");
		exit(EXIT_FAILURE);
	  }

	ctl = BTCTL_STOP;
	if ((r = ioctl(devfd, BIOCTRACECTL, &ctl)) < 0) {
		perror("ioctl(BIOCTRACECTL)");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		if ((r = ioctl(devfd, BIOCTRACEGET, buf)) < 0) {
			perror("ioctl(BIOCTRACEGET)");
			break;
		}

		if (r == 0) break;

		size = r * sizeof(buf[0]);
		if ((r = write(outfd, (char *)buf, size)) != size) {
			if (r < 0) perror("write");
			else fputs("short write\n", stderr);
		}
	}

	close(outfd);

	size = 0;
	if ((r = ioctl(devfd, BIOCTRACEBUF, &size)) < 0) {
		perror("ioctl(BIOCTRACEBUF)");
		exit(EXIT_FAILURE);
	}

	close(devfd);
}

static void
btrace_reset(char * device)
{
	size_t size;
	int r, ctl, devfd;

	if ((devfd = open(device, O_RDONLY)) < 0) {
		perror("device open");
		exit(EXIT_FAILURE);
	}

	ctl = BTCTL_STOP;
	(void)ioctl(devfd, BIOCTRACECTL, &ctl);

	size = 0;
	if ((r = ioctl(devfd, BIOCTRACEBUF, &size)) < 0) {
		perror("ioctl(BIOCTRACEBUF)");
		exit(EXIT_FAILURE);
	}

	close(devfd);
}

static void
dump_entry(btrace_entry * entry)
{
	switch (entry->request) {
	case BTREQ_OPEN: printf("OPEN"); break;
	case BTREQ_CLOSE: printf("CLOSE"); break;
	case BTREQ_READ: printf("READ"); break;
	case BTREQ_WRITE: printf("WRITE"); break;
	case BTREQ_GATHER: printf("GATHER"); break;
	case BTREQ_SCATTER: printf("SCATTER"); break;
	case BTREQ_IOCTL: printf("IOCTL"); break;
	}

	printf(" request\n");

	switch (entry->request) {
	case BTREQ_OPEN:
		printf("- access:\t%x\n", entry->size);
		break;
	case BTREQ_READ:
	case BTREQ_WRITE:
	case BTREQ_GATHER:
	case BTREQ_SCATTER:
		printf("- position:\t%08lx%08lx\n",
		    ex64hi(entry->position), ex64lo(entry->position));
		printf("- size:\t\t%u\n", entry->size);
		printf("- flags:\t%x\n", entry->flags);
		break;
	case BTREQ_IOCTL:
		printf("- request:\t%08x\n", entry->size);
		break;
	}

	printf("- start:\t%u us\n", entry->start_time);
	printf("- finish:\t%u us\n", entry->finish_time);
	if (entry->result == BTRES_INPROGRESS)
		printf("- result:\t(in progress)\n");
	else
		printf("- result:\t%d\n", entry->result);
	printf("\n");
}

static void
btrace_dump(char * file)
{
	int i, r, infd;

	if ((infd = open(file, O_RDONLY)) < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		if ((r = read(infd, (char *)buf, sizeof(buf))) <= 0)
			break;

		r /= sizeof(buf[0]);

		for (i = 0; i < r; i++)
			dump_entry(&buf[i]);
	}

	if (r < 0) perror("read");

	close(infd);
}

int main(int argc, char ** argv)
{
	int num;

	setprogname(argv[0]);

	if (argc < 3) usage();

	if (!strcmp(argv[1], "start")) {
		if (argc < 4) usage();

		num = atoi(argv[3]);

		if (num <= 0) usage();

		btrace_start(argv[2], num);

	} else if (!strcmp(argv[1], "stop")) {
		if (argc < 4) usage();

		btrace_stop(argv[2], argv[3]);

	} else if (!strcmp(argv[1], "reset")) {
		btrace_reset(argv[2]);

	} else if (!strcmp(argv[1], "dump")) {
		btrace_dump(argv[2]);

	} else
		usage();

	return EXIT_SUCCESS;
}
