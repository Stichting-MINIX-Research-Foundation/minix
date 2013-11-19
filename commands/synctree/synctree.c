/*	synctree 4.16 - Synchronise file tree.		Author: Kees J. Bot
 *								5 Apr 1989
 * SYNOPSYS
 *	synctree [-iuf] [[user1@machine1:]dir1 [[user2@]machine2:]dir2
 *
 * Dir2 will then be synchronized to dir1 with respect to the flags.
 * The flags mean:
 *	-i  Be interactive on files other than directories too.
 *	-u  Only install files that are newer, i.e. that need an update.
 *	-f  Force.  Don't ask for confirmation, all answers are 'yes'.
 *
 * Hitting return lets synctree use its proposed answer.  Hitting CTRL-D is
 * like typing return to all questions that follow.
 *
 * If either of the directories to be synchronized contains the file ".backup"
 * then it is a backup directory.  The file ".backup" in this directory is
 * an array of mode information indexed on inode number.
 *
 * 89/04/05, Kees J. Bot - Birth of tree synchronizing program.
 * 92/02/02		 - General overhaul, rcp(1) like syntax.
 */

#define nil 0
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <utime.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#define USE_SHADOWING 0

#ifndef PATH_MAX

#define PATH_MAX	1024
#endif

#ifndef S_ISLNK
/* There were no symlinks in medieval times. */
#define S_ISLNK(mode)			(0)
#define lstat				stat
#define symlink(path1, path2)		(errno= ENOSYS, -1)
#define readlink(path, buf, len)	(errno= ENOSYS, -1)
#endif

#define NUMBYTES     4	/* Any number fits in this many bytes. */
#define CHUNK     4096	/* Transfer files in this size chunks. */

static int install= 0;	/* Install files, do not delete, update if newer. */
static int interact= 0;	/* Ask permission to install too. */
static int force= 0;	/* Force trees to be completely equal. */
static int backup= 0;	/* This tree is for backup. */

static char SYNCNAME[]	= "synctree";
static char SLAVENAME[]	= "==SLAVE==";
static char MASTERNAME[]= "==MASTER==";


static char BACKUP[] = ".backup";	/* Backup filemodes. */
static int filemodes;			/* Filemodes fildes. */

static int chan[2]= { 0, 1 };	/* In and output channel to opposite side. */

#define BUCKSIZE (1+NUMBYTES+CHUNK)
static char bucket[BUCKSIZE];	/* Put a lot of things here before sending. */
static char *buckp= bucket;	/* Fill pointer. */
static int buckn= 0;		/* # bytes in bucket. */

enum orders {	/* What back breaking labour should the slave perform? */
	ENTER= 128,	/* Make ready to process contents of directory. */
	ADVANCE,	/* Determine next pathname and report it back. */
	CAT,		/* Send contents of file. */
	MORE,		/* Send more file contents. */
	ORDER_CANCEL,		/* Current pathname is not installed, remove as link. */
	DIE,		/* Die with exit(0); */
	DIE_BAD,	/* exit(1); */
	POSITIVE,	/* Ask a yes/no question expecting yes. */
	NEGATIVE,	/* Same expecting no. */
	PASS_YES,	/* Pass this to the master will you. */
	PASS_NO		/* Same here. */
};

#ifdef DEBUG
char *ORDERS[]= {
	"ENTER", "ADVANCE", "CAT", "MORE", "CANCEL", "DIE", "DIE_BAD",
	"POSITIVE", "NEGATIVE", "PASS_YES", "PASS_NO"
};
#endif

enum answers {
	PATH= 128,	/* Report pathname, and stat(2) info. */
	LINK,		/* Report linkname, pathname, and stat(2) info. */
	DATA,		/* Contents of file follow. */
	NODATA,		/* Can't read file. */
	DONE,		/* Nothing more to advance to. */
	SYMLINK,	/* Report symlinkname, pathname, and stat(2) info. */
	YES, NO		/* Result of an ASK. */
};

#ifdef DEBUG
char *ANSWERS[]= {
	"PATH", "LINK", "DATA", "NODATA", "DONE", "SYMLINK", "YES", "NO"
};

#define DPRINTF(format, arg)	fprintf(stderr, format, arg0, arg)
#else
#define DPRINTF(format, arg)
#endif

struct mode {
	unsigned short	md_mode;
	unsigned short	md_uid;
	unsigned short	md_gid;
	unsigned short	md_rdev;
	unsigned short	md_devsiz;
};

static char *arg0;	/* basename(argv[0]) */
static int ex= 0;	/* exit status. */

static void because()
{
	fprintf(stderr, ": %s\n", strerror(errno));
	ex= 1;
}

static void perr(char *label)
{
	fprintf(stderr, "%s: %s: %s\n", arg0, label, strerror(errno));
	ex= 1;
}

static void perrx(char *label)
{
	perr(label);
	exit(1);
}

#if S_HIDDEN
/* Support for per achitecture hidden files. */
static int transparent= 0;

static void isvisible(char *name)
{
	char *p= name + strlen(name);

	while (p > name && *--p == '/') {}

	if (p > name && *p == '@' && p[-1] != '/') transparent= 1;
}
#else
#define transparent	0
#define isvisible(name)	((void) 0)
#endif

