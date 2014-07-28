/*	remsync 1.5 - remotely synchronize file trees	Author: Kees J. Bot
 *								10 Jun 1994
 */
#define nil 0
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <utime.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

#ifndef major
#define major(dev)	((int) ((dev) >> 8))
#define minor(dev)	((int) ((dev) & 0xFF))
#endif

#ifndef S_ISLNK
/* There were no symlinks in medieval times. */
#define S_ISLNK(mode)			(0)
#define lstat				stat
#define symlink(path1, path2)		(errno= ENOSYS, -1)
#define readlink(path, buf, len)	(errno= ENOSYS, -1)
#endif

int sflag;		/* Make state file. */
int dflag;		/* Make list of differences. */
int uflag;		/* Only update files with newer versions. */
int xflag;		/* Do not cross device boundaries. */
int Dflag;		/* Debug: Readable differences, no file contents. */
int vflag;		/* Verbose. */

#define NO_DEVICE	(-1)
dev_t xdev= NO_DEVICE;	/* The device that you should stay within. */

int excode= 0;		/* Exit(excode); */

#define BASE_INDENT	2	/* State file basic indent. */

void report(const char *label)
{
	fprintf(stderr, "remsync: %s: %s\n", label, strerror(errno));
	excode= 1;
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

void *allocate(void *mem, size_t size)
{
	if ((mem= mem == nil ? malloc(size) : realloc(mem, size)) == nil) {
		fprintf(stderr, "remsync: Out of memory: %s\n",
			strerror(errno));
		exit(1);
	}
	return mem;
}

void deallocate(void *mem)
{
	if (mem != nil) free(mem);
}

/* One needs to slowly forget two sets of objects: for the code that reads
 * the state file, and for the code that traverses trees.
 */
int keep;
#define KEEP_STATE	0
#define KEEP_TRAVERSE	1

void forget(void *mem)
/* Some objects must be deleted in time, but not just yet. */
{
	static void *death_row[2][50];
	static void **dp[2]= { death_row[0], death_row[1] };

	deallocate(*dp[keep]);
	*dp[keep]++= mem;
	if (dp[keep] == arraylimit(death_row[keep])) dp[keep]= death_row[keep];
}

char *copystr(const char *s)
{
	char *c= allocate(nil, (strlen(s) + 1) * sizeof(c[0]));
	strcpy(c, s);
	return c;
}

typedef struct pathname {
	char		*path;	/* The actual pathname. */
	size_t		idx;	/* Index for the terminating null byte. */
	size_t		lim;	/* Actual length of the path array. */
} pathname_t;

void path_init(pathname_t *pp)
/* Initialize a pathname to the null string. */
{
	pp->path= allocate(nil, (pp->lim= 16) * sizeof(pp->path[0]));
	pp->path[pp->idx= 0]= 0;
}

void path_add(pathname_t *pp, const char *name)
/* Add a component to a pathname. */
{
	size_t lim;
	char *p;
	int slash;

	lim= pp->idx + strlen(name) + 2;

	if (lim > pp->lim) {
		pp->lim= lim + lim/2;	/* add an extra 50% growing space. */
		pp->path= allocate(pp->path, pp->lim * sizeof(pp->path[0]));
	}

	p= pp->path + pp->idx;
	slash= (pp->idx > 0);
	if (pp->idx == 1 && p[-1] == '/') p--;

	while (*name != 0) {
		if (*name == '/') {
			slash= 1;
		} else {
			if (slash) { *p++ = '/'; slash= 0; }
			*p++= *name;
		}
		name++;
	}
	if (slash && p == pp->path) *p++= '/';
	*p = 0;
	pp->idx= p - pp->path;
}

void path_trunc(pathname_t *pp, size_t didx)
/* Delete part of a pathname to a remembered length. */
{
	pp->path[pp->idx= didx]= 0;
}

#if kept_for_comments_only

const char *path_name(const pathname_t *pp)
/* Return the actual name as a char array. */
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
	free(pp->path);
}
#endif

#define path_name(pp)		((const char *) (pp)->path)
#define path_length(pp)		((pp)->idx)
#define path_drop(pp)		free((void *) (pp)->path)

typedef struct namelist {	/* Obviously a list of names. */
	struct namelist	*next;
	char		*name;
} namelist_t;

char *rdlink(const char *link, off_t size)
/* Look where "link" points. */
{
	static char *path= nil;
	static size_t len= 0;
	size_t n;

	if (len <= size) {
		path= allocate(path, (len= size * 2) * sizeof(path[0]));
	}
	if ((n= readlink(link, path, len)) == -1) return nil;
	path[n]= 0;
	return path;
}

void sort(namelist_t **anl)
/* A stable mergesort disguised as line noise.  Must be called like this:
 *	if (L!=nil && L->next!=nil) sort(&L);
 */
{
	/* static */ namelist_t *nl1, **mid;  /* Need not be local */
	namelist_t *nl2;

	nl1= *(mid= &(*anl)->next);
	do {
		if ((nl1= nl1->next) == nil) break;
		mid= &(*mid)->next;
	} while ((nl1= nl1->next) != nil);

	nl2= *mid;
	*mid= nil;

	if ((*anl)->next != nil) sort(anl);
	if (nl2->next != nil) sort(&nl2);

	nl1= *anl;
	for (;;) {
		if (strcmp(nl1->name, nl2->name)<=0) {
			if ((nl1= *(anl= &nl1->next)) == nil) {
				*anl= nl2;
				break;
			}
		} else {
			*anl= nl2;
			nl2= *(anl= &nl2->next);
			*anl= nl1;
			if (nl2 == nil) break;
		}
	}
}

