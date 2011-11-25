/*	cp 1.12 - copy files				Author: Kees J. Bot
 *	mv      - move files					20 Jul 1993
 *	rm      - remove files
 *	ln      - make a link
 *	cpdir   - copy a directory tree (cp -psmr)
 *	clone   - make a link farm (ln -fmr)
 */
#define nil 0
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#ifndef DEBUG
#define DEBUG	0
#define NDEBUG	1
#endif
#include <assert.h>

/* Copy files in this size chunks: */
#if __minix && !__minix_vmd
#define CHUNK	(8192 * sizeof(char *))
#else
#define CHUNK	(1024 << (sizeof(int) + sizeof(char *)))
#endif


#ifndef CONFORMING
#define CONFORMING	1	/* Precisely POSIX conforming. */
#endif


#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

char *prog_name;		/* Call name of this program. */
int ex_code= 0;			/* Final exit code. */

typedef enum identity { CP, MV, RM, LN, CPDIR, CLONE } identity_t;
typedef enum action { COPY, MOVE, REMOVE, LINK } action_t;

identity_t identity;		/* How did the user call me? */
action_t action;		/* Copying, moving, or linking. */
int pflag= 0;			/* -p/-s: Make orginal and copy the same. */
int iflag= 0;			/* -i: Interactive overwriting/deleting. */
int fflag= 0;			/* -f: Force. */
int sflag= 0;			/* -s: Make a symbolic link (ln/clone). */
int Sflag= 0;			/* -S: Make a symlink if across devices. */
int mflag= 0;			/* -m: Merge trees, no target dir trickery. */
int rflag= 0;			/* -r/-R: Recursively copy a tree. */
int vflag= 0;			/* -v: Verbose. */
int xflag= 0;			/* -x: Don't traverse past mount points. */
int xdev= 0;			/* Set when moving or linking cross-device. */
int expand= 0;			/* Expand symlinks, ignore links. */
int conforming= CONFORMING;	/* Sometimes standards are a pain. */

int fc_mask;			/* File creation mask. */
int uid, gid;			/* Effective uid & gid. */
int istty;			/* Can have terminal input. */

#ifndef S_ISLNK
/* There were no symlinks in medieval times. */
#define S_ISLNK(mode)			(0)
#define lstat				stat
#define symlink(path1, path2)		(errno= ENOSYS, -1)
#define readlink(path, buf, len)	(errno= ENOSYS, -1)
#endif

void report(const char *label)
{
    if (action == REMOVE && fflag) return;
    fprintf(stderr, "%s: %s: %s\n", prog_name, label, strerror(errno));
    ex_code= 1;
}

void fatal(const char *label)
{
    report(label);
    exit(1);
}

void report2(const char *src, const char *dst)
{
    fprintf(stderr, "%s %s %s: %s\n", prog_name, src, dst, strerror(errno));
    ex_code= 1;
}

#if DEBUG
size_t nchunks= 0;	/* Number of allocated cells. */
#endif

void *allocate(void *mem, size_t size)
/* Like realloc, but with checking of the return value. */
{
#if DEBUG
    if (mem == nil) nchunks++;
#endif
    if ((mem= mem == nil ? malloc(size) : realloc(mem, size)) == nil)
	fatal("malloc()");
    return mem;
}

void deallocate(void *mem)
/* Release a chunk of memory. */
{
    if (mem != nil) {
#if DEBUG
	nchunks--;
#endif
	free(mem);
    }
}

typedef struct pathname {
    char		*path;	/* The actual pathname. */
    size_t		idx;	/* Index for the terminating null byte. */
    size_t		lim;	/* Actual length of the path array. */
} pathname_t;

void path_init(pathname_t *pp)
/* Initialize a pathname to the null string. */
{
    pp->path= allocate(nil, pp->lim= NAME_MAX + 2);
    pp->path[pp->idx= 0]= 0;
}

void path_add(pathname_t *pp, const char *name)
/* Add a component to a pathname. */
{
    size_t lim;
    char *p;

    lim= pp->idx + strlen(name) + 2;

    if (lim > pp->lim) {
	pp->lim= lim += lim/2;	/* add an extra 50% growing space. */

	pp->path= allocate(pp->path, lim);
    }

    p= pp->path + pp->idx;
    if (p > pp->path && p[-1] != '/') *p++ = '/';

    while (*name != 0) {
	if (*name != '/' || p == pp->path || p[-1] != '/') *p++ = *name;
	name++;
    }
    *p = 0;
    pp->idx= p - pp->path;
}