static void isbackup(int slave)
{
	if ((filemodes= open(BACKUP, slave ? O_RDONLY : O_RDWR)) >= 0)
		backup= 1;
	else {
		if (errno != ENOENT) perrx(BACKUP);
	}
}

static char path[PATH_MAX+1];	/* Holds pathname of file being worked on. */
static char lnkpth[PATH_MAX+1];	/* (Sym)link to path. */
static char *linkpath;		/* What path is, or should be linked to. */
static struct stat st;		/* Corresponding stat(2) info. */
static char Spath[PATH_MAX+1];	/* Slave is looking at this. */
static char Slnkpth[PATH_MAX+1];/* (Sym)link to Spath. */
static char *Slinkpath=nil;	/* Either nil or Slnkpth. */
static struct stat Sst;		/* Slave's stat(2). */

static char *addpath(char *p, char *n)
/* Add a name to the path, return pointer to end. */
{
	if (p - path + 1 + strlen(n) > PATH_MAX) {
		*p= 0;
		fprintf(stderr, "%s: %s/%s is too long.\n", arg0, path, n);
		fprintf(stderr, "%s: Unable to continue.\n", arg0);
		exit(1);
	}
	if (p == path+1 && path[0] == '.') p= path;

	if (p > path) *p++ = '/';

	while (*n != 0) *p++ = *n++;
	*p= 0;
	return p;
}

struct entry {	/* A directory entry. */
	struct entry	*next;	/* Next entry in same directory */
	struct entry	*dir;	/* It is part of this directory */
	struct entry	*con;	/* If a dir, its contents */
	char		*name;	/* Name of this dir entry */
};

static struct entry *E= nil;		/* File being processed. */

static void setpath(struct entry *e)
/* Set path leading to e. */
{
	static char *pend;

	if (e == nil)
		pend= path;
	else {
		setpath(e->dir);
		pend= addpath(pend, e->name);
	}
}

static void sort(struct entry **ae)
/* This is either a stable mergesort, or thermal noise, I'm no longer sure.
 * It must be called like this: if (L!=nil && L->next!=nil) sort(&L);
 */
{
	/* static */ struct entry *e1, **mid;  /* Need not be local */
	struct entry *e2;

	e1= *(mid= &(*ae)->next);
	do {
		if ((e1= e1->next) == nil) break;
		mid= &(*mid)->next;
	} while ((e1= e1->next) != nil);

	e2= *mid;
	*mid= nil;

	if ((*ae)->next != nil) sort(ae);
	if (e2->next != nil) sort(&e2);

	e1= *ae;
	for (;;) {
		if (strcmp(e1->name, e2->name)<=0) {
			if ((e1= *(ae= &e1->next)) == nil) {
				*ae= e2;
				break;
			}
		} else {
			*ae= e2;
			e2= *(ae= &e2->next);
			*ae= e1;
			if (e2 == nil) break;
		}
	}
}

static void enter()
/* Collect directory entries of E. */
{
	struct entry **last= &E->con, *new;
	struct dirent *e;
	DIR *d;

	if ((d= opendir(path)) == nil) {
		fprintf(stderr, "%s: Can't read dir %s\n", arg0, path);
		return;
	}

	while ((e= readdir(d)) != nil) {
		if (e->d_name[0] == '.' && (e->d_name[1] == 0
			|| (e->d_name[1] == '.' && e->d_name[2] == 0)
		)) continue;

		new= (struct entry *) malloc(sizeof(*new));

		new->next= nil;
		new->dir= E;
		new->con= nil;
		new->name= (char *) malloc(strlen(e->d_name) + 1);
		strcpy(new->name, e->d_name);
		*last= new;
		last= &new->next;
	}
	closedir(d);
	if (E->con != nil && E->con->next != nil) sort(&E->con);
}

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

