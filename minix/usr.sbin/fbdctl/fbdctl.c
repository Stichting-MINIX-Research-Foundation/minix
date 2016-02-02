/* fbdctl - FBD control tool - by D.C. van Moolenbroek */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <minix/u64.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define PATH_DEV_FBD	"/dev/fbd"

static void __dead
usage(void)
{

	fprintf(stderr, "usage:\n");
	fprintf(stderr, "  %s list\n", getprogname());
	fprintf(stderr, "  %s add [-a start[-end]] [-s skip] [-c count] [-rw] "
	    "<action> [params]\n", getprogname());
	fprintf(stderr, "  %s del N\n", getprogname());
	fprintf(stderr, "\n");
	fprintf(stderr, "actions and params:\n");
	fprintf(stderr, "  corrupt [zero|persist|random]\n");
	fprintf(stderr, "  error [OK|EIO]\n");
	fprintf(stderr, "  misdir <start>-<end> <align>\n");
	fprintf(stderr, "  lost\n");
	fprintf(stderr, "  torn <lead>\n");
	fprintf(stderr, "use %s -d <device> to specify a device other than "
	    "%s\n", getprogname(), PATH_DEV_FBD);

	exit(EXIT_FAILURE);
}

static void
print_rule(struct fbd_rule * rule)
{
	printf("%-2d %04lX%08lX-%04lX%08lX %-4d %-5d %c%c ",
	    rule->num, ex64hi(rule->start), ex64lo(rule->start),
	    ex64hi(rule->end), ex64lo(rule->end), rule->skip,
	    rule->count, (rule->flags & FBD_FLAG_READ) ? 'r' : ' ',
	    (rule->flags & FBD_FLAG_WRITE) ? 'w' : ' ');

	switch (rule->action) {
	case FBD_ACTION_CORRUPT:
		printf("%-7s ", "corrupt");
		switch (rule->params.corrupt.type) {
		case FBD_CORRUPT_ZERO: printf("zero"); break;
		case FBD_CORRUPT_PERSIST: printf("persist"); break;
		case FBD_CORRUPT_RANDOM: printf("random"); break;
		default: printf("<unknown>");
		}
		break;

	case FBD_ACTION_ERROR:
		printf("%-7s ", "error");

		switch (rule->params.error.code) {
		case 0:
			printf("OK");
			break;
		case EIO:
		case -EIO:
			printf("EIO");
			break;
		default:
			printf("%d", rule->params.error.code);
		}

		break;

	case FBD_ACTION_MISDIR:
		printf("%-7s %04lX%08lX-%04lX%08lX %u",
		    "misdir", ex64hi(rule->params.misdir.start),
		    ex64lo(rule->params.misdir.start),
		    ex64hi(rule->params.misdir.end),
		    ex64lo(rule->params.misdir.end),
		    rule->params.misdir.align);
		break;

	case FBD_ACTION_LOSTTORN:
		if (rule->params.losttorn.lead > 0)
			printf("%-7s %u", "torn", rule->params.losttorn.lead);
		else
			printf("%-7s", "lost");
	}

	printf("\n");
}

static int
do_list(int fd)
{
	struct fbd_rule rule;
	int i;

	printf("N  Start        End          Skip Count RW Action  Params\n");

	for (i = 1; ; i++) {
		rule.num = i;

		if (ioctl(fd, FBDCGETRULE, &rule) < 0) {
			if (errno == ENOENT)
				continue;
			break;
		}

		print_rule(&rule);
	}

	return EXIT_SUCCESS;
}

static int
scan_hex64(char * input, u64_t * val)
{
	u32_t lo, hi;
	char buf[9];
	int len;

	len = strlen(input);

	if (len < 1 || len > 16) return 0;

	if (len > 8) {
		memcpy(buf, input, len - 8);
		buf[len - 8] = 0;
		input += len - 8;

		hi = strtoul(buf, NULL, 16);
	}
	else hi = 0;

	lo = strtoul(input, NULL, 16);

	*val = make64(lo, hi);

	return 1;
}

static int
scan_range(char * input, u64_t * start, u64_t * end, int need_end)
{
	char *p;

	if ((p = strchr(input, '-')) != NULL) {
		*p++ = 0;

		if (!scan_hex64(p, end)) return 0;
	}
	else if (need_end) return 0;

	return scan_hex64(input, start);
}