void path_trunc(pathname_t *pp, size_t didx)
/* Delete part of a pathname to a remembered length. */
{
    pp->path[pp->idx= didx]= 0;
}

#if DEBUG
const char *path_name(const pathname_t *pp)
/* Return the actual name as a C string. */
{
    return pp->path;
}

size_t path_length(const pathname_t *pp)
/* The length of the pathname. */
{
    return pp->idx;
}

void path_drop(pathname_t *pp)
/* Release the storage occupied by the pathname. */
{
    deallocate(pp->path);
}

#else /* !DEBUG */
#define path_name(pp)		((const char *) (pp)->path)
#define path_length(pp)		((pp)->idx)
#define path_drop(pp)		deallocate((void *) (pp)->path)
#endif /* !DEBUG */

char *basename(const char *path)
/* Return the last component of a pathname.  (Note: declassifies a const
 * char * just like strchr.
 */
{
    const char *p= path;

    for (;;) {
	while (*p == '/') p++;			/* Trailing slashes? */

	if (*p == 0) break;

	path= p;
	while (*p != 0 && *p != '/') p++;	/* Skip component. */
    }
    return (char *) path;
}

int affirmative(void)
/* Get a yes/no answer from the suspecting user. */
{
    int c;
    int ok;

    fflush(stdout);
    fflush(stderr);

    while ((c= getchar()) == ' ') {}
    ok= (c == 'y' || c == 'Y');
    while (c != EOF && c != '\n') c= getchar();

    return ok;
}

int writable(const struct stat *stp)
/* True iff the file with the given attributes allows writing.  (And we have
 * a terminal to ask if ok to overwrite.)
 */
{
    if (!istty || uid == 0) return 1;
    if (stp->st_uid == uid) return stp->st_mode & S_IWUSR;
    if (stp->st_gid == gid) return stp->st_mode & S_IWGRP;
    return stp->st_mode & S_IWOTH;
}

#ifndef PATH_MAX
#define PATH_MAX	1024
#endif

static char *link_islink(const struct stat *stp, const char *file)
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

int trylink(const char *src, const char *dst, const struct stat *srcst,
			const struct stat *dstst)
/* Keep the link structure intact if src has been seen before. */
{
    char *olddst;
    int linked;

    if (action == COPY && expand) return 0;

    if ((olddst= link_islink(srcst, dst)) == nil) {
	/* if (errno != ENOENT) ... */
	return 0;
    }

    /* Try to link the file copied earlier to the new file. */
    if (dstst->st_ino != 0) (void) unlink(dst);

    if ((linked= (link(olddst, dst) == 0)) && vflag)
	printf("ln %s ..\n", olddst);

    return linked;
}

int copy(const char *src, const char *dst, struct stat *srcst,
			struct stat *dstst)
