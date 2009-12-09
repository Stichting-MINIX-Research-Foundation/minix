/* Tests for MINIX3 realpath(3) - by Erik van der Kouwe */
#define _POSIX_SOURCE 1
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_ERROR 4

static int errct = 0;
static const char *executable, *subtest;

#define ERR (e(__LINE__))

static void e(int n)
{
	printf("File %s, error line %d, errno %d: %s\n",
		subtest, n, errno, strerror(errno));

	if (errct++ > MAX_ERROR) 
	{
		printf("Too many errors; test aborted\n");
		exit(1);
	}
}

static void quit(void)
{
	if (errct == 0) 
	{
		printf("ok\n");
		exit(0);
	} 
	else 
	{
		printf("%d errors\n", errct);
		exit(1);
	}
}

static char *remove_last_path_component(char *path)
{
	char *current, *last;

	assert(path);

	/* find last slash */
	last = NULL;
	for (current = path; *current; current++)
		if (*current == '/')
			last = current;

	/* check path component */
	if (last)
	{
		if (strcmp(last + 1, ".") == 0) ERR;
		if (strcmp(last + 1, "..") == 0) ERR;
	}

	/* if only root path slash, we are done */
	if (last <= path)
		return NULL;

	/* chop off last path component */
	*last = 0;
	return path;
}

static void check_realpath(const char *path, int expected_errno)
{
	char buffer[PATH_MAX + 1], *resolved_path;
	struct stat statbuf[2];

	assert(path);
	
	/* run realpath */
	subtest = path;
	resolved_path = realpath(path, buffer);

	/* do we get errors when expected? */
	if (expected_errno)
	{
		if (errno != expected_errno) ERR;
		if (resolved_path) ERR;
		subtest = NULL;
		return;
	}

	/* do we get success when expected? */
	if (!resolved_path)
	{
		ERR;
		subtest = NULL;
		return;
	}
	errno = 0;

	/* do the paths point to the same file? */
	if (stat(path,          &statbuf[0]) < 0) { ERR; return; }
	if (stat(resolved_path, &statbuf[1]) < 0) { ERR; return; }
	if (statbuf[0].st_dev != statbuf[1].st_dev) ERR;
	if (statbuf[0].st_ino != statbuf[1].st_ino) ERR;

	/* is the path absolute? */
	if (resolved_path[0] != '/') ERR;

	/* is each path element allowable? */
	while (remove_last_path_component(resolved_path))
	{
		/* not a symlink? */
		if (lstat(resolved_path, &statbuf[1]) < 0) { ERR; return; }
		if ((statbuf[1].st_mode & S_IFMT) != S_IFDIR) ERR;
	}
	subtest = NULL;
}

static void check_realpath_step_by_step(const char *path, int expected_errno)
{
	char buffer[PATH_MAX + 1];
	const char *path_current;

	assert(path);
	assert(strlen(path) < sizeof(buffer));

	/* check the absolute path */
	check_realpath(path, expected_errno);

	/* try with different CWDs */
	for (path_current = path; *path_current; path_current++)
		if (path_current[0] == '/' && path_current[1])
		{
			/* set CWD */
			memcpy(buffer, path, path_current - path + 1);
			buffer[path_current - path + 1] = 0;
			if (chdir(buffer) < 0) { ERR; continue; }

			/* perform test */
			check_realpath(path_current + 1, expected_errno);
		}
}

static char *pathncat(char *buffer, size_t size, const char *path1, const char *path2)
{
	size_t len1, len2, lenslash;

	assert(buffer);
	assert(path1);
	assert(path2);

	/* check whether it fits */
	len1 = strlen(path1);
	len2 = strlen(path2);
	lenslash = (len1 > 0 && path1[len1 - 1] == '/') ? 0 : 1;
	if (len1 >= size || /* check individual components to avoid overflow */
		len2 >= size || 
		len1 + len2 + lenslash >= size)
		return NULL;

	/* perform the copy */
	memcpy(buffer, path1, len1);
	if (lenslash)
		buffer[len1] = '/';

	memcpy(buffer + len1 + lenslash, path2, len2 + 1);
	return buffer;
}