static int
do_add(int fd, int argc, char ** argv, int off)
{
	struct fbd_rule rule;
	int c, r;

	memset(&rule, 0, sizeof(rule));

	while ((c = getopt(argc-off, argv+off, "a:s:c:rw")) != EOF) {
		switch (c) {
		case 'a':
			if (!scan_range(optarg, &rule.start, &rule.end, 0))
				usage();
			break;
		case 's':
			rule.skip = atoi(optarg);
			break;
		case 'c':
			rule.count = atoi(optarg);
			break;
		case 'r':
			rule.flags |= FBD_FLAG_READ;
			break;
		case 'w':
			rule.flags |= FBD_FLAG_WRITE;
			break;
		default:
			usage();
		}
	}

	optind += off; /* compensate for the shifted argc/argv */

	if (optind >= argc) usage();

	/* default to reads and writes */
	if (!rule.flags) rule.flags = FBD_FLAG_READ | FBD_FLAG_WRITE;

	if (!strcmp(argv[optind], "corrupt")) {
		if (optind+1 >= argc) usage();

		rule.action = FBD_ACTION_CORRUPT;

		if (!strcmp(argv[optind+1], "zero"))
			rule.params.corrupt.type = FBD_CORRUPT_ZERO;
		else if (!strcmp(argv[optind+1], "persist"))
			rule.params.corrupt.type = FBD_CORRUPT_PERSIST;
		else if (!strcmp(argv[optind+1], "random"))
			rule.params.corrupt.type = FBD_CORRUPT_RANDOM;
		else usage();
	}
	else if (!strcmp(argv[optind], "error")) {
		if (optind+1 >= argc) usage();

		rule.action = FBD_ACTION_ERROR;

		if (!strcmp(argv[optind+1], "OK"))
			rule.params.error.code = 0;
		else if (!strcmp(argv[optind+1], "EIO")) {
			if (EIO > 0)
				rule.params.error.code = -EIO;
			else
				rule.params.error.code = EIO;
		}
		else usage();
	}
	else if (!strcmp(argv[optind], "misdir")) {
		if (optind+2 >= argc) usage();

		rule.action = FBD_ACTION_MISDIR;

		if (!scan_range(argv[optind+1], &rule.params.misdir.start,
		    &rule.params.misdir.end, 1))
			usage();

		rule.params.misdir.align = atoi(argv[optind+2]);

		if ((int)rule.params.misdir.align <= 0)
			usage();
	}
	else if (!strcmp(argv[optind], "lost")) {
		rule.action = FBD_ACTION_LOSTTORN;

		rule.params.losttorn.lead = 0;
	}
	else if (!strcmp(argv[optind], "torn")) {
		if (optind+1 >= argc) usage();

		rule.action = FBD_ACTION_LOSTTORN;

		rule.params.losttorn.lead = atoi(argv[optind+1]);

		if ((int)rule.params.losttorn.lead <= 0)
			usage();
	}
	else usage();

#if DEBUG
	print_rule(&rule);
#endif

	r = ioctl(fd, FBDCADDRULE, &rule);

	if (r < 0) {
		perror("ioctl");

		return EXIT_FAILURE;
	}

	printf("Added rule %d\n", r);

	return EXIT_SUCCESS;
}

static int
do_del(int fd, int argc, char ** argv, int off)
{
	fbd_rulenum_t num;

	if (argc < off + 2)
		usage();

	num = atoi(argv[off + 1]);

	if (ioctl(fd, FBDCDELRULE, &num)) {
		perror("ioctl");

		return EXIT_FAILURE;
	}

	printf("Deleted rule %d\n", num);

	return EXIT_SUCCESS;
}

int
main(int argc, char ** argv)
{
	int r, fd, off = 1;
	const char *dev = PATH_DEV_FBD;

	setprogname(argv[0]);

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "-d")) {
		if (argc < 4)
			usage();

		dev = argv[2];

		off += 2;
	}

	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		perror(dev);

		return EXIT_FAILURE;
	}

	if (!strcmp(argv[off], "list"))
		r = do_list(fd);
	else if (!strcmp(argv[off], "add"))
		r = do_add(fd, argc, argv, off);
	else if (!strcmp(argv[off], "del"))
		r = do_del(fd, argc, argv, off);
	else
		usage();

	close(fd);

	return r;
}