namelist_t *collect(const char *dir)
/* Return a sorted list of directory entries.  Returns null with errno != 0
 * on error.
 */
{
	namelist_t *names, **pn= &names;
	DIR *dp;
	struct dirent *entry;

	if ((dp= opendir(dir)) == nil) return nil;

	while ((entry= readdir(dp)) != nil) {
		if (entry->d_name[0] == '.'
			&& (entry->d_name[1] == 0
				|| (entry->d_name[1] == '.'
					&& entry->d_name[2] == 0))) {
			continue;
		}
		*pn= allocate(nil, sizeof(**pn));
		(*pn)->name= copystr(entry->d_name);
		pn= &(*pn)->next;
	}
	closedir(dp);
	*pn= nil;
	errno= 0;
	if (names != nil && names->next != nil) sort(&names);
	return names;
}

char *pop_name(namelist_t **names)
/* Return one name of a name list. */
{
	char *name;
	namelist_t *junk;

	junk= *names;
	*names= junk->next;
	name= junk->name;
	deallocate(junk);
	forget(name);
	return name;
}

typedef enum filetype {		/* The files we know about. */
	F_DIR,
	F_FILE,
	F_BLK,
	F_CHR,
	F_PIPE,
	F_LINK
} filetype_t;

typedef struct entry {		/* One file. */
	int		depth;		/* Depth in directory tree. */
	const char	*name;		/* Name of entry. */
	const char	*path;		/* Path name. */
	int		ignore;		/* Ignore this entry (errno number.) */
	unsigned long	fake_ino;	/* Fake inode number for hard links. */
	int		linked;		/* Is the file hard linked? */
	int		lastlink;	/* Is it the last link? */
	char		*link;		/* Where a (sym)link points to. */
	filetype_t	type;
	mode_t		mode;		/* Not unlike those in struct stat. */
	uid_t		uid;
	gid_t		gid;
	off_t		size;
	time_t		mtime;
	dev_t		rdev;
} entry_t;

void linked(entry_t *entry, struct stat *stp)
/* Return an "inode number" if a file could have links (link count > 1).
 * Also return a path to the first link if you see the file again.
 */
{
	static unsigned long new_fake_ino= 0;
	static struct links {
		struct links	*next;
		char		*path;
		ino_t		ino;
		dev_t		dev;
		nlink_t		nlink;
		unsigned long	fake_ino;
	} *links[1024];
	struct links **plp, *lp;

	entry->linked= entry->lastlink= 0;
	entry->fake_ino= 0;
	entry->link= nil;

	if (S_ISDIR(stp->st_mode) || stp->st_nlink < 2) return;

	plp= &links[stp->st_ino % arraysize(links)];
	while ((lp= *plp) != nil && (lp->ino != stp->st_ino
				|| lp->dev != stp->st_dev)) plp= &lp->next;

	if (lp == nil) {
		/* New file, store it with a new fake inode number. */
		*plp= lp= allocate(nil, sizeof(*lp));
		lp->next= nil;
		lp->path= copystr(entry->path);
		lp->ino= stp->st_ino;
		lp->dev= stp->st_dev;
		lp->nlink= stp->st_nlink;
		lp->fake_ino= ++new_fake_ino;
	} else {
		entry->link= lp->path;
		entry->linked= 1;
	}
	entry->fake_ino= lp->fake_ino;

	if (--lp->nlink == 0) {
		/* No need to remember this one, no more links coming. */
		*plp= lp->next;
		forget(lp->path);
		deallocate(lp);
		entry->lastlink= 1;
	}
}

char *tree;		/* Tree to work on. */
FILE *statefp;		/* State file. */
char *state_file;
FILE *difffp;		/* File of differences. */
char *diff_file;

