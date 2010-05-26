/*
bintoc.c

Convert a (binary) file to a series of comma separated hex values suitable
for initializing a character array in C.
*/

#define _POSIX_C_SOURCE 2

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *progname;
static unsigned char buf[1024];

static void fatal(char *fmt, ...);
static void usage(void);

int main(int argc, char *argv[])
{
	int c, i, r, first;
	FILE *file_in, *file_out;
	char *in_name;
	char *o_arg;

	(progname=strrchr(argv[0],'/')) ? progname++ : (progname=argv[0]);

	o_arg= NULL;
	while (c= getopt(argc, argv, "?o:"), c != -1)
	{
		switch(c)
		{
		case '?': usage();
		case 'o': o_arg= optarg; break;
		default:  fatal("getopt failed: '%c'\n", c);
		}
	}

	if (o_arg)
	{
		file_out= fopen(o_arg, "w");
		if (file_out == NULL)
		{
			fatal("unable to create '%s': %s\n",
				o_arg, strerror(errno));
			exit(1);
		}
	}
	else
		file_out= stdout;

	if (optind < argc)
	{
		in_name= argv[optind];
		optind++;
		file_in= fopen(in_name, "r");
		if (file_in == NULL)
		{
			fatal("unable to open '%s': %s",
				in_name, strerror(errno));
		}
	}
	else
	{
		in_name= "(stdin)";
		file_in= stdin;
	}

	if (optind != argc)
		usage();

	first= 1;
	for (;;)
	{
		r= fread(buf, 1, sizeof(buf), file_in);
		if (r == 0)
			break;
		for (i= 0; i<r; i++)
		{
			if ((i % 8) == 0)
			{
				if (first)
				{
					fprintf(file_out, "\t");
					first= 0;
				}
				else
					fprintf(file_out, ",\n\t");
			}
			else
				fprintf(file_out, ", ");
			fprintf(file_out, "0x%02x", buf[i]);
		}
	}

	if (ferror(file_in))
	{
		fatal("read error on %s: %s\n",
			in_name, strerror(errno));
	}
	fprintf(file_out, "\n");

	exit(0);
}

static void fatal(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", progname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	exit(1);
}

static void usage(void)
{
	fprintf(stderr, "Usage: bintoc [-o <out-file>] [<in-file>]\n");
	exit(1);
}
