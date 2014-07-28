/* Extracted from sys/arch/i386/stand/lib/bootmenu.c */
int atoi(const char *);

#define isnum(c) ((c) >= '0' && (c) <= '9')

int
atoi(const char *in)
{
	const char *c;
	int ret;

	ret = 0;
	c = in;
	if (*c == '-')
		c++;
	for (; isnum(*c); c++)
		ret = (ret * 10) + (*c - '0');

	return (*in == '-') ? -ret : ret;
}