entry_t *traverse(void)
/* Get one name from the directory tree. */
{
	static int depth;
	static pathname_t path;
	static entry_t entry;
	static namelist_t **entries;
	static size_t *trunc;
	static size_t deep;
	static namelist_t *newentries;
	struct stat st;

recurse:
	keep= KEEP_TRAVERSE;

	if (deep == 0) {
		/* Initialize for the root of the tree. */
		path_init(&path);
		path_add(&path, tree);
		entries= allocate(nil, 1 * sizeof(entries[0]));
		entries[0]= allocate(nil, sizeof(*entries[0]));
		entries[0]->next= nil;
		entries[0]->name= copystr("/");
		trunc= allocate(nil, 1 * sizeof(trunc[0]));
		trunc[0]= path_length(&path);
		deep= 1;
	} else
	if (newentries != nil) {
		/* Last entry was a directory, need to go down. */
		if (entry.ignore) {
			/* Ouch, it is to be ignored! */
			while (newentries != nil) (void) pop_name(&newentries);
			goto recurse;
		}
		if (++depth == deep) {
			deep++;
			entries= allocate(entries, deep * sizeof(entries[0]));
			trunc= allocate(trunc, deep * sizeof(trunc[0]));
		}
		entries[depth]= newentries;
		newentries= nil;
		trunc[depth]= path_length(&path);
	} else {
		/* Pop up out of emptied directories. */
		while (entries[depth] == nil) {
			if (depth == 0) return nil;	/* Back at the root. */

			/* Go up one level. */
			depth--;
		}
	}
	entry.name= pop_name(&entries[depth]);
	path_trunc(&path, trunc[depth]);
	path_add(&path, entry.name);
	if (depth == 0) {
		entry.path= "/";
	} else {
		entry.path= path_name(&path) + trunc[0];
		if (entry.path[0] == '/') entry.path++;
	}
	entry.depth= depth;
	entry.ignore= 0;

	if (lstat(path_name(&path), &st) < 0) {
		if (depth == 0 || errno != ENOENT) {
			/* Something wrong with this entry, complain about
			 * it and ignore it further.
			 */
			entry.ignore= errno;
			report(path_name(&path));
			return &entry;
		} else {
			/* Entry strangely nonexistent; simply continue. */
			goto recurse;
		}
	}

	/* Don't cross mountpoints if -x is set. */
	if (xflag) {
		if (xdev == NO_DEVICE) xdev= st.st_dev;
		if (st.st_dev != xdev) {
			/* Ignore the mountpoint. */
			entry.ignore= EXDEV;
			return &entry;
		}
	}

	entry.mode= st.st_mode & 07777;
	entry.uid= st.st_uid;
	entry.gid= st.st_gid;
	entry.size= st.st_size;
	entry.mtime= st.st_mtime;
	entry.rdev= st.st_rdev;

	linked(&entry, &st);

	if (S_ISDIR(st.st_mode)) {
		/* A directory. */
		entry.type= F_DIR;

		/* Gather directory entries for the next traverse. */
		if ((newentries= collect(path_name(&path))) == nil
							&& errno != 0) {
			entry.ignore= errno;
			report(path_name(&path));
		}
	} else
	if (S_ISREG(st.st_mode)) {
		/* A plain file. */
		entry.type= F_FILE;
	} else
	if (S_ISBLK(st.st_mode)) {
		/* A block special file. */
		entry.type= F_BLK;
	} else
	if (S_ISCHR(st.st_mode)) {
		/* A character special file. */
		entry.type= F_CHR;
	} else
	if (S_ISFIFO(st.st_mode)) {
		/* A named pipe. */
		entry.type= F_PIPE;
	} else
	if (S_ISLNK(st.st_mode)) {
		/* A symbolic link. */
		entry.type= F_LINK;
		if ((entry.link= rdlink(path_name(&path), st.st_size)) == nil) {
			entry.ignore= errno;
			report(path_name(&path));
		}
	} else {
		/* Unknown type of file. */
		entry.ignore= EINVAL;
	}
	return &entry;
}

void checkstate(void)
{
	if (ferror(statefp)) fatal(state_file);
}

void indent(int depth)
/* Provide indentation to show directory depth. */
{
	int n= BASE_INDENT * (depth - 1);

	while (n >= 8) {
		if (putc('\t', statefp) == EOF) checkstate();
		n-= 8;
	}
	while (n > 0) {
		if (putc(' ', statefp) == EOF) checkstate();
		n--;
	}
}

int print_name(FILE *fp, const char *name)
/* Encode a name. */
{
	const char *p;
	int c;

	for (p= name; (c= (unsigned char) *p) != 0; p++) {
		if (c <= ' ' || c == '\\') {
			fprintf(fp, "\\%03o", c);
			if (ferror(fp)) return 0;
		} else {
			if (putc(c, fp) == EOF) return 0;
		}
	}
	return 1;
}

void mkstatefile(void)
/* Make a state file out of the directory tree. */
{
	entry_t *entry;

	while ((entry= traverse()) != nil) {
		indent(entry->depth);
		if (!print_name(statefp, entry->name)) checkstate();

		if (entry->ignore) {
			fprintf(statefp, "\tignore (%s)\n",
				strerror(entry->ignore));
			checkstate();
			continue;
		}

		switch (entry->type) {
		case F_DIR:
			fprintf(statefp, "\td%03o %u %u",
				(unsigned) entry->mode,
				(unsigned) entry->uid, (unsigned) entry->gid);
			break;
		case F_FILE:
			fprintf(statefp, "\t%03o %u %u %lu %lu",
				(unsigned) entry->mode,
				(unsigned) entry->uid, (unsigned) entry->gid,
				(unsigned long) entry->size,
				(unsigned long) entry->mtime);
			break;
		case F_BLK:
		case F_CHR:
			fprintf(statefp, "\t%c%03o %u %u %x",
				entry->type == F_BLK ? 'b' : 'c',
				(unsigned) entry->mode,
				(unsigned) entry->uid, (unsigned) entry->gid,
				(unsigned) entry->rdev);
			break;
		case F_PIPE:
			fprintf(statefp, "\tp%03o %u %u",
				(unsigned) entry->mode,
				(unsigned) entry->uid, (unsigned) entry->gid);
			break;
		case F_LINK:
			fprintf(statefp, "\t-> ");
			checkstate();
			(void) print_name(statefp, entry->link);
			break;
		}
		checkstate();
		if (entry->fake_ino != 0)
			fprintf(statefp, " %lu", entry->fake_ino);
		if (entry->lastlink)
			fprintf(statefp, " last");
		if (fputc('\n', statefp) == EOF) checkstate();
	}
	fflush(statefp);
	checkstate();
}