/* Copy one file to another and copy (some of) the attributes. */
{
    char buf[CHUNK];
    int srcfd, dstfd;
    ssize_t n;

    assert(srcst->st_ino != 0);

    if (dstst->st_ino == 0) {
	/* The file doesn't exist yet. */

	if (!S_ISREG(srcst->st_mode)) {
	    /* Making a new mode 666 regular file. */
	    srcst->st_mode= (S_IFREG | 0666) & fc_mask;
	} else
	if (!pflag && conforming) {
	    /* Making a new file copying mode with umask applied. */
	    srcst->st_mode &= fc_mask;
	}
    } else {
	/* File exists, ask if ok to overwrite if '-i'. */

	if (iflag || (action == MOVE && !fflag && !writable(dstst))) {
	    fprintf(stderr, "Overwrite %s? (mode = %03o) ",
			dst, dstst->st_mode & 07777);
	    if (!affirmative()) return 0;
	}

	if (action == MOVE) {
	    /* Don't overwrite, remove first. */
	    if (unlink(dst) < 0 && errno != ENOENT) {
		report(dst);
		return 0;
	    }
	} else {
	    /* Overwrite. */
	    if (!pflag) {
		/* Keep the existing mode and ownership. */
		srcst->st_mode= dstst->st_mode;
		srcst->st_uid= dstst->st_uid;
		srcst->st_gid= dstst->st_gid;
	    }
	}
    }

    /* Keep the link structure if possible. */
    if (trylink(src, dst, srcst, dstst)) return 1;

    if ((srcfd= open(src, O_RDONLY)) < 0) {
	report(src);
	return 0;
    }

    dstfd= open(dst, O_WRONLY|O_CREAT|O_TRUNC, srcst->st_mode & 0777);
    if (dstfd < 0 && fflag && errno == EACCES) {
	/* Retry adding a "w" bit. */
	(void) chmod(dst, dstst->st_mode | S_IWUSR);
	dstfd= open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0);
    }
    if (dstfd < 0 && fflag && errno == EACCES) {
	/* Retry after trying to delete. */
	(void) unlink(dst);
	dstfd= open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0);
    }
    if (dstfd < 0) {
	report(dst);
	close(srcfd);
	return 0;
    }

    /* Get current parameters. */
    if (fstat(dstfd, dstst) < 0) {
	report(dst);
	close(srcfd);
	close(dstfd);
	return 0;
    }

    /* Copy the little bytes themselves. */
    while ((n= read(srcfd, buf, sizeof(buf))) > 0) {
	char *bp = buf;
	ssize_t r;

	while (n > 0 && (r= write(dstfd, bp, n)) > 0) {
	    bp += r;
	    n -= r;
	}
	if (r <= 0) {
	    if (r == 0) {
		fprintf(stderr,
		    "%s: Warning: EOF writing to %s\n",
		    prog_name, dst);
		break;
	    }
	    fatal(dst);
	}
    }

    if (n < 0) {
	report(src);
	close(srcfd);
	close(dstfd);
	return 0;
    }

    close(srcfd);
    close(dstfd);

    /* Copy the ownership. */
    if ((pflag || !conforming)
	&& S_ISREG(dstst->st_mode)
	&& (dstst->st_uid != srcst->st_uid
		|| dstst->st_gid != srcst->st_gid)
    ) {
	if (chmod(dst, 0) == 0) dstst->st_mode&= ~07777;
	if (chown(dst, srcst->st_uid, srcst->st_gid) < 0) {
	    if (errno != EPERM) {
		report(dst);
		return 0;
	    }
	} else {
	    dstst->st_uid= srcst->st_uid;
	    dstst->st_gid= srcst->st_gid;
	}
    }

    if (conforming && S_ISREG(dstst->st_mode)
	&& (dstst->st_uid != srcst->st_uid
		|| dstst->st_gid != srcst->st_gid)
    ) {
	/* Suid bits must be cleared in the holy name of
	 * security (and the assumed user stupidity).
	 */
	srcst->st_mode&= ~06000;
    }

    /* Copy the mode. */
    if (S_ISREG(dstst->st_mode) && dstst->st_mode != srcst->st_mode) {
	if (chmod(dst, srcst->st_mode) < 0) {
	    if (errno != EPERM) {
		report(dst);
		return 0;
	    }
	    fprintf(stderr, "%s: Can't change the mode of %s\n",
		prog_name, dst);
	}
    }

    /* Copy the file modification time. */
    if ((pflag || !conforming) && S_ISREG(dstst->st_mode)) {
	struct utimbuf ut;

	ut.actime= action == MOVE ? srcst->st_atime : time(nil);
	ut.modtime= srcst->st_mtime;
	if (utime(dst, &ut) < 0) {
	    if (errno != EPERM) {
		report(dst);
		return 0;
	    }
	    if (pflag) {
		fprintf(stderr,
		    "%s: Can't set the time of %s\n",
		    prog_name, dst);
	    }
	}
    }
    if (vflag) {
	printf(action == COPY ? "cp %s ..\n" : "mv %s ..\n", src);
    }
    return 1;
}

void copy1(const char *src, const char *dst, struct stat *srcst,
			    struct stat *dstst)
