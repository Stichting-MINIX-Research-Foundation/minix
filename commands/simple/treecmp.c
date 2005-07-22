/* treecmp - compare two trees		Author: Andy Tanenbaum */

/* This program recursively compares two trees and reports on differences.
 * It can be used, for example, when a project consists of a large number
 * of files and directories.  When a new release (i.e., a new tree) has been
 * prepared, the old and new tree can be compared to give a list of what has
 * changed.  The algorithm used is that the second tree is recursively
 * descended and for each file or directory found, the corresponding one in
 * the other tree checked.  The two arguments are not completely symmetric
 * because the second tree is descended, not the first one, but reversing
 * the arguments will still detect all the differences, only they will be
 * printed in a different order.  The program needs lots of stack space
 * because routines with local arrays are called recursively. The call is
 *    treecmp [-cv] old_dir new_dir
 * The -v flag (verbose) prints the directory names as they are processed.
 * The -c flag (changes) just prints the names of changed and new files.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFSIZE 4096		/* size of file buffers */
#define MAXPATH 128		/* longest acceptable path */
#define DIRENTLEN 14		/* number of characters in a file name */

struct dirstruct {		/* layout of a directory entry */
  ino_t inum;
  char fname[DIRENTLEN];
};

struct stat stat1, stat2;	/* stat buffers */

char buf1[BUFSIZE];		/* used for comparing bufs */
char buf2[BUFSIZE];		/* used for comparing bufs */

int changes;			/* set on -c flag */
int verbose;			/* set on -v flag */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void compare, (char *old, char *new));
_PROTOTYPE(void regular, (char *old, char *new));
_PROTOTYPE(void directory, (char *old, char *new));
_PROTOTYPE(void check, (char *s, struct dirstruct *dp1, int ent1, char *new));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  char *p;

  if (argc < 3 || argc > 4) usage();
  p = argv[1];
  if (argc == 4) {
	if (*p != '-') usage();
	p++;
	if (*p == '\0') usage();
	while (*p) {
		if (*p == 'c') changes++;
		if (*p == 'v') verbose++;
		if (*p != 'c' && *p != 'v') usage();
		p++;
	}
  }
  if (argc == 3)
	compare(argv[1], argv[2]);
  else
	compare(argv[2], argv[3]);

  return(0);
}

void compare(old, new)
char *old, *new;
{
/* This is the main comparision routine.  It gets two path names as arguments
 * and stats them both.  Depending on the results, it calls other routines
 * to compare directories or files.
 */

  int type1, type2;

  if (stat(new, &stat1) < 0) {
	/* The new file does not exist. */
	if (changes == 0)
		fprintf(stderr, "Cannot stat: %s\n", new);
	else
		printf("%s\n", new);
	return;
  }
  if (stat(old, &stat2) < 0) {
	/* The old file does not exist. */
	if (changes == 0) 
		fprintf(stderr, "Missing file: %s\n", old);
	else
		printf("%s\n", new);
	return;
  }

  /* Examine the types of the files. */
  type1 = stat1.st_mode & S_IFMT;
  type2 = stat2.st_mode & S_IFMT;
  if (type1 != type2) {
	fprintf(stderr, "Type diff: %s and %s\n", new, old);
	return;
  }

  /* The types are the same. */
  switch (type1) {
      case S_IFREG:	regular(old, new);	break;
      case S_IFDIR:	directory(old, new);	break;
      case S_IFCHR:	break;
      case S_IFBLK:	break;
      default:		fprintf(stderr, "Unknown file type %o\n", type1);
  }
  return;
}

void regular(old, new)
char *old, *new;
{
/* Compare to regular files.  If they are different, complain. */

  int fd1, fd2, n1, n2;
  unsigned bytes;
  long count;

  if (stat1.st_size != stat2.st_size) {
	if (changes == 0)
		printf("Size diff: %s and %s\n", new, old);
	else
		printf("%s\n", new);
	return;
  }

  /* The sizes are the same.  We actually have to read the files now. */
  fd1 = open(new, O_RDONLY);
  if (fd1 < 0) {
	fprintf(stderr, "Cannot open %s for reading\n", new);
	return;
  }
  fd2 = open(old, O_RDONLY);
  if (fd2 < 0) {
	fprintf(stderr, "Cannot open %s for reading\n", old);
	return;
  }
  count = stat1.st_size;
  while (count > 0L) {
	bytes = (unsigned) (count > BUFSIZE ? BUFSIZE : count);	/* rd count */
	n1 = read(fd1, buf1, bytes);
	n2 = read(fd2, buf2, bytes);
	if (n1 != n2) {
		if (changes == 0)
			printf("Length diff: %s and %s\n", new, old);
		else
			printf("%s\n", new);
		close(fd1);
		close(fd2);
		return;
	}

	/* Compare the buffers. */
	if (memcmp((void *) buf1, (void *) buf2, (size_t) n1) != 0) {
		if (changes == 0)
			printf("File diff: %s and %s\n", new, old);
		else
			printf("%s\n", new);
		close(fd1);
		close(fd2);
		return;
	}
	count -= n1;
  }
  close(fd1);
  close(fd2);
}