char *read1line(FILE *fp)
/* Read one line from a file.  Return null on EOF or error. */
{
	static char *line;
	static size_t len;
	size_t idx;
	int c;

	if (len == 0) line= allocate(nil, (len= 16) * sizeof(line[0]));

	idx= 0;
	while ((c= getc(fp)) != EOF && c != '\n') {
		if (c < '\t') {
			/* Control characters are not possible. */
			fprintf(stderr,
				"remsync: control character in data file!\n");
			exit(1);
		}
		line[idx++]= c;
		if (idx == len) {
			line= allocate(line, (len*= 2) * sizeof(line[0]));
		}
	}
	if (c == EOF) {
		if (ferror(fp)) return nil;
		if (idx == 0) return nil;
	}
	line[idx]= 0;
	return line;
}

void getword(char **pline, char **parg, size_t *plen)
/* Get one word from a line, interpret octal escapes. */
{
	char *line= *pline;
	char *arg= *parg;
	size_t len= *plen;
	int i;
	int c;
	size_t idx;

	idx= 0;
	while ((c= *line) != 0 && c != ' ' && c != '\t') {
		line++;
		if (c == '\\') {
			c= 0;
			for (i= 0; i < 3; i++) {
				if ((unsigned) (*line - '0') >= 010) break;
				c= (c << 3) | (*line - '0');
				line++;
			}
		}
		arg[idx++]= c;
		if (idx == len) arg= allocate(arg, (len*= 2) * sizeof(arg[0]));
	}
	arg[idx]= 0;
	*pline= line;
	*parg= arg;
	*plen= len;
}

void splitline(char *line, char ***pargv, size_t *pargc)
/* Split a line into an array of words. */
{
	static char **argv;
	static size_t *lenv;
	static size_t len;
	size_t idx;

	idx= 0;
	for (;;) {
		while (*line == ' ' || *line == '\t') line++;

		if (*line == 0) break;

		if (idx == len) {
			len++;
			argv= allocate(argv, len * sizeof(argv[0]));
			lenv= allocate(lenv, len * sizeof(lenv[0]));
			argv[idx]= allocate(nil, 16 * sizeof(argv[idx][0]));
			lenv[idx]= 16;
		}
		getword(&line, &argv[idx], &lenv[idx]);
		idx++;
	}
	*pargv= argv;
	*pargc= idx;
}

int getattributes(entry_t *entry, int argc, char **argv)
/* Convert state or difference file info into file attributes. */
{
	int i;
	int attr;
#define A_MODE1		0x01	/* Some of these attributes follow the name */
#define A_MODE		0x02
#define	A_OWNER		0x04
#define A_SIZETIME	0x08
#define A_DEV		0x10
#define A_LINK		0x20

	switch (argv[0][0]) {
	case 'd':
		/* Directory. */
		entry->type= F_DIR;
		attr= A_MODE1 | A_OWNER;
		break;
	case 'b':
		/* Block device. */
		entry->type= F_BLK;
		attr= A_MODE1 | A_OWNER | A_DEV;
		break;
	case 'c':
		/* Character device. */
		entry->type= F_CHR;
		attr= A_MODE1 | A_OWNER | A_DEV;
		break;
	case 'p':
		/* Named pipe. */
		entry->type= F_PIPE;
		attr= A_MODE1 | A_OWNER;
		break;
	case '-':
		/* Symlink. */
		entry->type= F_LINK;
		attr= A_LINK;
		break;
	default:
		/* Normal file. */
		entry->type= F_FILE;
		attr= A_MODE | A_OWNER | A_SIZETIME;
	}

	if (attr & (A_MODE | A_MODE1)) {
		entry->mode= strtoul(argv[0] + (attr & A_MODE1), nil, 010);
	}
	i= 1;
	if (attr & A_OWNER) {
		if (i + 2 > argc) return 0;
		entry->uid= strtoul(argv[i++], nil, 10);
		entry->gid= strtoul(argv[i++], nil, 10);
	}
	if (attr & A_SIZETIME) {
		if (i + 2 > argc) return 0;
		entry->size= strtoul(argv[i++], nil, 10);
		entry->mtime= strtoul(argv[i++], nil, 10);
	}
	if (attr & A_DEV) {
		if (i + 1 > argc) return 0;
		entry->rdev= strtoul(argv[i++], nil, 0x10);
	}
	if (attr & A_LINK) {
		if (i + 1 > argc) return 0;
		entry->link= argv[i++];
	}
	entry->linked= entry->lastlink= 0;
	if (i < argc) {
		/* It has a fake inode number, so it is a hard link. */
		static struct links {	/* List of hard links. */
			struct links	*next;
			unsigned long	fake_ino;
			char		*path;
		} *links[1024];
		struct links **plp, *lp;
		unsigned long fake_ino;

		fake_ino= strtoul(argv[i++], nil, 10);

		plp= &links[fake_ino % arraysize(links)];
		while ((lp= *plp) != nil && lp->fake_ino != fake_ino)
			plp= &lp->next;

		if (lp == nil) {
			/* New link. */
			*plp= lp= allocate(nil, sizeof(*lp));
			lp->next= nil;
			lp->fake_ino= fake_ino;
			lp->path= copystr(entry->path);
		} else {
			/* Linked to. */
			entry->link= lp->path;
			entry->linked= 1;
		}

		if (i < argc) {
			if (strcmp(argv[i++], "last") != 0) return 0;

			/* Last hard link of a file. */
			forget(lp->path);
			*plp= lp->next;
			deallocate(lp);
			entry->lastlink= 1;
		}
	}
	if (i != argc) return 0;
	return 1;
}

void state_syntax(off_t line)
{
	fprintf(stderr, "remsync: %s: syntax error on line %lu\n",
		state_file, (unsigned long) line);
	exit(1);
}

