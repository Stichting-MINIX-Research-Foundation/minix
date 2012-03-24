
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv);

void
prettyprogress(long b, long maxb, time_t starttime)
{
  /* print progress indication */
  time_t spent, now;
  double bpsec;
  time(&now);
  spent = now - starttime;
  printf("\r");	/* Make sure progress bar starts at beginning of line */
  if(spent > 0 && (bpsec = (double)b / spent) > 0) {
  	int len, i;
  	long secremain, minremain, hremain;
	  secremain = (maxb - b) / bpsec;
	  minremain = (secremain / 60) % 60;
	  hremain = secremain / 3600;
  	len = printf("Remaining: %ld files. ", maxb-b);

#if 0
  	len += printf("ETA: %d:%02d:%02d ",
  		hremain, minremain, secremain % 60);
#endif

	len += printf(" [");

#define WIDTH 77
  	len = WIDTH - len;
  	for(i = 0; i < (b * (len-1) / maxb); i++) 
  		printf("=");
 	printf("|");
  	for(; i < len-2; i++) 
  		printf("-");
  	printf("][K\n");
  } else printf("\n");

  return;
}

int main(argc, argv)
int argc;
char *argv[];
{
	long i = 0, count = 0;
	int l;
	char line[2000];
	time_t start;
	if(argc < 2) return 1;
	count = atol(argv[1]);
	if(count < 0) return 1;
	time(&start);
	printf("\n");
#define LINES 5
	for(l = 1; l <= LINES+1; l++) printf("\n");
	printf("[A");
	while(fgets(line, sizeof(line), stdin)) {
		char *nl;
		i++;
		for(l = 0; l <= LINES; l++)  printf("[A");
		if(i <= count) prettyprogress(i, count, start);
		else printf("\n");
		printf("[M");
		for(l = 0; l < LINES; l++)  printf("[B");
		if((nl = strchr(line, '\n'))) *nl = '\0';
		line[78] = '\0';
		printf("\r%s\r", line);
	}

  	printf("\nDone.[K\n");

	return 0;
}
