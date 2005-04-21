/* stat.c Feb 1987 - main, printit, statit
 *
 * stat - a program to perform what the stat(2) call does.  
 *
 * usage: stat [-] [-all] -<field> [-<field> ...] [file1 file2 file3 ...]
 *
 * where   <field> is one of the struct stat fields without the leading "st_".
 *	   The three times can be printed out as human times by requesting
 *	   -Ctime instead of -ctime (upper case 1st letter).
 *	   - means take the file names from stdin.
 *	   -0.. means fd0..
 *	   no files means all fds.
 *
 * output: if only one field is specified, that fields' contents are printed.
 *         if more than one field is specified, the output is
 *	   file	filed1: f1val, field2: f2val, etc
 *
 * written: Larry McVoy, (mcvoy@rsch.wisc.edu)  
 */

# define	ALLDEF		/* Make -all default. (kjb) */

# include	<sys/types.h>
# include	<errno.h>
# include	<limits.h>
# include	<stdio.h>
# include	<stdlib.h>
# include 	<string.h>
# include	<time.h>
# include	<sys/stat.h>
# define	addr(x)		((void*) &sbuf.x)
# define	size(x)		sizeof(sbuf.x)
# define	equal(s, t)	(strcmp(s, t) == 0)
# ifndef PATH_MAX
#  define	PATH_MAX	1024
# endif
# undef		LS_ADDS_SPACE	/* AT&T Unix PC, ls prints "file[* /]" */
				/* This makes stat fail. */

# ifndef _MINIX			/* All but Minix have u_* and st_blocks */
#  define BSD
# endif

# ifndef BSD
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
# endif

# ifndef S_IREAD
#  define S_IREAD	S_IRUSR
#  define S_IWRITE	S_IWUSR
#  define S_IEXEC	S_IXUSR
# endif

char *      arg0;
struct stat sbuf;
extern int  errno;
int	    first_file= 1;
#ifndef S_IFLNK
#define lstat	stat
#endif

struct field {
    char* f_name;	/* field name in stat */
    u_char* f_addr;	/* address of the field in sbuf */
    u_short f_size;	/* size of the object, needed for pointer arith */
    u_short f_print;	/* show this field? */
} fields[] = {
    { "dev",		addr(st_dev),		size(st_dev),		0 },
    { "ino",		addr(st_ino),		size(st_ino),		0 },
    { "mode",		addr(st_mode),		size(st_mode),		0 },
    { "nlink",		addr(st_nlink),		size(st_nlink),		0 },
    { "uid",		addr(st_uid),		size(st_uid),		0 },
    { "gid",		addr(st_gid),		size(st_gid),		0 },
    { "rdev",		addr(st_rdev),		size(st_rdev),		0 },
    { "size",		addr(st_size),		size(st_size),		0 },
    { "Atime",		addr(st_atime),		size(st_atime),		0 },
    { "atime",		addr(st_atime),		size(st_atime),		0 },
    { "Mtime",		addr(st_mtime),		size(st_mtime),		0 },
    { "mtime",		addr(st_mtime),		size(st_mtime),		0 },
    { "Ctime",		addr(st_ctime),		size(st_ctime),		0 },
    { "ctime",		addr(st_ctime),		size(st_ctime),		0 },
# ifdef BSD
    { "blksize", 	addr(st_blksize),	size(st_blksize),	0 },
    { "blocks",		addr(st_blocks),	size(st_blocks),	0 },
# endif
    { NULL,		0,			0,			0 },
};
    
void printstat(struct stat *sbuf, int nprint);
void printit(struct stat* sb, struct field* f, int n);
void rwx(mode_t mode, char *bit);
void usage(void);