entry_t *readstate(void)
/* Read one entry from the state file. */
{
	static entry_t entry;
	static pathname_t path;
	static size_t *trunc;
	static size_t trunc_len;
	static int base_indent;
	char *line;
	char **argv;
	size_t argc;
	static off_t lineno;
	int indent, depth;

recurse:
	keep= KEEP_STATE;

	if (feof(statefp) || (line= read1line(statefp)) == nil) {
		checkstate();
		return nil;
	}
	lineno++;

	/* How far is this entry indented? */
	indent= 0;
	while (*line != 0) {
		if (*line == ' ') indent++;
		else
		if (*line == '\t') indent= (indent + 8) & ~7;
		else
			break;
		line++;
	}
	if (indent > 0 && base_indent == 0) base_indent= indent;
	depth= (base_indent == 0 ? 0 : indent / base_indent) + 1;

	if (entry.ignore && depth > entry.depth) {
		/* If the old directory is ignored, then so are its entries. */
		goto recurse;
	}
	entry.depth= depth;

	splitline(line, &argv, &argc);
	if (argc < 2) state_syntax(lineno);

	if (trunc == nil) {
		/* The root of the tree, initialize path. */
		if (argv[0][0] != '/') state_syntax(lineno);
		path_init(&path);
		path_add(&path, "/");
		trunc= allocate(nil, (trunc_len= 16) * sizeof(trunc[0]));

		/* The root has depth 0. */
		entry.depth= 0;
		trunc[0]= 0;
	} else {
		if (entry.depth > trunc_len) {
			trunc= allocate(trunc,
					(trunc_len*= 2) * sizeof(trunc[0]));
		}
		path_trunc(&path, trunc[entry.depth - 1]);
		path_add(&path, argv[0]);
		trunc[entry.depth]= path_length(&path);
	}

	entry.path= path_name(&path);
	entry.name= argv[0];
	entry.link= nil;
	if ((entry.ignore= strcmp(argv[1], "ignore") == 0)) {
		return &entry;
	}
	if (!getattributes(&entry, argc - 1, argv + 1)) state_syntax(lineno);
	return &entry;
}

void checkdiff(void)
{
	if (ferror(difffp)) fatal(diff_file);
}

enum { DELETE, REPLACE, COPY, SIMILAR, EQUAL, ADD }
compare(entry_t *remote, entry_t *local)
/* Compare the local and remote entries and tell what need to be done. */
{
	int cmp;

	/* Surplus entries? */
	if (local == nil) return DELETE;
	if (remote == nil) return ADD;

	/* Extra directory entries? */
	if (remote->depth > local->depth) return DELETE;
	if (local->depth > remote->depth) return ADD;

	/* Compare names. */
	cmp= strcmp(remote->name, local->name);
	if (cmp < 0) return DELETE;
	if (cmp > 0) return ADD;

	/* The files have the same name.  Ignore one, ignore the other. */
	if (remote->ignore || local->ignore) {
		remote->ignore= local->ignore= 1;
		return EQUAL;
	}

	/* Reasons for replacement? */
	if (remote->type != local->type) return REPLACE;

	/* Should be hard linked to the same file. */
	if (remote->linked || local->linked) {
		if (!remote->linked || !local->linked) return REPLACE;
		if (strcmp(remote->link, local->link) != 0) return REPLACE;
	}

	switch (remote->type) {
	case F_FILE:
		if (uflag) {
			if (remote->mtime < local->mtime) return COPY;
		} else {
			if (remote->size != local->size
					|| remote->mtime != local->mtime)
				return COPY;
		}
		goto check_modes;
	case F_BLK:
	case F_CHR:
		if (remote->rdev != local->rdev) return REPLACE;
		goto check_modes;
	case F_DIR:
	case F_PIPE:
	check_modes:
		if (remote->mode != local->mode
			|| remote->uid != local->uid
			|| remote->gid != local->gid) return SIMILAR;
		break;
	case F_LINK:
		if (strcmp(remote->link, local->link) != 0) return REPLACE;
		break;
	}
	return EQUAL;
}

void delete(entry_t *old)
/* Emit an instruction to remove an entry. */
{
	if (old->ignore) return;
	if (uflag) return;

	fprintf(difffp, "rm ");
	checkdiff();
	if (!print_name(difffp, old->path)) checkdiff();
	if (putc('\n', difffp) == EOF) checkdiff();
	if (vflag) fprintf(stderr, "rm %s\n", old->path);
}

void change_modes(entry_t *old, entry_t *new)
/* Emit an instruction to change the attributes of an entry. */
{
	if (new->ignore) return;

	fprintf(difffp, "chmod ");
	checkdiff();
	if (!print_name(difffp, new->path)) checkdiff();
	fprintf(difffp, " %03o %u %u\n",
		(unsigned) new->mode,
		(unsigned) new->uid, (unsigned) new->gid);
	checkdiff();
	if (vflag && old->mode != new->mode) {
		fprintf(stderr, "chmod %s %03o %u %u\n",
			new->path,
			(unsigned) new->mode,
			(unsigned) new->uid, (unsigned) new->gid);
	}
}

