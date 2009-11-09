/* factor - print the prime factors of a number      Author: Andy Tanenbaum */

#include <stdlib.h>
#include <stdio.h>

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(long first, (long k));

int main(argc, argv)
int argc;
char *argv[];
{
/* Factor a number */

  long i, n, flag = 0;

  if (argc != 2 || (n = atol(argv[1])) < 2) {
	printf("Usage: factor n   (2 <= n < 2**31)\n");
	exit(1);
  }
  if (n == 2) {
	printf("2 is a prime\n");
	exit(0);
  }
  while (1) {
	i = first(n);
	if (i == 0) {
		if (flag == 0)
			printf("%ld is a prime\n", n);
		else
			printf("%ld\n", n);
		exit(0);
	}
	printf("%ld ", i);
	n = n / i;
	flag = 1;
  }
}


long first(k)
long k;
{
/* Return the first factor of k.  If it is a prime, return 0; */

  long i;

  if (k == 2) return(0);
  if (k % 2 == 0) return (2);
  for (i = 3; i <= k / 3; i += 2)
	if (k % i == 0) return(i);
  return(0);
}