int main(int ac, char** av)
{
    int      i, j, nprint = 0, files = 0;
    char     buf[PATH_MAX], *check;
    int      sym=0, ret=0, from_stdin = 0;
    int      err;
    u_long   fd;

    if ((arg0 = strrchr(av[0], '/')) == NULL) arg0 = av[0]; else arg0++;
#ifdef S_IFLNK
    if (equal(arg0, "lstat")) sym = 1;
#endif

    if (ac > 1 && equal(av[i = 1], "-"))
	i++, from_stdin++;

    for (i= 1; i<ac; i++)  {
	if (av[i][0] == '-')  {
	    if (equal(av[i], "-")) {
		from_stdin= 1;
		files++;
		continue;
	    }
	    if (equal("-all", av[i])) {
		for (j=0; fields[j].f_name; j++)
		    nprint++, fields[j].f_print++;
		continue;
	    }
	    if (equal("-s", av[i])) {
#ifdef S_IFLNK
		sym=1;
#endif
		continue;
	    }
	    fd = strtoul(av[i]+1, &check, 0);
	    if (check != av[i]+1 && *check == '\0')
	    {
		files++;
		continue;
	    }
	    for (j=0; fields[j].f_name; j++) 
		if (equal(fields[j].f_name, &av[i][1])) {
		    nprint++, fields[j].f_print++;
		    break;
		}
	    if (!fields[j].f_name) {
		if (!equal("-?", av[i])) {
		    fprintf(stderr, "stat: %s: bad field\n", av[i]);
		}
		usage();
	    }
	}
	else 
	    files++;
    }
    if (!nprint) {
# ifndef ALLDEF
	usage();
# else
	for (j=0; fields[j].f_name; j++)
	    nprint++, fields[j].f_print++;
# endif
    }

    if (from_stdin)
	files++;	/* We don't know how many files come from stdin. */

    if (files == 0) {	/* Stat all file descriptors. */
	for (i= 0; i<OPEN_MAX; i++) {
	    err= fstat(i, &sbuf);
	    if (err == -1 && errno == EBADF)
		continue;
	    if (err == 0) {
		if (!first_file) fputc('\n', stdout);
		printf("fd %d:\n", i);
		printstat(&sbuf, nprint);
	    }
	    else {
		fprintf(stderr, "%s: fd %d: %s\n", arg0, i, strerror(errno));
		ret= 1;
	    }
	}
	exit(ret);
    }
		
    for (i=1; i<ac; i++) {
	if (equal(av[i], "-")) {
	    while (fgets(buf, sizeof(buf), stdin)) {
	    	char *p= strchr(buf, '\n');
	    	if (p) *p= 0;
		if (!sym) err= stat(av[i], &sbuf);
		if (sym || (err != 0 && errno == ENOENT)) {
		    err= lstat(av[i], &sbuf);
		}
		if (err == -1) {
		    fprintf(stderr, "%s: %s: %s\n",
			arg0, av[i], strerror(errno));
		    ret= 1;
		}
		else {
		    if (!first_file) fputc('\n', stdout);
		    printf("%s:\n", buf);
		    printstat(&sbuf, nprint);
		}
	    }
	    continue;
	}
	if (av[i][0] == '-') {
	    fd= strtoul(av[i]+1, &check, 10);
	    if (check == av[i]+1 || *check != '\0') continue;
	    if (fd >= INT_MAX) {
		err= -1;
		errno= EBADF;
	    }
	    else {
		err= fstat((int) fd, &sbuf);
	    }
	    if (err != -1) {
		if (!first_file) fputc('\n', stdout);
		if (files != 1) printf("fd %lu:\n", fd);
		printstat(&sbuf, nprint);
	    }
	    else {
		fprintf(stderr, "fd %lu: %s\n", fd, strerror(errno));
		ret= 1;
	    }
	    continue;
	}
	if (!sym) err= stat(av[i], &sbuf);
	if (sym || (err != 0 && errno == ENOENT)) err= lstat(av[i], &sbuf);
	if (err != -1) {
	    if (!first_file) fputc('\n', stdout);
	    if (files != 1) printf("%s:\n", av[i]);
	    printstat(&sbuf, nprint);
	}
	else {
	    fprintf(stderr, "%s: %s: %s\n", arg0, av[i], strerror(errno));
	    ret= 1;
	}
    }
    exit(ret);
}

/*------------------------------------------------------------------30/Jan/87-*
 * printstat(file, nprint) - do the work
 *----------------------------------------------------------------larry mcvoy-*/
void printstat(struct stat *sbuf, int nprint)
{
    int      j;
    int      first_field= 1;

    for (j=0; fields[j].f_name; j++) {
	if (fields[j].f_print) {
	    if (!first_field) fputc('\n', stdout);
	    printit(sbuf, &fields[j], nprint);
	    first_field= 0;
	}
    }
    fputc('\n', stdout);
    first_file= 0;
}