int cat(int f, off_t size)
/* Include the contents of a file in the differences file. */
{
	ssize_t n;
	unsigned char buf[1024 << sizeof(int)];
	unsigned char *p;
	int c;

	if (Dflag) return 1;	/* Debug: Don't need the file contents. */

	while ((n= read(f, buf, sizeof(buf))) > 0) {
		p= buf;
		do {
			if (size == 0) {
				/* File suddenly larger. */
				errno= EINVAL;
				return 0;
			}
			c= *p++;
			if (putc(c, difffp) == EOF) checkdiff();
			size--;
		} while (--n != 0);
	}
	if (size > 0) {
		int err= errno;

		/* File somehow shrunk, pad it out. */
		do {
			if (putc(0, difffp) == EOF) checkdiff();
		} while (--size != 0);
		errno= n == 0 ? EINVAL : err;
		n= -1;
	}
	return n == 0;
}

void add(entry_t *old, entry_t *new)
/* Emit an instruction to add an entry. */
{
	pathname_t file;
	int f;

	if (new->ignore) return;

	if (new->linked) {
		/* This file is to be a hard link to an existing file. */
		fprintf(difffp, "ln ");
		checkdiff();
		if (!print_name(difffp, new->link)) checkdiff();
		if (fputc(' ', difffp) == EOF) checkdiff();
		if (!print_name(difffp, new->path)) checkdiff();
		if (fputc('\n', difffp) == EOF) checkdiff();
		if (vflag) {
			fprintf(stderr, "ln %s %s\n", new->link, new->path);
		}
		return;
	}

	/* Add some other type of file. */
	fprintf(difffp, "add ");
	checkdiff();
	if (!print_name(difffp, new->path)) checkdiff();

	switch (new->type) {
	case F_DIR:
		fprintf(difffp, " d%03o %u %u\n",
			(unsigned) new->mode,
			(unsigned) new->uid, (unsigned) new->gid);
		if (vflag) fprintf(stderr, "mkdir %s\n", new->path);
		break;
	case F_FILE:
		path_init(&file);
		path_add(&file, tree);
		path_add(&file, new->path);
		if ((f= open(path_name(&file), O_RDONLY)) < 0) {
			report(path_name(&file));
			path_drop(&file);
			fprintf(difffp, " ignore\n");
			break;
		}
		fprintf(difffp, " %03o %u %u %lu %lu\n",
			(unsigned) new->mode,
			(unsigned) new->uid, (unsigned) new->gid,
			(unsigned long) new->size,
			(unsigned long) new->mtime);
		checkdiff();
		if (!cat(f, new->size)) {
			int err= errno;
			report(path_name(&file));
			fprintf(difffp, "old ");
			checkdiff();
			print_name(difffp, err == EINVAL
				? "File changed when copied" : strerror(err));
			fputc('\n', difffp);
			checkdiff();
		} else {
			if (vflag) {
				fprintf(stderr, "%s %s\n",
					old == nil ? "add" :
						old->mtime > new->mtime ?
							"restore" : "update",
					new->path);
			}
		}
		close(f);
		path_drop(&file);
		break;
	case F_BLK:
	case F_CHR:
		fprintf(difffp, " %c%03o %u %u %lx\n",
			new->type == F_BLK ? 'b' : 'c',
			(unsigned) new->mode,
			(unsigned) new->uid, (unsigned) new->gid,
			(unsigned long) new->rdev);
		if (vflag) fprintf(stderr, "mknod %s\n", new->path);
		break;
	case F_PIPE:
		fprintf(difffp, " p%03o %u %u\n",
			(unsigned) new->mode,
			(unsigned) new->uid, (unsigned) new->gid);
		if (vflag) fprintf(stderr, "mkfifo %s\n", new->path);
		break;
	case F_LINK:
		fprintf(difffp, " -> ");
		checkdiff();
		(void) print_name(difffp, new->link);
		checkdiff();
		fputc('\n', difffp);
		if (vflag) {
			fprintf(stderr, "ln -s %s %s\n", new->link, new->path);
		}
		break;
	}
	checkdiff();
}

void mkdifferences(void)
{
	entry_t *remote;
	entry_t *local;

	remote= readstate();
	local= traverse();

	while (remote != nil || local != nil) {
		switch (compare(remote, local)) {
		case DELETE:
			/* Remove the remote file. */
			delete(remote);
			remote->ignore= 1;
			remote= readstate();
			break;
		case REPLACE:
			/* Replace the remote file with the local one. */
			if (remote->type == F_FILE && local->type == F_FILE
							&& !local->linked) {
				/* Don't overwrite, remove first. */
				delete(remote);
			}
			/*FALL THROUGH*/
		case COPY:
			/* Overwrite the remote file with the local one. */
			add(remote, local);
			remote->ignore= 1;
			goto skip2;
		case SIMILAR:
			/* About the same, but the attributes need changing. */
			change_modes(remote, local);
			goto skip2;
		case EQUAL:
		skip2:
			/* Skip two files. */
			remote= readstate();
			local= traverse();
			break;
		case ADD:
			/* Add the local file. */
			add(nil, local);
			local= traverse();
			break;
		}
	}
	fprintf(difffp, "end\n");
	fflush(difffp);
	checkdiff();
}