/* Inspect the source file and then copy it.  Treatment of symlinks and
 * special files is a bit complicated.  The filetype and link-structure are
 * ignored if (expand && !rflag), symlinks and link-structure are ignored
 * if (expand && rflag), everything is copied precisely if !expand.
 */
{
    int r, linked;

    assert(srcst->st_ino != 0);

    if (srcst->st_ino == dstst->st_ino && srcst->st_dev == dstst->st_dev) {
	fprintf(stderr, "%s: can't copy %s onto itself\n",
	    prog_name, src);
	ex_code= 1;
	return;
    }

    /* You can forget it if the destination is a directory. */
    if (dstst->st_ino != 0 && S_ISDIR(dstst->st_mode)) {
	errno= EISDIR;
	report(dst);
	return;
    }

    if (S_ISREG(srcst->st_mode) || (expand && !rflag)) {
	if (!copy(src, dst, srcst, dstst)) return;

	if (action == MOVE && unlink(src) < 0) {
	    report(src);
	    return;
	}
	return;
    }

    if (dstst->st_ino != 0) {
	if (iflag || (action == MOVE && !fflag && !writable(dstst))) {
	    fprintf(stderr, "Replace %s? (mode = %03o) ",
		dst, dstst->st_mode & 07777);
	    if (!affirmative()) return;
	}
	if (unlink(dst) < 0) {
	    report(dst);
	    return;
	}
	dstst->st_ino= 0;
    }

    /* Apply the file creation mask if so required. */
    if (!pflag && conforming) srcst->st_mode &= fc_mask;

    linked= 0;

    if (S_ISLNK(srcst->st_mode)) {
	char buf[1024+1];

	if ((r= readlink(src, buf, sizeof(buf)-1)) < 0) {
	    report(src);
	    return;
	}
	buf[r]= 0;
	r= symlink(buf, dst);
	if (vflag && r == 0)
	    printf("ln -s %s %s\n", buf, dst);
    } else
    if (trylink(src, dst, srcst, dstst)) {
	linked= 1;
	r= 0;
    } else
    if (S_ISFIFO(srcst->st_mode)) {
	r= mkfifo(dst, srcst->st_mode);
	if (vflag && r == 0)
	    printf("mkfifo %s\n", dst);
    } else
    if (S_ISBLK(srcst->st_mode) || S_ISCHR(srcst->st_mode)) {
	r= mknod(dst, srcst->st_mode, srcst->st_rdev);
	if (vflag && r == 0) {
	    printf("mknod %s %c %d %d\n",
		dst,
		S_ISBLK(srcst->st_mode) ? 'b' : 'c',
		(srcst->st_rdev >> 8) & 0xFF,
		(srcst->st_rdev >> 0) & 0xFF);
	}
    } else {
	fprintf(stderr, "%s: %s: odd filetype %5o (not copied)\n",
	    prog_name, src, srcst->st_mode);
	ex_code= 1;
	return;
    }

    if (r < 0 || lstat(dst, dstst) < 0) {
	report(dst);
	return;
    }

    if (action == MOVE && unlink(src) < 0) {
	report(src);
	(void) unlink(dst);	/* Don't want it twice. */
	return;
    }

    if (linked) return;

    if (S_ISLNK(srcst->st_mode)) return;

    /* Copy the ownership. */
    if ((pflag || !conforming)
	&& (dstst->st_uid != srcst->st_uid
		|| dstst->st_gid != srcst->st_gid)
    ) {
	if (chown(dst, srcst->st_uid, srcst->st_gid) < 0) {
	    if (errno != EPERM) {
		report(dst);
		return;
	    }
	}
    }

    /* Copy the file modification time. */
    if (pflag || !conforming) {
	struct utimbuf ut;

	ut.actime= action == MOVE ? srcst->st_atime : time(nil);
	ut.modtime= srcst->st_mtime;
	if (utime(dst, &ut) < 0) {
	    if (errno != EPERM) {
		report(dst);
		return;
	    }
	    fprintf(stderr, "%s: Can't set the time of %s\n",
		prog_name, dst);
	}
    }
}

void remove1(const char *src, const struct stat *srcst)
{
    if (iflag || (!fflag && !writable(srcst))) {
	fprintf(stderr, "Remove %s? (mode = %03o) ", src,
			srcst->st_mode & 07777);
	if (!affirmative()) return;
    }
    if (unlink(src) < 0) {
	report(src);
    } else {
	if (vflag) printf("rm %s\n", src);
    }
}

