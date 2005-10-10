/* $Header$ */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#define	WRBIT	0100000
#define	TXTBIT	040000
#define	EOFBIT	020000
#define	ELNBIT	010000
#define	WINDOW	04000
#define	MAGIC	0252

#define	PC_BUFLEN	1024

struct file {
	char	*ptr;
	unsigned flags;
	char	*fname;
	int	ufd;
	int	size;
	int	count;
	int	buflen;
	char	bufadr[PC_BUFLEN];
};
