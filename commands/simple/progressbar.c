
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

_PROTOTYPE(int main, (int argc, char **argv));

void
prettyprogress(long b, long maxb, time_t starttime)
{
  /* print progress indication */
  time_t spent, now;
  long bpsec;
  time(&now);
  spent = now - starttime;
  if(spent > 0 && (bpsec = b / spent) > 0) {
  	int len, i;
  	long secremain, minremain, hremain;
	  secremain = (maxb - b) / bpsec;
	  minremain = (secremain / 60) % 60;
	  hremain = secremain / 3600;
  	len = fprintf(stderr, "Remain %ld files. ETA: %d:%02d:%02d  [",
  		maxb - b,
  		hremain, minremain, secremain % 60);
#define WIDTH 77
  	len = WIDTH - len;
  	for(i = 0; i < (b * (len-1) / maxb); i++) 
  		fprintf(stderr, "=");
 	fprintf(stderr, "|");
  	for(; i < len-2; i++) 
  		fprintf(stderr, "-");
  	fprintf(stderr, "]\r");
  	fflush(stderr);
  }

  return;
}

int main(argc, argv)
int argc;
char *argv[];
{
	long i = 0, count = 0;
	char line[2000];
	time_t start;
	if(argc < 2) return 1;
	count = atol(argv[1]);
	if(count < 1) return 1;
	time(&start);
	printf("\n");
	while(fgets(line, sizeof(line), stdin)) {
		i++;
		printf("[K%s", line);
		if(i <= count) prettyprogress(i, count, start);
		printf("[A");
		fflush(NULL);
		sleep(1);
	}

  	fprintf(stderr, "\nDone.[K\n");

	return 0;
}
