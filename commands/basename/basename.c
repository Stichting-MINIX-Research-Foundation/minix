/* basename - print last part of a path      Authors: B. Garfolo & P. Nelson */

/* Basename - print the last part of a path.
 *
 *    For MINIX  --  Conforms to POSIX - P1003.2/D10
 *      Exception -- it ignores the LC environment variables.
 *
 *    Original MINIX author:  Blaine Garfolo
 *    POSIX rewrite author:   Philip A. Nelson
 *
 *    POSIX version - October 20, 1990
 *      Feb 14, 1991: changed rindex to strrchr. (PAN)
 *
 */


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define EOS '\0'

int main(int argc, char **argv);

int main(argc, argv)
int argc;
char *argv[];
{
  char *result_string;		/* The pointer into argv[1]. */
  char *temp;			/* Used to move around in argv[1]. */
  int suffix_len;		/* Length of the suffix. */
  int suffix_start;		/* Where the suffix should start. */


  /* Check for the correct number of arguments. */
  if ((argc < 2) || (argc > 3)) {
	fprintf(stderr, "Usage: basename string [suffix] \n");
	exit(1);
  }

  /* Check for all /'s */
  for (temp = argv[1]; *temp == '/'; temp++)	/* Move to next char. */
	;
  if (*temp == EOS) {
	printf("/\n");
	exit(0);
  }

  /* Build the basename. */
  result_string = argv[1];

  /* Find the last /'s */
  temp = strrchr(result_string, '/');

  if (temp != NULL) {
	/* Remove trailing /'s. */
	while ((*(temp + 1) == EOS) && (*temp == '/')) *temp-- = EOS;

	/* Set result_string to last part of path. */
	if (*temp != '/') temp = strrchr(result_string, '/');
	if (temp != NULL && *temp == '/') result_string = temp + 1;
  }

  /* Remove the suffix, if any. */
  if (argc > 2) {
	suffix_len = strlen(argv[2]);
	suffix_start = strlen(result_string) - suffix_len;
	if (suffix_start > 0)
		if (strcmp(result_string + suffix_start, argv[2]) == EOS)
			*(result_string + suffix_start) = EOS;
  }

  /* Print the resultant string. */
  printf("%s\n", result_string);
  return(0);
}