void apply_remove(pathname_t *pp)
/* Remove an obsolete file. */
{
	struct stat st;

	if (lstat(path_name(pp), &st) < 0) {
		if (errno != ENOENT) report(path_name(pp));
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		/* Recursively delete directories. */
		size_t len;
		namelist_t *entries;

		if ((entries= collect(path_name(pp))) == nil && errno != 0) {
			report(path_name(pp));
			return;
		}
		len= path_length(pp);

		while (entries != nil) {
			path_add(pp, pop_name(&entries));
			apply_remove(pp);
			path_trunc(pp, len);
		}
		if (rmdir(path_name(pp)) < 0) {
			report(path_name(pp));
			return;
		}
		if (vflag) fprintf(stderr, "rmdir %s\n", path_name(pp));
	} else {
		/* Some other type of file. */
		if (unlink(path_name(pp)) < 0) {
			report(path_name(pp));
			return;
		}
		if (vflag) fprintf(stderr, "rm %s\n", path_name(pp));
	}
}

void apply_mkold(const char *file, const char *err)
/* Make a file very old.  (An error occurred when it was added.) */
{
	struct utimbuf utb;

	utb.actime= utb.modtime= 0;
	if (utime(file, &utb) < 0) {
		report(file);
		return;
	}
	fprintf(stderr, "made %s look old", file);
	fprintf(stderr, err == nil ? "\n" : " due to a remote problem: %s\n",
								err);
}

void apply_chmod(const char *file, mode_t mode, uid_t uid, gid_t gid, int talk)
/* Change mode and ownership. */
{
	struct stat st;

	if (lstat(file, &st) < 0) {
		report(file);
		return;
	}
	if ((st.st_mode & 07777) != mode) {
		if (chmod(file, mode) < 0) {
			report(file);
			return;
		}
		if (vflag && talk) {
			fprintf(stderr, "chmod %03o %s\n",
						(unsigned) mode, file);
		}
	}
	if (st.st_uid != uid || st.st_gid != gid) {
		if (chown(file, uid, gid) < 0) {
			if (errno != EPERM) report(file);
			return;
		}
		if (vflag && talk) {
			fprintf(stderr, "chown %u:%u %s\n",
				(unsigned) uid, (unsigned) gid, file);
		}
	}
}

void apply_add(pathname_t *pp, entry_t *entry)
/* Add or replace a file. */
{
	const char *file;
	off_t size;
	int f;
	unsigned char buf[1024 << sizeof(int)];
	unsigned char *p;
	int c;
	int dirty;
	struct stat st;
	struct utimbuf utb;

	if (entry->ignore) return;

	if (lstat(path_name(pp), &st) >= 0 && (entry->type != F_FILE
					|| !S_ISREG(st.st_mode))) {
		apply_remove(pp);
	}

	file= path_name(pp);

	switch (entry->type) {
	case F_DIR:
		if (mkdir(file, entry->mode) < 0) {
			report(file);
			return;
		}
		if (vflag) fprintf(stderr, "mkdir %s\n", file);
		break;
	case F_FILE:
		size= entry->size;

		f= -1;
		st.st_mode= 0;
		if (lstat(file, &st) < 0 || S_ISREG(st.st_mode)) {
			f= open(file, O_WRONLY | O_CREAT | O_TRUNC,
						entry->mode);
			if (f < 0) {
				(void) chmod(file, entry->mode | 0200);
				f= open(file, O_WRONLY | O_CREAT | O_TRUNC,
						entry->mode);
			}
			if (f < 0) {
				(void) unlink(file);
				f= open(file, O_WRONLY | O_CREAT | O_TRUNC,
						entry->mode);
			}
			if (f < 0) report(file);
		}
		dirty= (f >= 0);
		p= buf;
		while (size > 0 && (c= getc(difffp)) != EOF) {
			size--;
			*p++= c;
			if (p == arraylimit(buf) || size == 0) {
				if (f >= 0 && write(f, buf, p - buf) < 0) {
					report(file);
					close(f);
					f= -1;
				}
				p= buf;
			}
		}
		if (size > 0) {
			if (ferror(difffp)) report(diff_file);
			if (feof(difffp)) {
				fprintf(stderr, "remspec: %s: premature EOF\n",
					diff_file);
			}
			if (dirty) apply_mkold(file, nil);
			exit(1);
		}
		if (f < 0) {
			if (dirty) apply_mkold(file, nil);
			return;
		}
		close(f);
		if (vflag) {
			fprintf(stderr, st.st_mode == 0 ? "add %s\n"
				: entry->mtime >= st.st_mtime
					? "update %s\n" : "restore %s\n", file);
		}
		utb.actime= time(nil);
		utb.modtime= entry->mtime;
		if (utime(file, &utb) < 0) report(file);
		break;
	case F_BLK:
		if (mknod(file, S_IFBLK | entry->mode, entry->rdev) < 0) {
			report(file);
			return;
		}
		if (vflag) {
			fprintf(stderr, "mknod %s b %d %d\n", file,
				major(entry->rdev), minor(entry->rdev));
		}
		break;
	case F_CHR:
		if (mknod(file, S_IFCHR | entry->mode, entry->rdev) < 0) {
			report(file);
			return;
		}
		if (vflag) {
			fprintf(stderr, "mknod %s c %d %d\n", file,
				major(entry->rdev), minor(entry->rdev));
		}
		break;
	case F_PIPE:
		if (mknod(file, S_IFIFO | entry->mode, 0) < 0) {
			report(file);
			return;
		}
		if (vflag) fprintf(stderr, "mknod %s p\n", file);
		break;
	case F_LINK:
		if (symlink(entry->link, file) < 0) {
			report(file);
			return;
		}
		if (vflag) fprintf(stderr, "ln -s %s %s\n", entry->link, file);
		return;
	}
	apply_chmod(file, entry->mode, entry->uid, entry->gid, 0);
}

