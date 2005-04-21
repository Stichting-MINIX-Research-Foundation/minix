/*	cleantmp 1.6 - clean out a tmp dir.		Author: Kees J. Bot
 *								11 Apr 1991
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

#ifndef S_ISLNK
/* There were no symlinks in medieval times. */
#define lstat stat
#endif

#ifndef DEBUG
#define NDEBUG
#endif
#include <assert.h>

#define SEC_DAY	(24 * 3600L)	/* A full day in seconds */
#define DOTDAYS	14		/* Don't remove tmp/.* in at least 14 days. */

void report(const char *label)
{
	fprintf(stderr, "cleantmp: %s: %s\n", label, strerror(errno));
}

void fatal(const char *label)
{
	report(label);
	exit(1);
}

void *alloc(size_t s)
{
	void *mem;

	if ((mem= (void *) malloc(s)) == nil) fatal("");
	return mem;
}

int force= 0;			/* Force remove all. */
int debug= 0;			/* Debug level. */

void days2time(unsigned long days, time_t *retired, time_t *dotretired)
{
	struct tm *tm;
	time_t t;

	time(&t);

	tm= localtime(&t);
	tm->tm_hour= 0;
	tm->tm_min= 0;
	tm->tm_sec= 0;	/* Step back to midnight of this day. */
	t= mktime(tm);

	if (t < (days - 1) * SEC_DAY) {
		*retired= *dotretired= 0;
	} else {
		*retired= t - (days - 1) * SEC_DAY;
		*dotretired= t - (DOTDAYS - 1) * SEC_DAY;
		if (*dotretired > *retired) *dotretired= *retired;
	}
	if (debug >= 2) fprintf(stderr, "Retired:    %s", ctime(retired));
	if (debug >= 2) fprintf(stderr, "Dotretired: %s", ctime(dotretired));
}

/* Path name construction, addpath adds a component, delpath removes it.
 * The string 'path' is used throughout the program as the file under
 * examination.
 */

char *path;	/* Path name constructed in path[]. */
int plen= 0, pidx= 0;	/* Lenght/index for path[]. */

void addpath(int *didx, char *name)
/* Add a component to path. (name may also be a full path at the first call)
 * The index where the current path ends is stored in *pdi.
 */
{
	if (plen == 0) path= (char *) alloc((plen= 32) * sizeof(path[0]));

	*didx= pidx;	/* Record point to go back to for delpath. */

	if (pidx > 0 && path[pidx-1] != '/') path[pidx++]= '/';

	do {
		if (*name != '/' || pidx == 0 || path[pidx-1] != '/') {
			if (pidx == plen &&
				(path= (char *) realloc((void *) path,
					(plen*= 2) * sizeof(path[0]))) == nil
			) fatal("");
			path[pidx++]= *name;
		}
	} while (*name++ != 0);

	--pidx;		/* Put pidx back at the null.  The path[pidx++]= '/'
			 * statement will overwrite it at the next call.
			 */
	assert(pidx < plen);
}

void delpath(int didx)
{
	assert(0 <= didx);
	assert(didx <= pidx);
	path[pidx= didx]= 0;
}

struct file {
	struct file	*next;
	char		*name;
};

struct file *listdir(void)
{
	DIR *dp;
	struct dirent *entry;
	struct file *first, **last= &first;

	if ((dp= opendir(path)) == nil) {
		report(path);
		return nil;
	}

	while ((entry= readdir(dp)) != nil) {
		struct file *new;

		if (strcmp(entry->d_name, ".") == 0
			|| strcmp(entry->d_name, "..") == 0) continue;

		new= (struct file *) alloc(sizeof(*new));
		new->name= (char *) alloc((size_t) strlen(entry->d_name) + 1);
		strcpy(new->name, entry->d_name);

		*last= new;
		last= &new->next;
	}
	closedir(dp);
	*last= nil;

	return first;
}

struct file *shorten(struct file *list)
{
	struct file *junk;

	assert(list != nil);

	junk= list;
	list= list->next;

	free((void *) junk->name);
	free((void *) junk);

	return list;
}

/* Hash list of files to ignore. */
struct file *ignore_list[1024];
size_t n_ignored= 0;

unsigned ihash(char *name)
/* A simple hashing function on a file name. */
{
	unsigned h= 0;

	while (*name != 0) h= (h * 0x1111) + *name++;

	return h & (arraysize(ignore_list) - 1);
}