static char *link_islink(struct stat *stp, const char *file)
{
    /* Tell if a file, which stat(2) information in '*stp', has been seen
     * earlier by this function under a different name.  If not return a
     * null pointer with errno set to ENOENT, otherwise return the name of
     * the link.  Return a null pointer with an error code in errno for any
     * error, using E2BIG for a too long file name.
     *
     * Use link_islink(nil, nil) to reset all bookkeeping.
     *
     * Call for a file twice to delete it from the store.
     */

    typedef struct link {	/* In-memory link store. */
	struct link	*next;		/* Hash chain on inode number. */
	ino_t		ino;		/* File's inode number. */
	off_t		off;		/* Offset to more info in temp file. */
    } link_t;
    typedef struct dlink {	/* On-disk link store. */
	dev_t		dev;		/* Device number. */
	char		file[PATH_MAX];	/* Name of earlier seen link. */
    } dlink_t;
    static link_t *links[256];		/* Hash list of known links. */
    static int tfd= -1;			/* Temp file for file name storage. */
    static dlink_t dlink;
    link_t *lp, **plp;
    size_t len;
    off_t off;

    if (file == nil) {
	/* Reset everything. */
	for (plp= links; plp < arraylimit(links); plp++) {
	    while ((lp= *plp) != nil) {
		*plp= lp->next;
		free(lp);
	    }
	}
	if (tfd != -1) close(tfd);
	tfd= -1;
	return nil;
    }

    /* The file must be a non-directory with more than one link. */
    if (S_ISDIR(stp->st_mode) || stp->st_nlink <= 1) {
	errno= ENOENT;
	return nil;
    }

    plp= &links[stp->st_ino % arraysize(links)];

    while ((lp= *plp) != nil) {
	if (lp->ino == stp->st_ino) {
	    /* May have seen this link before.  Get it and check. */
	    if (lseek(tfd, lp->off, SEEK_SET) == -1) return nil;
	    if (read(tfd, &dlink, sizeof(dlink)) < 0) return nil;

	    /* Only need to check the device number. */
	    if (dlink.dev == stp->st_dev) {
		if (strcmp(file, dlink.file) == 0) {
		    /* Called twice.  Forget about this link. */
		    *plp= lp->next;
		    free(lp);
		    errno= ENOENT;
		    return nil;
		}

		/* Return the name of the earlier link. */
		return dlink.file;
	    }
	}
	plp= &lp->next;
    }

    /* First time I see this link.  Add it to the store. */
    if (tfd == -1) {
	for (;;) {
	    char *tmp;

	    tmp= tmpnam(nil);
	    tfd= open(tmp, O_RDWR|O_CREAT|O_EXCL, 0600);
	    if (tfd < 0) {
		if (errno != EEXIST) return nil;
	    } else {
		(void) unlink(tmp);
		break;
	    }
	}
    }
    if ((len= strlen(file)) >= PATH_MAX) {
	errno= E2BIG;
	return nil;
    }

    dlink.dev= stp->st_dev;
    strcpy(dlink.file, file);
    len += offsetof(dlink_t, file) + 1;
    if ((off= lseek(tfd, 0, SEEK_END)) == -1) return nil;
    if (write(tfd, &dlink, len) != len) return nil;

    if ((lp= malloc(sizeof(*lp))) == nil) return nil;
    lp->next= nil;
    lp->ino= stp->st_ino;
    lp->off= off;
    *plp= lp;
    errno= ENOENT;
    return nil;
}

#define cancellink()	((void) islink())

static char *islink()
/* Returns the name of the file path is linked to.  If no such link can be
 * found, then path is added to the list and nil is returned.  If all the
 * links of a file have been seen, then it is removed from the list.
 * Directories are not seen as linkable.
 */
{
	char *name;

	name= link_islink(&st, path);
	if (name == nil && errno != ENOENT) perrx(path);
	return name;
}

static void setstat(ino_t ino, struct stat *stp)
/* Set backup status info, we know that backup is true. */
{
	struct mode md;

	md.md_mode = stp->st_mode;
	md.md_uid = stp->st_uid;
	md.md_gid = stp->st_gid;
	md.md_rdev = stp->st_rdev;
	md.md_devsiz = stp->st_size / 1024;

	if (lseek(filemodes, (off_t) sizeof(md) * (off_t) ino, 0) == -1
		|| write(filemodes, (char *) &md, sizeof(md)) != sizeof(md)
	) perrx(BACKUP);
}

static int getstat(char *name, struct stat *stp)
/* Get status information of file name, skipping some files.  Backup info
 * is inserted as needed.
 */
{
	errno= 0;

	if (strcmp(name, BACKUP) == 0) return -1;

	if (lstat(name, stp) < 0) return -1;

	if (stp->st_mode == S_IFREG && stp->st_mtime == 0) return -1;

	if (backup) {
		struct mode md;

		if (lseek(filemodes,
			(off_t) sizeof(md) * (off_t) stp->st_ino, 0) == -1
		    || read(filemodes, (char *) &md, sizeof(md)) != sizeof(md)
		    || md.md_mode == 0
		) {
			errno= 0;
			setstat(stp->st_ino, stp);
		} else {
			stp->st_mode = md.md_mode;
			stp->st_uid = md.md_uid;
			stp->st_gid = md.md_gid;
			stp->st_rdev = md.md_rdev;
			if (S_ISBLK(stp->st_mode))
				stp->st_size= (off_t) md.md_devsiz * 1024;
		}
	}
	return 0;
}

static int advance()
/* Determine next pathname, return true on success. */
{
	for (;;) {
		if (E==nil) {	/* First call, enter root dir. */
			E= (struct entry *) malloc(sizeof(*E));
			E->dir= nil;
			E->con= nil;
			E->next= nil;
			E->name= (char *) malloc(3);
			strcpy(E->name, transparent ? ".@" : ".");
		} else
		if (E->con != nil)	/* Dir's files must be processed. */
			E= E->con;
		else {
			for (;;) {
				/* Remove E from it's parents list, then
				 * try next entry, if none, go to parent dir.
				 */
				struct entry *junk= E, *parent= E->dir;

				if (parent != nil) parent->con= E->next;
				E= E->next;
				free(junk->name);
				free(junk);

				if (E != nil) break;

				if ((E= parent) == nil) return 0;
			}
		}
		setpath(E);
		if (getstat(path, &st) == 0) {
			if (S_ISLNK(st.st_mode)) {
				int n;

				if ((n= readlink(path, lnkpth, PATH_MAX)) >= 0)
				{
					lnkpth[n]= 0;
					break;
				}
			} else {
				break;
			}
		}
		if (errno != 0 && errno != ENOENT) perr(path);
	}

	linkpath= islink();
	DPRINTF("%s: path = %s\n", path);
	return 1;
}