void link1(const char *src, const char *dst, const struct stat *srcst,
			    const struct stat *dstst)
{
    pathname_t sym;
    const char *p;

    if (dstst->st_ino != 0 && (iflag || fflag)) {
	if (srcst->st_ino == dstst->st_ino) {
	    if (fflag) return;
	    fprintf(stderr, "%s: Can't link %s onto itself\n",
		prog_name, src);
	    ex_code= 1;
	    return;
	}
	if (iflag) {
	    fprintf(stderr, "Remove %s? ", dst);
	    if (!affirmative()) return;
	}
	errno= EISDIR;
	if (S_ISDIR(dstst->st_mode) || unlink(dst) < 0) {
	    report(dst);
	    return;
	}
    }

    if (!sflag && !(rflag && S_ISLNK(srcst->st_mode)) && !(Sflag && xdev)) {
	/* A normal link. */
	if (link(src, dst) < 0) {
	    if (!Sflag || errno != EXDEV) {
		report2(src, dst);
		return;
	    }
	    /* Can't do a cross-device link, we have to symlink. */
	    xdev= 1;
	} else {
	    if (vflag) printf("ln %s..\n", src);
	    return;
	}
    }

    /* Do a symlink. */
    if (!rflag && !Sflag) {
	/* We can get away with a "don't care if it works" symlink. */
	if (symlink(src, dst) < 0) {
	    report(dst);
	    return;
	}
	if (vflag) printf("ln -s %s %s\n", src, dst);
	return;
    }

    /* If the source is a symlink then it is simply copied. */
    if (S_ISLNK(srcst->st_mode)) {
	int r;
	char buf[1024+1];

	if ((r= readlink(src, buf, sizeof(buf)-1)) < 0) {
	    report(src);
	    return;
	}
	buf[r]= 0;
	if (symlink(buf, dst) < 0) {
	    report(dst);
	    return;
	}
	if (vflag) printf("ln -s %s %s\n", buf, dst);
	return;
    }

    /* Make a symlink that has to work, i.e. we must be able to access the
     * source now, and the link must work.
     */
    if (dst[0] == '/' && src[0] != '/') {
	/* ln -[rsS] relative/path /full/path. */
	fprintf(stderr,
	    "%s: Symlinking %s to %s is too difficult for me to figure out\n",
	    prog_name, src, dst);
	exit(1);
    }

    /* Count the number of subdirectories in the destination file and
     * add one '..' for each.
     */
    path_init(&sym);
    if (src[0] != '/') {
	p= dst;
	while (*p != 0) {
	    if (p[0] == '.') {
		if (p[1] == '/' || p[1] == 0) {
		    /* A "." component; skip. */
		    do p++; while (*p == '/');
		    continue;
		} else
		if (p[1] == '.' && (p[2] == '/' || p[2] == 0)) {
		    /* A ".." component; oops. */
		    switch (path_length(&sym)) {
		    case 0:
			fprintf(stderr,
	    "%s: Symlinking %s to %s is too difficult for me to figure out\n",
			    prog_name, src, dst);
			exit(1);
		    case 2:
			path_trunc(&sym, 0);
			break;
		    default:
			path_trunc(&sym, path_length(&sym) - 3);
		    }
		    p++;
		    do p++; while (*p == '/');
		    continue;
		}
	    }
	    while (*p != 0 && *p != '/') p++;
	    while (*p == '/') p++;
	    if (*p == 0) break;
	    path_add(&sym, "..");
	}
    }
    path_add(&sym, src);

    if (symlink(path_name(&sym), dst) < 0) {
	report(dst);
    } else {
	if (vflag) printf("ln -s %s %s\n", path_name(&sym), dst);
    }
    path_drop(&sym);
}

typedef struct entrylist {
    struct entrylist	*next;
    char			*name;
} entrylist_t;

int eat_dir(const char *dir, entrylist_t **dlist)
/* Make a linked list of all the names in a directory. */
{
    DIR *dp;
    struct dirent *entry;

    if ((dp= opendir(dir)) == nil) return 0;

    while ((entry= readdir(dp)) != nil) {
	if (strcmp(entry->d_name, ".") == 0) continue;
	if (strcmp(entry->d_name, "..") == 0) continue;

	*dlist= allocate(nil, sizeof(**dlist));
	(*dlist)->name= allocate(nil, strlen(entry->d_name)+1);
	strcpy((*dlist)->name, entry->d_name);
	dlist= &(*dlist)->next;
    }
    closedir(dp);
    *dlist= nil;
    return 1;
}

void chop_dlist(entrylist_t **dlist)
/* Chop an entry of a name list. */
{
    entrylist_t *junk= *dlist;

    *dlist= junk->next;
    deallocate(junk->name);
    deallocate(junk);
}

void drop_dlist(entrylist_t *dlist)
/* Get rid of a whole list. */
{
    while (dlist != nil) chop_dlist(&dlist);
}

