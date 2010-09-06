/*	realpath() - resolve absolute path        Author: Erik van der Kouwe
 *                                            4 December 2009
 *
 * Based on this specification:
 * http://www.opengroup.org/onlinepubs/000095399/functions/realpath.html 
 */
 
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *append_path_component(char *path, const char *component, 
	size_t component_length);
static char *process_path_component(const char *component, 
	size_t component_length, char *resolved_name, int last_part, int max_depth);
static char *realpath_recurse(const char *file_name, char *resolved_name, 
	int max_depth);
static char *remove_last_path_component(char *path);

static char *append_path_component(char *path, const char *component, 
	size_t component_length)
{
	size_t path_length, slash_length;

	/* insert or remove a slash? */
	path_length = strlen(path);
	slash_length = 
		((path[path_length - 1] == '/') ? 0 : 1) + 
		((component[0] == '/') ? 0 : 1) - 1;

	/* check whether this fits */
	if (path_length + slash_length + component_length >= PATH_MAX)
	{
		errno = ENAMETOOLONG;
		return NULL;
	}

	/* insert slash if needed */
	if (slash_length > 0)
		path[path_length] = '/';

	/* copy the bytes */
	memcpy(path + path_length + slash_length, component, component_length);
	path[path_length + slash_length + component_length] = 0;

	return path;
}

static char *process_path_component(const char *component, 
	size_t component_length, char *resolved_name, int last_part, int max_depth)
{
	char readlink_buffer[PATH_MAX + 1];
	ssize_t readlink_buffer_length;
	struct stat stat_buffer;

	/* handle zero-length components */
	if (!component_length)
	{
		if (last_part)
			return resolved_name;
		else
		{
			errno = ENOENT;
			return NULL;
		}
	}

	/* ignore current directory components */
	if (component_length == 1 && component[0] == '.')
		return resolved_name;

	/* process parent directory components */
	if (component_length == 2 && component[0] == '.' && component[1] == '.')
		return remove_last_path_component(resolved_name);

	/* not a special case, so just add the component */
	if (!append_path_component(resolved_name, component, component_length))
		return NULL;

	/* stat partially resolved file */
	if (lstat(resolved_name, &stat_buffer) < 0)
	{
		if (last_part && errno == ENOENT)
			return resolved_name;
		else
			return NULL;
	}

	if (S_ISLNK(stat_buffer.st_mode))
	{
		/* resolve symbolic link */
		readlink_buffer_length = readlink(resolved_name, 
			readlink_buffer, 
			sizeof(readlink_buffer) - 1);
		if (readlink_buffer_length < 0)
			return NULL;

		readlink_buffer[readlink_buffer_length] = 0;
		
		/* recurse to resolve path in link */
		remove_last_path_component(resolved_name);
		if (!realpath_recurse(readlink_buffer, resolved_name, 
			max_depth - 1))
			return NULL;

		/* stat symlink target */
		if (lstat(resolved_name, &stat_buffer) < 0)
		{
			if (last_part && errno == ENOENT)
				return resolved_name;
			else
				return NULL;
		}
	}
	
	/* non-directories may appear only as the last component */
	if (!last_part && !S_ISDIR(stat_buffer.st_mode))
	{
		errno = ENOTDIR;
		return NULL;
	}
	
	return resolved_name;
}

static char *realpath_recurse(const char *file_name, char *resolved_name, 
	int max_depth)
{
	const char *file_name_component;

	/* avoid infinite recursion */
	if (max_depth <= 0)
	{
		errno = ELOOP;
		return NULL;
	}

	/* relative to root or to current? */
	if (file_name[0] == '/')
	{
		/* relative to root */
		resolved_name[0] = '/';
		resolved_name[1] = '\0';
		file_name++;
	}

	/* process the path component by component */
	while (*file_name)
	{
		/* extract a slash-delimited component */
		file_name_component = file_name;
		while (*file_name && *file_name != '/')
			file_name++;

		/* check length of component */
		if (file_name - file_name_component > PATH_MAX)
		{
			errno = ENAMETOOLONG;
			return NULL;
		}

		/* add the component to the current result */
		if (!process_path_component(
			file_name_component, 
			file_name - file_name_component,
			resolved_name,
			!*file_name,
			max_depth))
			return NULL;

		/* skip the slash(es) */
		while (*file_name == '/')
			file_name++;
	}

	return resolved_name;
}

static char *remove_last_path_component(char *path)
{
	char *current, *slash;

	/* find the last slash */
	slash = NULL;
	for (current = path; *current; current++)
		if (*current == '/')
			slash = current;

	/* truncate after the last slash, but do not remove the root */
	if (slash > path)
		*slash = 0;
	else if (slash == path)
		slash[1] = 0;

	return path;
}

char *realpath(const char *file_name, char *resolved_name)
{
	/* check parameters */
	if (!file_name || !resolved_name)
	{
		errno = EINVAL;
		return NULL;
	}

	if (strlen(file_name) > PATH_MAX)
	{
		errno = ENAMETOOLONG;
		return NULL;
	}

	/* basis to resolve against: root or CWD */
	if (file_name[0] == '/')
		*resolved_name = 0;
	else if (!getcwd(resolved_name, PATH_MAX))
		return NULL;

	/* do the actual work */
	return realpath_recurse(file_name, resolved_name, SYMLOOP_MAX);
}