static enum orders request()
/* Slave reads command sent by master. */
{
	static char buf[64], *bp;
	static int n= 0;
	int req;

	for (;;) {
		if (n == 0) {
			if ((n= read(chan[0], buf, (int) sizeof(buf))) <= 0) {
				if (n < 0) perrx("request()");
				/* Master died, try to report it then follow. */
				fprintf(stderr,
					"%s: Master died prematurely.\n", arg0);
				exit(1);
			}
			bp= buf;
		}
		req= *bp++ & 0xFF;
		n--;
		if (req >= (int) ENTER) break;

		/* Master using slave to print to stdout: */
		putchar(req);
	}
	DPRINTF("%s: request() == %s\n", ORDERS[req - (int) ENTER]);

	return (enum orders) req;
}

static void report()
{
	int r;

	DPRINTF("%s: reporting now!\n", 0);

	buckp= bucket;

	while (buckn > 0) {
		r = buckn;
		if (r > (512 << sizeof(char*))) r= (512 << sizeof(char*));

		if ((r= write(chan[1], buckp, r)) < 0) perrx("report()");

		buckp += r;
		buckn -= r;
	}
	buckp= bucket;
	buckn= 0;
}

static void inform(enum answers a)
/* Slave replies to master. */
{
	DPRINTF("%s: inform(%s)\n", ANSWERS[(int) a - (int) PATH]);

	*buckp++ = (int) a;
	buckn++;
}

#define wwrite(buf, n)	(memcpy(buckp, (buf), (n)), buckp+= (n), buckn+= (n))

static void sendnum(long n)
/* Send number from least to most significant byte. */
{
#if BYTE_ORDER == LITTLE_ENDIAN
	wwrite((char *) &n, sizeof(n));
#else
	char buf[NUMBYTES];

	buf[0]= (char) (n >>  0);
	buf[1]= (char) (n >>  8);
	buf[2]= (char) (n >> 16);
	buf[3]= (char) (n >> 24);
	wwrite(buf, sizeof(buf));
#endif
}

static void send(char *buf, int n)
/* Slave sends size and contents of buf. */
{
	sendnum((long) n);
	if (n > 0) wwrite(buf, (size_t) n);
}

static void sendstat(struct stat *stp)
{
	sendnum((long) stp->st_mode);
	sendnum((long) stp->st_uid);
	sendnum((long) stp->st_gid);
	sendnum((long) stp->st_rdev);
	sendnum((long) stp->st_size);
	sendnum((long) stp->st_mtime);
}

static int ask();

static void slave()
/* Carry out orders from the master, such as transmitting path names.
 * Note that the slave uses path, not Spath, the master uses Spath.
 */
{
	int f, n;
	char buf[CHUNK];
	enum { run, done, die } state= run;

	do {
		switch (request()) {
		case ENTER:
			enter();
			break;
		case ADVANCE:
			if (!advance() || state == done) {
				inform(DONE);
				state= done;
			} else {
				if (linkpath!=nil) {
					inform(LINK);
					send(linkpath, strlen(linkpath) + 1);
				} else
				if (S_ISLNK(st.st_mode)) {
					inform(SYMLINK);
					send(lnkpth, strlen(lnkpth) + 1);
				} else {
					inform(PATH);
				}
				send(path, strlen(path) + 1);
				sendstat(&st);
			}
			break;
		case CAT:
			if ((f= open(path, O_RDONLY))<0) {
				fprintf(stderr, "%s: Can't open %s",
					arg0, path);
				because();
				inform(NODATA);
				break;
			}
			inform(DATA);
			do {
				n= read(f, buf, sizeof(buf));
				if (n < 0) perr(path);
				send(buf, n);
				if (n > 0) report();
			} while (n > 0);
			close(f);
			break;
		case ORDER_CANCEL:
			cancellink();
			break;
		case DIE_BAD:
			ex= 1;
			/*FALL THROUGH*/
		case DIE:
			state= die;
			break;
		case POSITIVE:
			inform(ask('y') ? YES : NO);
			break;
		case NEGATIVE:
			inform(ask('n') ? YES : NO);
			break;
		case PASS_YES:
			inform(YES);
			break;
		case PASS_NO:
			inform(NO);
			break;
		default:
			fprintf(stderr, "%s: strange request\n", arg0);
			exit(1);
		}
		report();
	} while (state != die);
}

static int execute(char **argv)
/* Execute a command and return its success or failure. */
{
	int pid, r, status;

	if ((pid= fork())<0) {
		perr("fork()");
		return 0;
	}
	if (pid == 0) {
		execvp(argv[0], argv);
		perrx(argv[0]);
	}
	while ((r= wait(&status)) != pid) {
		if (r < 0) {
			perr(argv[0]);
			return 0;
		}
	}
	return status == 0;
}

static int removedir(char *dir)
/* Remove a directory and its contents. */
{
	static char *argv[] = { "rm", "-r", nil, nil };

	printf("(rm -r %s)\n", dir);

	argv[2]= dir;
	return execute(argv);
}

