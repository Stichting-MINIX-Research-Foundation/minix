/* strip - remove symbols.		Author: Dick van Veen */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <a.out.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Strip [file] ...
 *
 *	-	when no file is present, a.out is assumed.
 *
 */

#define A_OUT		"a.out"
#define NAME_LENGTH	128	/* max file path name */

char buffer[BUFSIZ];		/* used to copy executable */
char new_file[NAME_LENGTH];	/* contains name of temporary */
struct exec header;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void strip, (char *file));
_PROTOTYPE(int read_header, (int fd));
_PROTOTYPE(int write_header, (int fd));
_PROTOTYPE(int make_tmp, (char *new_name, char *name));
_PROTOTYPE(int copy_file, (int fd1, int fd2, long size));

int main(argc, argv)
int argc;
char **argv;
{
  argv++;
  if (*argv == NULL)
	strip(A_OUT);
  else
	while (*argv != NULL) {
		strip(*argv);
		argv++;
	}
  return(0);
}

void strip(file)
char *file;
{
  int fd, new_fd;
  struct stat buf;
  long symb_size, relo_size;

  fd = open(file, O_RDONLY);
  if (fd == -1) {
	fprintf(stderr, "can't open %s\n", file);
	close(fd);
	return;
  }
  if (read_header(fd)) {
	fprintf(stderr, "%s: not an executable file\n", file);
	close(fd);
	return;
  }
  if (header.a_syms == 0L) {
	close(fd);		/* no symbol table present */
	return;
  }
  symb_size = header.a_syms;
  header.a_syms = 0L;		/* remove table size */
  fstat(fd, &buf);
  relo_size = buf.st_size - (A_MINHDR + header.a_text + header.a_data + symb_size);
  new_fd = make_tmp(new_file, file);
  if (new_fd == -1) {
	fprintf(stderr, "can't create temporary file\n");
	close(fd);
	return;
  }
  if (write_header(new_fd)) {
	fprintf(stderr, "can't write temporary file\n");
	unlink(new_file);
	close(fd);
	close(new_fd);
	return;
  }
  if (copy_file(fd, new_fd, header.a_text + header.a_data)) {
	fprintf(stderr, "can't copy %s\n", file);
	unlink(new_file);
	close(fd);
	close(new_fd);
	return;
  }
  if (relo_size != 0) {
	lseek(fd, symb_size, 1);
	if (copy_file(fd, new_fd, relo_size)) {
	    fprintf(stderr, "can't copy %s\n", file);
	    unlink(new_file);
	    close(fd);
	    close(new_fd);
	    return;
	}
  }
  close(fd);
  close(new_fd);
  if (unlink(file) == -1) {
	fprintf(stderr, "can't unlink %s\n", file);
	unlink(new_file);
	return;
  }
  link(new_file, file);
  unlink(new_file);
  chmod(file, buf.st_mode);
}

int read_header(fd)
int fd;
{
  if (read(fd, (char *) &header, A_MINHDR) != A_MINHDR) return(1);
  if (BADMAG(header)) return (1);
  if (header.a_hdrlen > sizeof(struct exec)) return (1);
  lseek(fd, 0L, SEEK_SET);	/* variable size header */
  if (read(fd, (char *)&header, (int)header.a_hdrlen) != (int) header.a_hdrlen)
	return(1);
  return(0);
}

int write_header(fd)
int fd;
{
  lseek(fd, 0L, SEEK_SET);
  if (write(fd, (char *)&header, (int)header.a_hdrlen) != (int)header.a_hdrlen)
	return(1);
  return(0);
}

int make_tmp(new_name, name)
char *new_name, *name;
{
  int len;
  char *nameptr;

  len = strlen(name);
  if (len + 1 > NAME_LENGTH) return(-1);
  strcpy(new_name, name);
  nameptr = strrchr(new_name, '/');
  if (nameptr == NULL) nameptr = new_name - 1;
  if (nameptr - new_name + 6 + 1 > NAME_LENGTH) return (-1);
  strcpy(nameptr + 1, "XXXXXX");
  mktemp(new_name);
  return(creat(new_name, 0777));
}

int copy_file(fd1, fd2, size)
int fd1, fd2;
long size;
{
  int length;

  while (size > 0) {
	if (size < sizeof(buffer))
		length = size;
	else
		length = sizeof(buffer);
	if (read(fd1, buffer, length) != length) return(1);
	if (write(fd2, buffer, length) != length) return (1);
	size -= length;
  }
  return(0);
}