/*------------------------------------------------------------------30/Jan/87-*
 * printit(sb, f, n) - print the field
 *
 * Inputs    -> (struct stat*), (struct field*), (int)
 *
 * Results   -> Displays the field, with special handling of weird fields like
 *		mode and mtime.  The mode field is dumped in octal, followed
 *		by one or more of the S_IF<X> and/or S_I<X> values.
 *----------------------------------------------------------------larry mcvoy-*/
void printit(struct stat* sb, struct field* f, int n)
{
    if (n > 1)
	printf("%s: ", f->f_name);
    if (equal(f->f_name, "mode")) {
		/* This lot changed to my personal liking. (kjb) */
	char bit[11];

	printf("%07lo, ", (u_long) sb->st_mode);

	strcpy(bit, "----------");

	switch (sb->st_mode&S_IFMT) {
	case S_IFDIR:	bit[0]='d';	break;
# ifdef S_IFFIFO
	case S_IFFIFO:	bit[0]='p';	break;
# endif
	case S_IFCHR:	bit[0]='c';	break;
	case S_IFBLK:	bit[0]='b';	break;
# ifdef S_IFSOCK
	case S_IFSOCK:	bit[0]='S';	break;
# endif
# ifdef S_IFMPC
	case S_IFMPC:	bit[0]='C';	break;
# endif
# ifdef S_IFMPB
	case S_IFMPB:	bit[0]='B';	break;
# endif
# ifdef S_IFLNK
	case S_IFLNK:	bit[0]='l';	break;
# endif
	}
	rwx(sb->st_mode, bit+1);
	rwx(sb->st_mode<<3, bit+4);
	rwx(sb->st_mode<<6, bit+7);
	if (sb->st_mode&S_ISUID)	bit[3]='s';
	if (sb->st_mode&S_ISGID)	bit[6]='s';
	if (sb->st_mode&S_ISVTX)	bit[9]='t';
	printf("\"%s\"", bit);
    }
    /* times in human form, uppercase first letter */
    else if (equal("Ctime", f->f_name)) {
	printf("%.24s (%lu)", ctime(&sb->st_ctime), (u_long) sb->st_ctime);
	f[1].f_print= 0;
    }
    else if (equal("Mtime", f->f_name)) {
	printf("%.24s (%lu)", ctime(&sb->st_mtime), (u_long) sb->st_mtime);
	f[1].f_print= 0;
    }
    else if (equal("Atime", f->f_name)) {
	printf("%.24s (%lu)", ctime(&sb->st_atime), (u_long) sb->st_atime);
	f[1].f_print= 0;
    }
    else if (equal("ctime", f->f_name)) {
	printf("%lu", (u_long) sb->st_ctime);
    }
    else if (equal("mtime", f->f_name)) {
	printf("%lu", (u_long) sb->st_mtime);
    }
    else if (equal("atime", f->f_name)) {
	printf("%lu", (u_long) sb->st_atime);
    }
    else {
	switch (f->f_size) {
	case sizeof(char):
	    printf("%d", * (u_char *) f->f_addr);
	    break;
	case sizeof(short):
	    printf("%u", (u_int) * (u_short *) f->f_addr);
	    break;
#if INT_MAX != SHRT_MAX
	case sizeof(int):
	    printf("%u", * (u_int *) f->f_addr);
	    break;
#endif
#if LONG_MAX != INT_MAX && LONG_MAX != SHRT_MAX
	case sizeof(long):
	    printf("%lu", * (u_long *) f->f_addr);
	    break;
#endif
	default:
	    fprintf(stderr, "\nProgram error: bad '%s' field size %d\n", 
			    f->f_name, f->f_size);
	    break;
	}
    }
}

void rwx(mode_t mode, char *bit)
{
	if (mode&S_IREAD)	bit[0]='r';
	if (mode&S_IWRITE)	bit[1]='w';
	if (mode&S_IEXEC)	bit[2]='x';
}

void usage(void)
{
    fprintf(stderr,
	"Usage: %s [-] [-fd] [-all] [-s] [-field ...] [file1 ...]\n", 
	arg0);
    exit(1);
}