void directory(old, new)
char *old, *new;
{
/* Recursively compare two directories by reading them and comparing their
 * contents.  The order of the entries need not be the same.
 */

  int fd1, fd2, n1, n2, ent1, ent2, i, used1 = 0, used2 = 0;
  char *dir1buf, *dir2buf;
  char name1buf[MAXPATH], name2buf[MAXPATH];
  struct dirstruct *dp1, *dp2;
  unsigned dir1bytes, dir2bytes;

  /* Allocate space to read in the directories */
  dir1bytes = (unsigned) stat1.st_size;
  dir1buf = (char *)malloc((size_t)dir1bytes);
  if (dir1buf == 0) {
	fprintf(stderr, "Cannot process directory %s: out of memory\n", new);
	return;
  }
  dir2bytes = (unsigned) stat2.st_size;
  dir2buf = (char *)malloc((size_t)dir2bytes);
  if (dir2buf == 0) {
	fprintf(stderr, "Cannot process directory %s: out of memory\n", old);
	free(dir1buf);
	return;
  }

  /* Read in the directories. */
  fd1 = open(new, O_RDONLY);
  if (fd1 > 0) n1 = read(fd1, dir1buf, dir1bytes);
  if (fd1 < 0 || n1 != dir1bytes) {
	fprintf(stderr, "Cannot read directory %s\n", new);
	free(dir1buf);
	free(dir2buf);
	if (fd1 > 0) close(fd1);
	return;
  }
  close(fd1);

  fd2 = open(old, O_RDONLY);
  if (fd2 > 0) n2 = read(fd2, dir2buf, dir2bytes);
  if (fd2 < 0 || n2 != dir2bytes) {
	fprintf(stderr, "Cannot read directory %s\n", old);
	free(dir1buf);
	free(dir2buf);
	close(fd1);
	if (fd2 > 0) close(fd2);
	return;
  }
  close(fd2);

  /* Linearly search directories */
  ent1 = dir1bytes / sizeof(struct dirstruct);
  dp1 = (struct dirstruct *) dir1buf;
  for (i = 0; i < ent1; i++) {
	if (dp1->inum != 0) used1++;
	dp1++;
  }

  ent2 = dir2bytes / sizeof(struct dirstruct);
  dp2 = (struct dirstruct *) dir2buf;
  for (i = 0; i < ent2; i++) {
	if (dp2->inum != 0) used2++;
	dp2++;
  }

  if (verbose) printf("Directory %s: %d entries\n", new, used1);

  /* Check to see if any entries in dir2 are missing from dir1. */
  dp1 = (struct dirstruct *) dir1buf;
  dp2 = (struct dirstruct *) dir2buf;
  for (i = 0; i < ent2; i++) {
	if (dp2->inum == 0 || strcmp(dp2->fname, ".") == 0 ||
					    strcmp(dp2->fname, "..") == 0) {
		dp2++;
		continue;
	}
	check(dp2->fname, dp1, ent1, new);
	dp2++;
  }

  /* Recursively process all the entries in dir1. */
  dp1 = (struct dirstruct *) dir1buf;
  for (i = 0; i < ent1; i++) {
	if (dp1->inum == 0 || strcmp(dp1->fname, ".") == 0 ||
	    strcmp(dp1->fname, "..") == 0) {
		dp1++;
		continue;
	}
	if (strlen(new) + DIRENTLEN >= MAXPATH) {
		fprintf(stderr, "Path too long: %s\n", new);
		free(dir1buf);
		free(dir2buf);
		return;
	}
	if (strlen(old) + DIRENTLEN >= MAXPATH) {
		fprintf(stderr, "Path too long: %s\n", old);
		free(dir1buf);
		free(dir2buf);
		return;
	}
	strcpy(name1buf, old);
	strcat(name1buf, "/");
	strncat(name1buf, dp1->fname, (size_t)DIRENTLEN);
	strcpy(name2buf, new);
	strcat(name2buf, "/");
	strncat(name2buf, dp1->fname, (size_t)DIRENTLEN);

	/* Here is the recursive call to process an entry. */
	compare(name1buf, name2buf);	/* recursive call */
	dp1++;
  }

  free(dir1buf);
  free(dir2buf);
}

void check(s, dp1, ent1, new)
char *s;
struct dirstruct *dp1;
int ent1;
char *new;
{
/* See if the file name 's' is present in the directory 'dirbuf'. */
  int i;
  char file[DIRENTLEN+1];

  for (i = 0; i < ent1; i++) {
	if (strncmp(dp1->fname, s, (size_t)DIRENTLEN) == 0) return;
	dp1++;
  }
  if (changes == 0) {
	strncpy(file, s, DIRENTLEN);
	file[DIRENTLEN] = '\0';
	printf("Missing file: %s/%s\n", new, file);
  }
	
}

void usage()
{
  printf("Usage: treecmp [-cv] old_dir new_dir\n");
  exit(1);
}
