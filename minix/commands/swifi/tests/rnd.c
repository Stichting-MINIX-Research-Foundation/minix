/*
rnd.c

Generate random numbers
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *progname;

static void fatal(char *fmt, ...);
static void usage(void);

int main(int argc, char *argv[])
{
	int c, i, count;
	unsigned long n, v, high, modulus;
	unsigned seed;
	char *check;
	char *c_arg, *m_arg, *s_arg;

	(progname=strrchr(argv[0],'/')) ? progname++ : (progname=argv[0]);

	c_arg= m_arg= s_arg= NULL;
	while (c= getopt(argc, argv, "?c:m:s:"), c != -1)
	{
		switch(c)
		{
		case 'c': c_arg= optarg; break;
		case 'm': m_arg= optarg; break;
		case 's': s_arg= optarg; break;
		default:
			fatal("getopt failed: '%c'", c);
		}
	}
	if (optind != argc)
		usage();
	if (c_arg)
	{
		count= strtol(c_arg, &check, 0);
		if (check[0] != '\0')
			fatal("bad count '%s'", c_arg);
	}
	else
		count= 1;
	if (m_arg)
	{
		modulus= strtoul(m_arg, &check, 0);
		if (check[0] != '\0' || modulus == 0)
			fatal("bad modulus '%s'", m_arg);
		n= 0x80000000UL / modulus;
		if (n == 0)
			fatal("bad modulus %lu (too big)", modulus);
		high= n * modulus;
	}
	else
		modulus= high= 0x80000000UL;
	if (s_arg)
	{
		seed= strtol(s_arg, &check, 0);
		if (check[0] != '\0')
			fatal("bad seed '%s'", s_arg);
		srandom(seed);
	}

	for (i= 0; i<count; i++)
	{
		do
		{
			v= random();
		} while (v > high);

		printf("%lu\n", v % modulus);
	}
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
	fprintf(stderr, "Usage: rnd [-c <count>] [-m <modulus>] [-s <seed>]\n");
	exit(1);
}