static void order(enum orders o)
/* Master tells slave what to do. */
{
	char c= (char) o;

	DPRINTF("%s: order(%s)\n", ORDERS[o - (int) ENTER]);

	if (write(chan[1], &c, 1) != 1) perrx("order()");
}

static void rread(char *buf, int n)
/* Master gets buf of size n from slave, doing multiple reads if needed. */
{
	int r;

	while (n > 0) {
		if (buckn == 0) {
			switch (buckn= read(chan[0], bucket, BUCKSIZE)) {
			case -1:
				perrx("reply channel from slave");
			case  0:
				fprintf(stderr,
					"%s: slave died prematurely.\n",
					arg0);
				exit(1);
			}
			buckp= bucket;
		}
		r= n < buckn ? n : buckn;
		memcpy(buf, buckp, r);
		buckp+= r;
		buckn-= r;
		buf+= r;
		n-= r;
	}
}

static enum answers answer()
/* Master reads slave's reply. */
{
	char c;
	int a;

	rread(&c, 1);
	a= c & 0xFF;

	DPRINTF("%s: answer() == %s\n", ANSWERS[a - (int) PATH]);

	return (enum answers) a;
}

static long recnum()
/* Read number as pack of bytes from least to most significant.  The data
 * is on the wire in little-endian format.  (Mostly run on PC's).
 */
{
#if BYTE_ORDER == LITTLE_ENDIAN
	long n;

	rread((char *) &n, (int) sizeof(n));
	return n;
#else
	unsigned char buf[NUMBYTES];

	rread(buf, sizeof(buf));
	return	buf[0]
		| ((unsigned) buf[1] << 8)
		| ((unsigned long) buf[2] << 16)
		| ((unsigned long) buf[3] << 24);
#endif
}

static int receive(char *buf, int max)
/* Master get's data from slave, by first reading size, then data. */
{
	int n;

	n= recnum();
	if (n > max) {
		fprintf(stderr, "%s: panic: Can't read %d bytes\n", arg0, n);
		exit(1);
	}
	if (n > 0) rread(buf, n);
	return n;
}

static void recstat(struct stat *stp)
{
	stp->st_mode= recnum();
	stp->st_uid= recnum();
	stp->st_gid= recnum();
	stp->st_rdev= recnum();
	stp->st_size= recnum();
	stp->st_mtime= recnum();
}

static int key()
{
	int c;
	static int tty= -1;

	if (tty < 0) tty= isatty(0);

	if (feof(stdin) || (c= getchar()) == EOF) {
		c= '\n';
		if (tty) putchar('\n');
	}

	if (!tty) putchar(c);

	return c;
}

static int ask(int def)
/* Ask for a yes or no, anything else means choose def. */
{
	int y, c;

	if (chan[0] == 0) {
		/* I'm running remote, tell the slave to ask. */
		fflush(stdout);
		order(def == 'y' ? POSITIVE : NEGATIVE);
		return answer() == YES;
	}

	printf("? (%c) ", def);
	fflush(stdout);

	do c= key(); while (c == ' ' || c == '\t');

	y= c;

	while (c != '\n') c= key();

	if (y != 'y' && y != 'Y' && y != 'n' && y != 'N') y= def;

	return y == 'y' || y == 'Y';
}

static void setmodes(int silent)
{
	struct stat st;
	int change= 0;
	struct utimbuf tms;

	errno= 0;
	getstat(Spath, &st);
	if (backup && silent) {
		setstat(st.st_ino, &Sst);
		getstat(Spath, &st);
	}

	if (S_ISLNK(st.st_mode)) return;

	if (errno == 0 && st.st_mode != Sst.st_mode) {
		if (!backup) chmod(Spath, Sst.st_mode & 07777);
		change= 1;
	}
	if (errno == 0
		&& (st.st_uid != Sst.st_uid || st.st_gid != Sst.st_gid)
		&& (backup || geteuid() == 0)
	) {
		errno= 0;
		if (!backup) chown(Spath, Sst.st_uid, Sst.st_gid);
		change= 1;
	}

	if (backup && !silent) setstat(st.st_ino, &Sst);

	if (errno == 0 && S_ISREG(Sst.st_mode) && st.st_mtime != Sst.st_mtime) {
		time(&tms.actime);
		tms.modtime= Sst.st_mtime;
		errno= 0;
		utime(Spath, &tms);
		change= 1;
	}
	if (errno != 0) {
		fprintf(stderr, "%s: Can't set modes of %s", arg0, Spath);
		because();
	} else
	if (change && !silent) {
		printf("Mode changed of %s\n", Spath);
	}
}

static void makeold()
{
	static struct utimbuf tms= { 0, 0 };

	if (utime(Spath, &tms) < 0) {
		if (errno != ENOENT) {
			fprintf(stderr,
				"%s: can't make %s look old", arg0, Spath);
			because();
		}
	} else {
		fprintf(stderr, "%s: made %s look old.\n", arg0, Spath);
	}
}

static int busy= 0;

static void bail_out(int sig)
{
	signal(sig, SIG_IGN);

	fprintf(stderr, "%s: Exiting after signal %d\n", arg0, sig);

	if (busy) {
		fprintf(stderr, "%s: was installing %s\n", arg0, Spath);
		makeold();
	}
	order(DIE_BAD);

	exit(sig);
}

