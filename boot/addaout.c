/* A small utility to append an a.out header to an arbitrary file. This allows
 * inclusion of arbitrary data in the boot image, so that it is magically 
 * loaded as a RAM disk. The a.out header is structured as follows:
 *
 *	a_flags:	A_IMG to indicate this is not an executable
 *
 * Created:	April 2005, Jorrit N. Herder
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <a.out.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#define INPUT_FILE	1
#define OUTPUT_FILE	2

/* Report problems. */
void report(char *problem, char *message) 
{
  fprintf(stderr, "%s:\n", problem);
  fprintf(stderr, "   %s\n\n", message);
}


int copy_data(int srcfd, int dstfd) 
{
    char buf[8192];
    ssize_t n;
    int total=0;

    /* Copy the little bytes themselves. (Source from cp.c). */
    while ((n= read(srcfd, buf, sizeof(buf))) > 0) {
	char *bp = buf;
	ssize_t r;

	while (n > 0 && (r= write(dstfd, bp, n)) > 0) {
	    bp += r;
	    n -= r;
	    total += r;
	}
	if (r <= 0) {
	    if (r == 0) {
		fprintf(stderr, "Warning: EOF writing to output file.\n");
		return(-1);
	    }
	}
    }
    return(total);
}


/* Main program. */
int main(int argc, char **argv)
{
  struct exec aout;
  struct stat stin;
  int fdin, fdout;
  char * bp;
  int n,r;
  int total_size=0;
  int result;

  /* Check if command line arguments are present, or print usage. */
  if (argc!=3) {
	printf("Invalid arguments. Usage:\n");
	printf("    %s <input_file> <output_file>\n",argv[0]); 
	return(1);
  }

  /* Check if we can open the input and output file. */
  if (stat(argv[INPUT_FILE], &stin) != 0) {
  	report("Couldn't get status of input file", strerror(errno));
  	return(1);
  }
  if ((fdin = open(argv[INPUT_FILE], O_RDONLY)) < 0) {
  	report("Couldn't open input file", strerror(errno));
  	return(1);
  }
  if ((fdout = open(argv[OUTPUT_FILE], O_WRONLY|O_CREAT|O_TRUNC, 
  		stin.st_mode & 0777)) < 0) {
  	report("Couldn't open output file", strerror(errno));
  	return(1);
  }


  /* Copy input file to output file, but leave space for a.out header. */ 
  lseek(fdout, sizeof(aout), SEEK_SET);
  total_size = copy_data(fdin, fdout);
  if (total_size < 0) {
  	report("Aborted", "Output file may be truncated.");
  	return(1);
  } else if (total_size == 0) {
  	report("Aborted without prepending header", "No data in input file.");
  	return(1);
  }


  /* Build a.out header and write to output file. */
  memset(&aout, 0, sizeof(struct exec));
  aout.a_magic[0] = A_MAGIC0;
  aout.a_magic[1] = A_MAGIC1;
  aout.a_flags |= A_IMG;
  aout.a_hdrlen = sizeof(aout);
  aout.a_text = 0;
  aout.a_data = total_size;
  aout.a_bss = 0;
  aout.a_total = aout.a_hdrlen + aout.a_data;

  bp = (char *) &aout;
  n = sizeof(aout);
  lseek(fdout, 0, SEEK_SET);
  while (n > 0 && (r= write(fdout, bp, n)) > 0) {
  	bp += r;
  	n -= r;
  }

  printf("Prepended data file (%u bytes) with a.out header (%u bytes).\n", 
	total_size, sizeof(aout));
  printf("Done.\n");

  return(0);
}