static void check_realpath_recurse(const char *path, int depth)
{
	DIR *dir;
	struct dirent *dirent;
	char pathsub[PATH_MAX + 1];

	/* check with the path itself */
	check_realpath_step_by_step(path, 0);

	/* don't go too deep */
	if (depth < 1)
		return;

	/* loop through subdirectories (including . and ..) */
	if (!(dir = opendir(path))) 
	{
		if (errno != ENOTDIR)
			ERR;
		return;
	}
	while (dirent = readdir(dir))
	{
		/* build path */
		if (!pathncat(pathsub, sizeof(pathsub), path, dirent->d_name))
		{
			ERR;
			continue;
		}

		/* check path */
		check_realpath_recurse(pathsub, depth - 1);
	}
	if (closedir(dir) < 0) ERR;
}

#define PATH_DEPTH 4
#define PATH_BASE "/t43"
#define L(x) PATH_BASE "/link_" #x ".tmp"

static char basepath[PATH_MAX + 1];

static char *addbasepath(char *buffer, const char *path)
{
	size_t basepathlen, pathlen;
	int slashlen;
	
	/* assumption: both start with slash and neither end with it */
	assert(basepath[0] == '/');
	assert(basepath[strlen(basepath) - 1] != '/');
	assert(buffer);
	assert(path);
	assert(path[0] == '/');
	
	/* check result length */
	basepathlen = strlen(basepath);
	pathlen = strlen(path);
	if (basepathlen + pathlen > PATH_MAX)
	{
		printf("path too long\n");
		exit(-1);
	}

	/* concatenate base path and path */
	memcpy(buffer, basepath, basepathlen);
	memcpy(buffer + basepathlen, path, pathlen + 1);
	return buffer;
}

static void cleanup(const char *path)
{
	DIR *dir;
	struct dirent *dirent;
	char pathsub[PATH_MAX + 1];
	struct stat statbuf;
	
	/* determine file type, avoid following links */
	if (lstat(path, &statbuf) < 0)
	{
		if (errno != ENOENT) ERR;
		return;
	}

	/* only recursively process directories (NOT symlinks!) */
	if ((statbuf.st_mode & S_IFMT) != S_IFDIR)
	{
		if (unlink(path) < 0) ERR;
		return;	
	}

	/* loop through subdirectories (excluding . and ..) */
	if (!(dir = opendir(path))) 
	{
		ERR;
		return;
	}
	while (dirent = readdir(dir))
	{
		/* ignore current and parent directories */
		if (strcmp(dirent->d_name, ".") == 0 || 
			strcmp(dirent->d_name, "..") == 0)
			continue;
			
		/* build path */
		if (!pathncat(pathsub, sizeof(pathsub), path, dirent->d_name))
		{
			ERR;
			continue;
		}

		/* delete path */
		cleanup(pathsub);
	}
	if (closedir(dir) < 0) ERR;

	/* remove the (now empty) directory itself */
	if (rmdir(path) < 0) ERR;
}

int main(int argc, char **argv)
{
	char buffer1[PATH_MAX + 1], buffer2[PATH_MAX + 1];

	/* initialize */
	printf("Test 43 ");
	fflush(stdout);
	executable = argv[0];
	getcwd(basepath, sizeof(basepath));
	cleanup(addbasepath(buffer1, PATH_BASE));

	/* prepare some symlinks to make it more difficult */
	if (mkdir(addbasepath(buffer1, PATH_BASE), S_IRWXU) < 0) ERR;
	if (symlink("/",      addbasepath(buffer1, L(1))) < 0) ERR;
	if (symlink(basepath, addbasepath(buffer1, L(2))) < 0) ERR;

	/* perform some tests */
	check_realpath_recurse(basepath, PATH_DEPTH);

	/* now try with recursive symlinks */
	if (symlink(addbasepath(buffer1, L(3)), addbasepath(buffer2, L(3))) < 0) ERR;
	if (symlink(addbasepath(buffer1, L(5)), addbasepath(buffer2, L(4))) < 0) ERR;
	if (symlink(addbasepath(buffer1, L(4)), addbasepath(buffer2, L(5))) < 0) ERR;
	check_realpath_step_by_step(addbasepath(buffer1, L(3)), ELOOP);
	check_realpath_step_by_step(addbasepath(buffer1, L(4)), ELOOP);
	check_realpath_step_by_step(addbasepath(buffer1, L(5)), ELOOP);

	/* delete the symlinks */
	cleanup(addbasepath(buffer1, PATH_BASE));

	/* done */
	quit();
	return(-1);	/* impossible */
}