static int makenode(char *name, int mode, dev_t addr, off_t size)
{
	int r;

	if (!backup) {
		r= mknod(name, mode, addr);
	} else {
		if ((r= creat(name, 0644)) >= 0) close(r);
	}
	return r;
}

static void add(int update)
/* Add Spath to the filesystem. */
{
	int f, n;
	char buf[CHUNK];
	int forced_update= force && update;

	if (Slinkpath != nil && !S_ISLNK(Sst.st_mode)) {
		if (interact && !update) {
			printf("Link %s to %s", Spath, Slinkpath);
			if (!ask('n')) return;
		}
		if (link(Slinkpath, Spath) >= 0) {
			printf("Linked %s to %s\n", Spath, Slinkpath);
			return;
		} else {
			fprintf(stderr,
				"%s: Can't link %s to %s",
				arg0, Slinkpath, Spath);
			because();
			/* Try to install instead. */
		}
	}
	switch (Sst.st_mode & S_IFMT) {
	case S_IFDIR:
		if (!force) {
			printf("Add dir %s", Spath);
			if (!ask('n')) return;
		}
		if (mkdir(Spath, backup ? 0755 : Sst.st_mode) < 0) {
			perr(Spath);
			return;
		}
		printf("Directory %s created.\n", Spath);
		order(ENTER);
		break;
	case S_IFBLK:
	case S_IFCHR:
	case S_IFIFO:
		if (interact && !update) {
			printf("Create special file %s", Spath);
			if (!ask('n')) { order(ORDER_CANCEL); return; }
		}
		if (makenode(Spath, Sst.st_mode, Sst.st_rdev, Sst.st_size)<0) {
			fprintf(stderr,
				"%s: Can't create special file %s",
				arg0, Spath);
			because();
			return;
		}
		printf("Special file %s created\n", Spath);
		break;
#ifdef S_IFLNK
	case S_IFLNK:
		if (interact && !update) {
			printf("Install %s -> %s", Spath, Slnkpth);
			if (!ask('n')) { order(ORDER_CANCEL); return; }
		}
		if (symlink(Slnkpth, Spath) < 0) {
			fprintf(stderr, "%s: Can't create symlink %s",
				arg0, Spath);
			because();
			return;
		}
		printf("%s %s -> %s\n",
			forced_update ? "Updated:  " : "Installed:",
			Spath, Slnkpth);
		break;
#endif
	case S_IFREG:
		if (interact && !update) {
			printf("Install %s", Spath);
			if (!ask('n')) { order(ORDER_CANCEL); return; }
		}
		order(CAT);
		if (answer() != DATA) return;

		busy= 1;
		if ((f= creat(Spath, backup ? 0644 : Sst.st_mode&07777)) < 0) {
			busy= 0;
			fprintf(stderr, "%s: Can't create %s", arg0, Spath);
			because();
		}

		while ((n= receive(buf, sizeof(buf)))>0) {
			if (f >= 0 && write(f, buf, n) != n) {
				fprintf(stderr, "%s: Write error on %s",
					arg0, Spath);
				because();
				close(f); f= -1;
			}
		}
		if (n < 0) {
			fprintf(stderr, "%s: Slave read err on %s\n",
				arg0, Spath);
		}
		if (f >= 0) close(f);
		if (n < 0 || f < 0) { makeold(); busy= 0; return; }
		busy= 0;
		printf("%s %s\n",
			forced_update ?
				Sst.st_mtime < st.st_mtime ? "Restored: " :
					"Updated:  " :
				"Installed:",
			Spath
		);
		break;
	default:
		fprintf(stderr,
			"%s: Won't add file with strange mode %05o: %s\n",
			arg0, Sst.st_mode, Spath);
		order(ORDER_CANCEL);
		return;
	}
	setmodes(1);
}

static int delete(int update)
/* Delete path. */
{
	int forced_update= force && update;

	if (S_ISDIR(st.st_mode)) {
		if (install) return 0;
		if (!force) {
			printf("Delete dir %s", path);
			if (!ask('n')) return 0;
		}
		if (!removedir(path)) { ex= 1; return 0; }
		if (!forced_update) printf("Directory %s deleted.\n", path);
		return 1;
	}

	if (install && !update) return 0;

	if (!force) {
		printf("Delete %s", path);
		if (!ask((interact && !update) ? 'n' : 'y')) return 0;
	}

	if (unlink(path)<0) {
		fprintf(stderr, "Can't delete %s", path);
		because();
		return 0;
	}
	cancellink();
	if (!forced_update) printf("Deleted:   %s\n", path);
	return 1;
}

static int different()
/* Return true iff path and Spath are different. */
{
	if (! ( (linkpath == nil && Slinkpath == nil)
		|| (linkpath != nil && Slinkpath != nil
			&& strcmp(linkpath, Slinkpath) == 0)
	)) {
		linkpath= Slinkpath;
		return 1;
	}

	if ((st.st_mode & S_IFMT) != (Sst.st_mode & S_IFMT)) return 1;

	switch (st.st_mode & S_IFMT) {
	case S_IFDIR:
		return 0;
	case S_IFBLK:
	case S_IFCHR:
		return st.st_rdev != Sst.st_rdev;
	case S_IFREG:
		if (install) return Sst.st_mtime > st.st_mtime;
		return st.st_size != Sst.st_size
			|| st.st_mtime != Sst.st_mtime;
	case S_IFIFO:	return 0;
#ifdef S_IFLNK
	case S_IFLNK:	return strcmp(lnkpth, Slnkpth) != 0;
#endif
	default:	return 1;
	}
}