void do1(pathname_t *src, pathname_t *dst, int depth)
/* Perform the appropriate action on a source and destination file. */
{
    size_t slashsrc, slashdst;
    struct stat srcst, dstst;
    entrylist_t *dlist;
    static ino_t topdst_ino;
    static dev_t topdst_dev;
    static dev_t topsrc_dev;

#if DEBUG
    if (vflag && depth == 0) {
	char flags[100], *pf= flags;

	if (pflag) *pf++= 'p';
	if (iflag) *pf++= 'i';
	if (fflag) *pf++= 'f';
	if (sflag) *pf++= 's';
	if (Sflag) *pf++= 'S';
	if (mflag) *pf++= 'm';
	if (rflag) *pf++= 'r';
	if (vflag) *pf++= 'v';
	if (xflag) *pf++= 'x';
	if (expand) *pf++= 'L';
	if (conforming) *pf++= 'C';
	*pf= 0;
	printf(": %s -%s %s %s\n", prog_name, flags,
		    path_name(src), path_name(dst));
    }
#endif

    /* st_ino == 0 if not stat()'ed yet, or nonexistent. */
    srcst.st_ino= 0;
    dstst.st_ino= 0;

    if (action != LINK || !sflag || rflag) {
	/* Source must exist unless symlinking. */
	if ((expand ? stat : lstat)(path_name(src), &srcst) < 0) {
	    report(path_name(src));
	    return;
	}
    }

    if (depth == 0) {
	/* First call: Not cross-device yet, first dst not seen yet,
	 * remember top device number.
	 */
	xdev= 0;
	topdst_ino= 0;
	topsrc_dev= srcst.st_dev;
    }

    /* Inspect the intended destination unless removing. */
    if (action != REMOVE) {
	if ((expand ? stat : lstat)(path_name(dst), &dstst) < 0) {
	    if (errno != ENOENT) {
		report(path_name(dst));
		return;
	    }
	}
    }

    if (action == MOVE && !xdev) {
	if (dstst.st_ino != 0 && srcst.st_dev != dstst.st_dev) {
	    /* It's a cross-device rename, i.e. copy and remove. */
	    xdev= 1;
	} else
	if (!mflag || dstst.st_ino == 0 || !S_ISDIR(dstst.st_mode)) {
	    /* Try to simply rename the file (not merging trees). */

	    if (srcst.st_ino == dstst.st_ino) {
		fprintf(stderr,
		    "%s: Can't move %s onto itself\n",
		    prog_name, path_name(src));
		ex_code= 1;
		return;
	    }

	    if (dstst.st_ino != 0) {
		if (iflag || (!fflag && !writable(&dstst))) {
		    fprintf(stderr,
			"Replace %s? (mode = %03o) ",
			path_name(dst),
			dstst.st_mode & 07777);
		    if (!affirmative()) return;
		}
		if (!S_ISDIR(dstst.st_mode))
		    (void) unlink(path_name(dst));
	    }

	    if (rename(path_name(src), path_name(dst)) == 0) {
		/* Success. */
		if (vflag) {
		    printf("mv %s %s\n", path_name(src),
			    path_name(dst));
		}
		return;
	    }
	    if (errno == EXDEV) {
		xdev= 1;
	    } else {
		report2(path_name(src), path_name(dst));
		return;
	    }
	}
    }

    if (srcst.st_ino == 0 || !S_ISDIR(srcst.st_mode)) {
	/* Copy/move/remove/link a single file. */
	switch (action) {
	case COPY:
	case MOVE:
	    copy1(path_name(src), path_name(dst), &srcst, &dstst);
	    break;
	case REMOVE:
	    remove1(path_name(src), &srcst);
	    break;
	case LINK:
	    link1(path_name(src), path_name(dst), &srcst, &dstst);
	    break;
	}
	return;
    }

    /* Recursively copy/move/remove/link a directory if -r or -R. */
    if (!rflag) {
	errno= EISDIR;
	report(path_name(src));
	return;
    }

    /* Ok to remove contents of dir? */
    if (action == REMOVE) {
	if (xflag && topsrc_dev != srcst.st_dev) {
	    /* Don't recurse past a mount point. */
	    return;
	}
	if (iflag) {
	    fprintf(stderr, "Remove contents of %s? ", path_name(src));
	    if (!affirmative()) return;
	}
    }

    /* Gather the names in the source directory. */
    if (!eat_dir(path_name(src), &dlist)) {
	report(path_name(src));
	return;
    }

    /* Check/create the target directory. */
    if (action != REMOVE && dstst.st_ino != 0 && !S_ISDIR(dstst.st_mode)) {
	if (action != MOVE && !fflag) {
	    errno= ENOTDIR;
	    report(path_name(dst));
	    return;
	}
	if (iflag) {
	    fprintf(stderr, "Replace %s? ", path_name(dst));
	    if (!affirmative()) {
		drop_dlist(dlist);
		return;
	    }
	}
	if (unlink(path_name(dst)) < 0) {
	    report(path_name(dst));
	    drop_dlist(dlist);
	    return;
	}
	dstst.st_ino= 0;
    }

    if (action != REMOVE) {
	if (dstst.st_ino == 0) {
	    /* Create a new target directory. */
	    if (!pflag && conforming) srcst.st_mode&= fc_mask;

	    if (mkdir(path_name(dst), srcst.st_mode | S_IRWXU) < 0
		    || stat(path_name(dst), &dstst) < 0) {
		report(path_name(dst));
		drop_dlist(dlist);
		return;
	    }
	    if (vflag) printf("mkdir %s\n", path_name(dst));
	} else {
	    /* Target directory already exists. */
	    if (action == MOVE && !mflag) {
		errno= EEXIST;
		report(path_name(dst));
		drop_dlist(dlist);
		return;
	    }
	    if (!pflag) {
		/* Keep the existing attributes. */
		srcst.st_mode= dstst.st_mode;
		srcst.st_uid= dstst.st_uid;
		srcst.st_gid= dstst.st_gid;
		srcst.st_mtime= dstst.st_mtime;
	    }
	}

	if (topdst_ino == 0) {
	    /* Remember the top destination. */
	    topdst_dev= dstst.st_dev;
	    topdst_ino= dstst.st_ino;
	}

	if (srcst.st_ino == topdst_ino && srcst.st_dev == topdst_dev) {
	    /* E.g. cp -r /shallow /shallow/deep. */
	    fprintf(stderr,
		"%s%s %s/ %s/: infinite recursion avoided\n",
		prog_name, action != MOVE ? " -r" : "",
		path_name(src), path_name(dst));
	    drop_dlist(dlist);
	    return;
	}

	if (xflag && topsrc_dev != srcst.st_dev) {
	    /* Don't recurse past a mount point. */
	    drop_dlist(dlist);
	    return;
	}
    }

    /* Go down. */
    slashsrc= path_length(src);
    slashdst= path_length(dst);

    while (dlist != nil) {
	path_add(src, dlist->name);
	if (action != REMOVE) path_add(dst, dlist->name);

	do1(src, dst, depth+1);

	path_trunc(src, slashsrc);
	path_trunc(dst, slashdst);
	chop_dlist(&dlist);
    }

    if (action == MOVE || action == REMOVE) {
	/* The contents of the source directory should have
	 * been (re)moved above.  Get rid of the empty dir.
	 */
	if (action == REMOVE && iflag) {
	    fprintf(stderr, "Remove directory %s? ",
			    path_name(src));
	    if (!affirmative()) return;
	}
	if (rmdir(path_name(src)) < 0) {
	    if (errno != ENOTEMPTY) report(path_name(src));
	    return;
	}
	if (vflag) printf("rmdir %s\n", path_name(src));
    }

    if (action != REMOVE) {
	/* Set the attributes of a new directory. */
	struct utimbuf ut;

	/* Copy the ownership. */
	if ((pflag || !conforming)
	    && (dstst.st_uid != srcst.st_uid
		|| dstst.st_gid != srcst.st_gid)
	) {
	    if (chown(path_name(dst), srcst.st_uid,
			    srcst.st_gid) < 0) {
		if (errno != EPERM) {
		    report(path_name(dst));
		    return;
		}
	    }
	}

	/* Copy the mode. */
	if (dstst.st_mode != srcst.st_mode) {
	    if (chmod(path_name(dst), srcst.st_mode) < 0) {
		report(path_name(dst));
		return;
	    }
	}

	/* Copy the file modification time. */
	if (dstst.st_mtime != srcst.st_mtime) {
	    ut.actime= action == MOVE ? srcst.st_atime : time(nil);
	    ut.modtime= srcst.st_mtime;
	    if (utime(path_name(dst), &ut) < 0) {
		if (errno != EPERM) {
		    report(path_name(dst));
		    return;
		}
		fprintf(stderr,
		    "%s: Can't set the time of %s\n",
		    prog_name, path_name(dst));
	    }
	}
    }
}

