/* talkd.c Copyright Michael Temari 07/22/1996 All Rights Reserved */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/gen/in.h>

#include "talk.h"
#include "talkd.h"
#include "net.h"
#include "process.h"

_PROTOTYPE(int main, (int argc, char *argv[]));

int opt_d = 0;
char myhostname[HOST_SIZE+1];

int main(argc, argv)
int argc;
char *argv[];
{
struct talk_request request;
struct talk_reply reply;

   if(argc > 1)
   	if(strcmp(argv[1], "-d") || argc > 2) {
   		fprintf(stderr, "Usage: talkd [-d]\n");
   		return(-1);
   	} else
		opt_d = 1;

   if(getuid() != 0) {
	fprintf(stderr, "talkd: Must be run as super user\n");
	return(-1);
   }

   if(gethostname(myhostname, HOST_SIZE) < 0) {
   	fprintf(stderr, "talkd: Error getting hostname\n");
   	return(-1);
   }

   if(NetInit()) {
   	fprintf(stderr, "talkd: Error in NetInit\n");
   	return(-1);
   }

   while(getrequest(&request) == 0) {
   	if(processrequest(&request, &reply)) break;
   	if(sendreply(&request, &reply)) break;
   }

   return(-1);
}