static void compare()
/* See if path and Spath are same. */
{
	if (different()) {
		if (!force) {
			printf("%sing %s (delete + add)\n",
				Sst.st_mtime < st.st_mtime ? "Restor" : "Updat",
				path);
		}
		if (delete(1)) add(1);
	} else {
		if (!install) setmodes(0);

		if (S_ISDIR(st.st_mode)) {
			order(ENTER);
			enter();
		}
	}
}

static int done= 0, Sdone= 0;

static enum action { ADD, COMPARE, DELETE } action()
/* Look at path's of master and slave, compare them alphabetically to see
 * who is ahead of who, then tell what is to be done.
 */
{
	int c;
	char *Sp, *p;

	if (done) return ADD;		/* Slave still has names. */
	if (Sdone) return DELETE;	/* Master has too many names. */

	/* Compare paths.  Let "a/a" come before "a.a". */
	Sp= Spath;
	p= path;
	while (*Sp == *p && *Sp != 0) { Sp++; p++; }
	if (*Sp == '/') return ADD;
	if (*p == '/') return DELETE;
	return (c= strcmp(Sp, p)) == 0 ? COMPARE : c < 0 ? ADD : DELETE;
}

static void master()
/* Synchronise file tree to that of its slave. */
{
	enum action a= COMPARE;	/* Trick first advances. */

	umask(backup ? 0022 : 0000);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, bail_out);
	signal(SIGINT, bail_out);
	signal(SIGTERM, bail_out);

	while (!done || !Sdone) {
		if (!Sdone && (a == ADD || a == COMPARE)) {
			/* Slave advances. */
			order(ADVANCE);
			switch (answer()) {
			case PATH:
				Slinkpath= nil;
				receive(Spath, sizeof(Spath));
				recstat(&Sst);
				break;
			case LINK:
				receive(Slnkpth, sizeof(Slnkpth));
				Slinkpath= Slnkpth;
				receive(Spath, sizeof(Spath));
				recstat(&Sst);
				break;
			case SYMLINK:
				Slinkpath= nil;
				receive(Slnkpth, sizeof(Slnkpth));
				receive(Spath, sizeof(Spath));
				recstat(&Sst);
				break;
			case DONE:
				Sdone= 1;
				break;
			default:
				fprintf(stderr,
					"%s: Strange answer from slave.\n",
					arg0);
				exit(1);
			}
		}
		if (!done && (a == COMPARE || a == DELETE)) {
			/* Master advances. */
			if (!advance()) done= 1;
		}

		if (done && Sdone) break;

		switch (a= action()) {
		case ADD:	/* Spath exists, path doesn't, add? */
			add(0);
			break;
		case COMPARE:	/* They both exist, are they the same? */
			compare();
			break;
		case DELETE:	/* path exists, Spath doesn't, delete? */
			delete(0);
		}
		fflush(stdout);	/* Don't keep user in suspense. */
	}
	order(ex == 0 ? DIE : DIE_BAD);
}

static void mediator()
/* Sits at the local machine and passes orders from master to slave, both
 * on remote machines.  Only diagnostics and questions are handled.
 */
{
	enum orders req;

	for (;;) {
		switch (req= request()) {
		case DIE_BAD:
			ex= 1;
			/*FALL THROUGH*/
		case DIE:
			order(DIE);
			return;
		case POSITIVE:
			order(ask('y') ? PASS_YES : PASS_NO);
			break;
		case NEGATIVE:
			order(ask('n') ? PASS_YES : PASS_NO);
			break;
		default:
			order(req);
		}
	}
}

#define P_EXIT		1	/* Make sure process doesn't return. */
#define P_SHADOW	2	/* Always use exec on 68000. */

static void startprocess(void (*proc)(), char *machine, char *path,
	int p_flags)
{
	char *argv[10], **argp= argv;
	char flags[10], *pfl= flags;

	if (machine != nil) {
		char *u= machine, *m;

		*argp++ = "rsh";
		if ((m= strchr(machine, '@')) != nil) {
			*m++ = 0;
			*argp++ = "-l";
			*argp++ = u;
			machine= m;
		}
		*argp++ = machine;
	} else
	/* Without this check it would run like a pig on an non MMU 68000: */
	if (!(USE_SHADOWING && p_flags & P_SHADOW)) {
		if (chdir(path) < 0) {
			if (proc != master || errno != ENOENT
						|| mkdir(path, 0700) < 0)
				perrx(path);
			if (chdir(path) < 0) perrx(path);
			printf("Destination directory %s created\n", path);
		}
		isvisible(path);
		isbackup(proc == slave);
		(*proc)();
		if (p_flags & P_EXIT) exit(ex);
		return;
	}
	*argp++ = SYNCNAME;
	*pfl++ = '-';
	if (interact) *pfl++ = 'i';
	if (install) *pfl++ = 'u';
	if (force) *pfl++ = 'f';
	*pfl= 0;
	*argp++ = flags;
	*argp++ = proc == slave ? SLAVENAME : MASTERNAME;
	*argp++ = path;
	*argp++ = nil;
#ifdef DEBUG
	fprintf(stderr, "execlp(");
	for (argp= argv; *argp != nil; argp++) fprintf(stderr, "%s, ", *argp);
	fprintf(stderr, "nil);\n");
#endif
	execvp(argv[0], argv);
	perrx(argv[0]);
}