void usage(void)
{
    char *flags1, *flags2;

    switch (identity) {
    case CP:
	flags1= "pifsmrRvx";
	flags2= "pifsrRvx";
	break;
    case MV:
	flags1= "ifsmvx";
	flags2= "ifsvx";
	break;
    case RM:
	fprintf(stderr, "Usage: rm [-ifrRvx] file ...\n");
	exit(1);
    case LN:
	flags1= "ifsSmrRvx";
	flags2= "ifsSrRvx";
	break;
    case CPDIR:
	flags1= "ifvx";
	flags2= nil;
	break;
    case CLONE:
	flags1= "ifsSvx";
	flags2= nil;
	break;
    }
    fprintf(stderr, "Usage: %s [-%s] file1 file2\n", prog_name, flags1);
  if (flags2 != nil)
    fprintf(stderr, "       %s [-%s] file ... dir\n", prog_name, flags2);
    exit(1);
}

int main(int argc, char **argv)
{
    int i;
    char *flags;
    struct stat st;
    pathname_t src, dst;
    size_t slash;

#if DEBUG >= 3
    /* The first argument is the call name while debugging. */
    if (argc < 2) exit(-1);
    argv++;
    argc--;
#endif
#if DEBUG
    vflag= isatty(1);
#endif

    /* Call name of this program. */
    prog_name= basename(argv[0]);

    /* Required action. */
    if (strcmp(prog_name, "cp") == 0) {
	identity= CP;
	action= COPY;
	flags= "pifsmrRvx";
	expand= 1;
    } else
    if (strcmp(prog_name, "mv") == 0) {
	identity= MV;
	action= MOVE;
	flags= "ifsmvx";
	rflag= pflag= 1;
    } else
    if (strcmp(prog_name, "rm") == 0) {
	identity= RM;
	action= REMOVE;
	flags= "ifrRvx";
    } else
    if (strcmp(prog_name, "ln") == 0) {
	identity= LN;
	action= LINK;
	flags= "ifsSmrRvx";
    } else
    if (strcmp(prog_name, "cpdir") == 0) {
	identity= CPDIR;
	action= COPY;
	flags= "pifsmrRvx";
	rflag= mflag= pflag= 1;
	conforming= 0;
    } else
    if (strcmp(prog_name, "clone") == 0) {
	identity= CLONE;
	action= LINK;
	flags= "ifsSmrRvx";
	rflag= mflag= fflag= 1;
    } else {
	fprintf(stderr,
	    "%s: Identity crisis, not called cp, mv, rm, ln, cpdir, or clone\n",
	    prog_name);
	exit(1);
    }

    /* Who am I?, where am I?, how protective am I? */
    uid= geteuid();
    gid= getegid();
    istty= isatty(0);
    fc_mask= ~umask(0);

    /* Gather flags. */
    i= 1;
    while (i < argc && argv[i][0] == '-') {
	char *opt= argv[i++] + 1;

	if (opt[0] == '-' && opt[1] == 0) break;	/* -- */

	while (*opt != 0) {
	    /* Flag supported? */
	    if (strchr(flags, *opt) == nil) usage();

	    switch (*opt++) {
	    case 'p':
		pflag= 1;
		break;
	    case 'i':
		iflag= 1;
		if (action == MOVE) fflag= 0;
		break;
	    case 'f':
		fflag= 1;
		if (action == MOVE) iflag= 0;
		break;
	    case 's':
		if (action == LINK) {
		    sflag= 1;
		} else {
		    /* Forget about POSIX, do it right. */
		    conforming= 0;
		}
		break;
	    case 'S':
		Sflag= 1;
		break;
	    case 'm':
		mflag= 1;
		break;
	    case 'r':
		expand= 0;
		/*FALL THROUGH*/
	    case 'R':
		rflag= 1;
		break;
	    case 'v':
		vflag= 1;
		break;
	    case 'x':
		xflag= 1;
		break;
	    default:
		assert(0);
	    }
	}
    }

    switch (action) {
    case REMOVE:
	if (i == argc) {
	    if (fflag)
		exit(0);
	    usage();
	}
	break;
    case LINK:
	/* 'ln dir/file' is to be read as 'ln dir/file .'. */
	if ((argc - i) == 1 && action == LINK) argv[argc++]= ".";
	/*FALL THROUGH*/
    default:
	if ((argc - i) < 2) usage();
    }

    path_init(&src);
    path_init(&dst);

    if (action != REMOVE && !mflag
	&& stat(argv[argc-1], &st) >= 0 && S_ISDIR(st.st_mode)
    ) {
	/* The last argument is a directory, this means we have to
	 * throw the whole lot into this directory.  This is the
	 * Right Thing unless you use -r.
	 */
	path_add(&dst, argv[argc-1]);
	slash= path_length(&dst);

	do {
	    path_add(&src, argv[i]);
	    path_add(&dst, basename(argv[i]));

	    do1(&src, &dst, 0);

	    path_trunc(&src, 0);
	    path_trunc(&dst, slash);
	} while (++i < argc-1);
    } else
    if (action == REMOVE || (argc - i) == 2) {
	/* Just two files (or many files for rm). */
	do {
	    path_add(&src, argv[i]);
	    if (action != REMOVE) path_add(&dst, argv[i+1]);

	    do1(&src, &dst, 0);
	    path_trunc(&src, 0);
	} while (action == REMOVE && ++i < argc);
    } else {
	usage();
    }
    path_drop(&src);
    path_drop(&dst);

#if DEBUG
    if (nchunks != 0) {
	fprintf(stderr, "(%ld chunks of memory not freed)\n",
	    (long) nchunks);
    }
#endif
    exit(ex_code);
    return ex_code;
}