void apply_link(const char *file, pathname_t *pp)
/* Hard link *pp to file. */
{
	struct stat st1, st2;

	if (lstat(file, &st1) < 0) {
		report(file);
		return;
	}
	if (lstat(path_name(pp), &st2) >= 0) {
		if (st1.st_ino == st2.st_ino && st1.st_dev == st2.st_dev)
			return;
		apply_remove(pp);
		if (lstat(path_name(pp), &st2) >= 0) return;
	}
	if (link(file, path_name(pp)) < 0) {
		fprintf(stderr, "remsync: ln %s %s: %s\n", file, path_name(pp),
			strerror(errno));
		excode= 1;
		return;
	}
	if (vflag) fprintf(stderr, "ln %s %s\n", file, path_name(pp));
}

void diff_syntax(const char *line)
{
	fprintf(stderr, "remsync: %s: syntax error on this line: %s\n",
		diff_file, line);
	exit(1);
}

void apply_differences(void)
/* Update a tree to a list of differences derived from a remote tree. */
{
	char *line;
	char **argv;
	size_t argc;
	pathname_t path, link;
	size_t trunc;

	path_init(&path);
	path_init(&link);
	path_add(&path, tree);
	path_add(&link, tree);
	trunc= path_length(&path);

	while (!feof(difffp) && (line= read1line(difffp)) != nil) {
		splitline(line, &argv, &argc);
		if (argc == 0) diff_syntax(line);

		path_trunc(&path, trunc);

		if (strcmp(argv[0], "add") == 0) {
			entry_t entry;

			if (argc < 3) diff_syntax(line);
			path_add(&path, argv[1]);
			entry.ignore= (strcmp(argv[2], "ignore") == 0);
			if (!entry.ignore && !getattributes(&entry,
							argc - 2, argv + 2))
				diff_syntax(line);
			apply_add(&path, &entry);
		} else
		if (strcmp(argv[0], "rm") == 0) {
			if (argc != 2) diff_syntax(line);
			path_add(&path, argv[1]);
			apply_remove(&path);
		} else
		if (strcmp(argv[0], "ln") == 0) {
			if (argc != 3) diff_syntax(line);
			path_trunc(&link, trunc);
			path_add(&link, argv[1]);
			path_add(&path, argv[2]);
			apply_link(path_name(&link), &path);
		} else
		if (strcmp(argv[0], "chmod") == 0) {
			if (argc != 5) diff_syntax(line);
			path_add(&path, argv[1]);
			apply_chmod(path_name(&path),
				strtoul(argv[2], nil, 010),
				strtoul(argv[3], nil, 10),
				strtoul(argv[4], nil, 10),
				1);
		} else
		if (strcmp(argv[0], "old") == 0) {
			if (argc != 3) diff_syntax(line);
			path_add(&path, argv[1]);
			apply_mkold(path_name(&path), argv[2]);
		} else
		if (strcmp(argv[0], "end") == 0) {
			if (argc != 1) diff_syntax(line);
			break;
		} else {
			diff_syntax(line);
		}
	}
	checkdiff();
}

void usage(void)
{
    fprintf(stderr, "Usage: remsync -sxv tree [state-file]\n");
    fprintf(stderr, "       remsync -duxvD tree [state-file [diff-file]]\n");
    fprintf(stderr, "       remsync [-xv] tree [diff-file]\n");
    exit(1);
}

int main(int argc, char **argv)
{
	int i;

	for (i= 1; i < argc && argv[i][0] == '-'; i++) {
		char *p= argv[i] + 1;

		if (p[0] == '-' && p[1] == 0) { i++; break; }

		while (*p != 0) {
			switch (*p++) {
			case 's':	sflag= 1; break;
			case 'd':	dflag= 1; break;
			case 'u':	uflag= 1; break;
			case 'x':	xflag= 1; break;
			case 'D':	Dflag= 1; break;
			case 'v':	vflag= 1; break;
			default:	usage();
			}
		}
	}
	if (sflag && dflag) usage();
	if (sflag && uflag) usage();
	if (!sflag && !dflag && uflag) usage();
	if (!dflag && Dflag) usage();

	if (i == argc) usage();
	tree= argv[i++];

	if (sflag) {
		/* Make a state file. */
		state_file= i < argc ? argv[i++] : "-";
		if (i != argc) usage();

		statefp= stdout;
		if (strcmp(state_file, "-") != 0) {
			if ((statefp= fopen(state_file, "w")) == nil)
				fatal(state_file);
		}
		mkstatefile();
	} else
	if (dflag) {
		/* Make a file of differences. */
		state_file= i < argc ? argv[i++] : "-";

		diff_file= i < argc ? argv[i++] : "-";
		if (i != argc) usage();

		statefp= stdin;
		if (strcmp(state_file, "-") != 0) {
			if ((statefp= fopen(state_file, "r")) == nil)
				fatal(state_file);
		}

		difffp= stdout;
		if (strcmp(diff_file, "-") != 0) {
			if ((difffp= fopen(diff_file, "w")) == nil)
				fatal(diff_file);
		}
		mkdifferences();
	} else {
		/* Apply a file of differences. */
		diff_file= i < argc ? argv[i++] : "-";
		if (i != argc) usage();

		difffp= stdin;
		if (strcmp(diff_file, "-") != 0) {
			if ((difffp= fopen(diff_file, "r")) == nil)
				fatal(diff_file);
		}
		apply_differences();
	}
	exit(excode);
}
