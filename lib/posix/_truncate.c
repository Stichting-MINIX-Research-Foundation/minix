#include <lib.h>
#define truncate	_truncate
#define ftruncate	_ftruncate
#include <unistd.h>

PUBLIC int truncate(const char *_path, off_t _length)
{
  message m;
  m.m1_p1 = (char *) _path;
  m.m1_i1 = _length;

  return(_syscall(FS, TRUNCATE, &m));
}

PUBLIC int ftruncate(int _fd, off_t _length)
{
  message m;
  m.m1_i2 = _fd;
  m.m1_i1 = _length;

  return(_syscall(FS, FTRUNCATE, &m));
}
