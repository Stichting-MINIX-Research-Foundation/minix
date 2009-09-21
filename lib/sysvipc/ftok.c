#include <sys/ipc.h>
#include <sys/stat.h>

key_t ftok(const char *path, int id)
{
	struct stat st;

	if (lstat(path, &st) < 0)
		return (key_t) -1;

	return (key_t) ((st.st_ino & 0xffff) |
		((st.st_dev & 0xff) << 16) |
		((id & 0xff) << 24));
}