void splitcolon(char *path, char **amach, char **adir)
{
	char *dir= path;

	for (;;) {
		if (*dir == ':') {
			*dir++ = 0;
			*amach= path;
			*adir= dir;
			break;
		}
		if (*dir == 0 || *dir == '/') {
			*amach= nil;
			*adir= path;
			break;
		}
		dir++;
	}
}

static void Usage()
{
	fprintf(stderr,
	    "Usage: %s [-iuf] [[user@]machine:]dir1 [[user@]machine:]dir2\n",
		arg0);
	exit(1);
}

int main(int argc, char **argv)
{
	char *s_mach, *s_dir;
	char *m_mach, *m_dir;
	int m2s[2], s2m[2], m2m[2];
	int s_pid= 0, m_pid= 0;
	int r;

	if ((arg0= strrchr(argv[0], '/')) == nil) arg0= argv[0]; else arg0++;

	while (argc>1 && argv[1][0] == '-') {
		char *f= argv[1]+1;

		while (*f != 0) {
			switch (*f++) {
			case 'i':	interact= 1; break;
			case 'u':	install= 1; break;
			case 'f':	force= 1; break;
			default:	Usage();
			}
		}
		argc--;
		argv++;
	}

	if (argc != 3) Usage();

	if (strcmp(argv[1], SLAVENAME) == 0) {
		arg0= "Slave";
		splitcolon(argv[2], &s_mach, &s_dir);
		startprocess(slave, s_mach, s_dir, P_EXIT);
	} else
	if (strcmp(argv[1], MASTERNAME) == 0) {
		arg0= "Master";
		splitcolon(argv[2], &m_mach, &m_dir);
		startprocess(master, m_mach, m_dir, P_EXIT);
	}

	splitcolon(argv[1], &s_mach, &s_dir);
	splitcolon(argv[2], &m_mach, &m_dir);

	/* How difficult can plumbing be? */
	if (pipe(m2s) < 0 || pipe(s2m) < 0) perrx("pipe()");

	if (m_mach == nil) {
		/* synctree [machine:]dir1 dir2 */
		switch (s_pid= fork()) {
		case -1:
			perrx("fork()");
		case 0:
			dup2(m2s[0], 0); close(m2s[0]); close(m2s[1]);
			dup2(s2m[1], 1); close(s2m[0]); close(s2m[1]);
			arg0= "Slave";
			startprocess(slave, s_mach, s_dir, P_EXIT|P_SHADOW);
		}
		chan[0]= s2m[0]; close(s2m[1]);
		chan[1]= m2s[1]; close(m2s[0]);
		startprocess(master, m_mach, m_dir, 0);
	} else
	if (s_mach == nil) {
		/* synctree dir1 machine:dir2 */
		switch (m_pid= fork()) {
		case -1:
			perrx("fork()");
		case 0:
			dup2(s2m[0], 0); close(s2m[0]); close(s2m[1]);
			dup2(m2s[1], 1); close(m2s[0]); close(m2s[1]);
			arg0= "Master";
			startprocess(master, m_mach, m_dir, P_EXIT|P_SHADOW);
		}
		chan[0]= m2s[0]; close(m2s[1]);
		chan[1]= s2m[1]; close(s2m[0]);
		startprocess(slave, s_mach, s_dir, 0);
	} else {
		/* synctree machine1:dir1 machine2:dir2 */
		if (pipe(m2m) < 0) perrx(pipe);

		switch (s_pid= fork()) {
		case -1:
			perrx("fork()");
		case 0:
			dup2(m2s[0], 0); close(m2s[0]); close(m2s[1]);
			dup2(s2m[1], 1); close(s2m[0]); close(s2m[1]);
			close(m2m[0]); close(m2m[1]);
			arg0= "Slave";
			startprocess(slave, s_mach, s_dir, P_EXIT|P_SHADOW);
		}

		switch (m_pid= fork()) {
		case -1:
			perrx("fork()");
		case 0:
			dup2(s2m[0], 0); close(s2m[0]); close(s2m[1]);
			close(m2s[0]); close(m2s[1]);
			dup2(m2m[1], 1); close(m2m[0]); close(m2m[1]);
			arg0= "Master";
			startprocess(master, m_mach, m_dir, P_EXIT|P_SHADOW);
		}
		close(s2m[0]); close(s2m[1]);
		chan[0]= m2m[0]; close(m2m[1]);
		chan[1]= m2s[1]; close(m2s[0]);
		mediator();
	}
	close(chan[0]);
	close(chan[1]);

	alarm(15); /* Don't wait(2) forever. */

	while (s_pid != 0 || m_pid != 0) {
		if ((r= wait((int *) nil)) < 0) perrx("wait()");
		if (r == s_pid) s_pid= 0;
		if (r == m_pid) m_pid= 0;
	}
	exit(ex);
}