void do_ignore(int add, char *name)
/* Add or remove a file to/from the list of files to ignore. */
{
	struct file **ipp, *ip;

	ipp= &ignore_list[ihash(name)];
	while ((ip= *ipp) != nil) {
		if (strcmp(name, ip->name) <= 0) break;
		ipp= &ip->next;
	}

	if (add) {
		ip= alloc(sizeof(*ip));
		ip->name= alloc((strlen(name) + 1) * sizeof(ip->name[0]));
		strcpy(ip->name, name);
		ip->next= *ipp;
		*ipp= ip;
		n_ignored++;
	} else {
		assert(ip != nil);
		*ipp= ip->next;
		free(ip->name);
		free(ip);
		n_ignored--;
	}
}

int is_ignored(char *name)
/* Is a file in the list of ignored files? */
{
	struct file *ip;
	int r;

	ip= ignore_list[ihash(name)];
	while (ip != nil) {
		if ((r = strcmp(name, ip->name)) <= 0) return (r == 0);
		ip= ip->next;
	}
	return 0;
}

#define is_ignored(name) (n_ignored > 0 && (is_ignored)(name))

time_t retired, dotretired;

enum level { TOP, DOWN };

void cleandir(enum level level, time_t retired)
{
	struct file *list;
	struct stat st;
	time_t ret;

	if (debug >= 2) fprintf(stderr, "Cleaning %s\n", path);

	list= listdir();

	while (list != nil) {
		int didx;

		ret= (level == TOP && list->name[0] == '.') ?
			dotretired : retired;
			/* don't rm tmp/.* too soon. */

		addpath(&didx, list->name);

		if (is_ignored(path)) {
			if (debug >= 1) fprintf(stderr, "ignoring %s\n", path);
			do_ignore(0, path);
		} else
		if (is_ignored(list->name)) {
			if (debug >= 1) fprintf(stderr, "ignoring %s\n", path);
		} else
		if (lstat(path, &st) < 0) {
			report(path);
		} else
		if (S_ISDIR(st.st_mode)) {
			cleandir(DOWN, ret);
			if (force || st.st_mtime < ret) {
				if (debug < 3 && rmdir(path) < 0) {
					if (errno != ENOTEMPTY
							&& errno != EEXIST) {
						report(path);
					}
				} else {
					if (debug >= 1) {
						fprintf(stderr,
							"rmdir %s\n", path);
					}
				}
			}
		} else {
			if (force || (st.st_atime < ret
					&& st.st_mtime < ret
					&& st.st_ctime < ret)
			) {
				if (debug < 3 && unlink(path) < 0) {
					if (errno != ENOENT) {
						report(path);
					}
				} else {
					if (debug >= 1) {
						fprintf(stderr,
							"rm %s\n", path);
					}
				}
			}
		}
		delpath(didx);
		list= shorten(list);
	}
}

void usage(void)
{
	fprintf(stderr,
	"Usage: cleantmp [-d[level]] [-i file ] ... -days|-f directory ...\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int i;
	unsigned long days;

	i= 1;
	while (i < argc && argv[i][0] == '-') {
		char *opt= argv[i++] + 1;

		if (opt[0] == '-' && opt[1] == 0) break;

		if (opt[0] == 'd') {
			debug= 1;
			if (opt[1] != 0) debug= atoi(opt + 1);
		} else
		if (opt[0] == 'i') {
			if (*++opt == 0) {
				if (i == argc) usage();
				opt= argv[i++];
			}
			do_ignore(1, opt);
		} else
		if (opt[0] == 'f' && opt[1] == 0) {
			force= 1;
			days= 1;
		} else {
			char *end;
			days= strtoul(opt, &end, 10);
			if (*opt == 0 || *end != 0
				|| days == 0
				|| ((time_t) (days * SEC_DAY)) / SEC_DAY != days
			) {
				fprintf(stderr,
				"cleantmp: %s is not a valid number of days\n",
					opt);
				exit(1);
			}
		}
	}
	if (days == 0) usage();

	days2time(days, &retired, &dotretired);

	while (i < argc) {
		int didx;

		if (argv[i][0] == 0) {
			fprintf(stderr, "cleantmp: empty pathname!\n");
			exit(1);
		}
		addpath(&didx, argv[i]);
		cleandir(TOP, retired);
		delpath(didx);
		assert(path[0] == 0);
		i++;
	}
	exit(0);
}
