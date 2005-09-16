/* httpd.c
 *
 * httpd	A Server implementing the HTTP protocol.
 *
 * usage:	tcpd http httpd &
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 * 07/04/2003			Al Woodhull <awoodhull@hampshire.edu>
 *
 */

#include <stdlib.h>	
#include <sys/types.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "http.h"
#include "utility.h"
#include "net.h"
#include "config.h"

FILE *stdlog = (FILE *)NULL;
FILE *dbglog = (FILE *)NULL;

char umsg[80];

_PROTOTYPE(int main, (int argc, char *argv[]));

struct http_request request;
struct http_reply reply;

int main(argc, argv)
int argc;
char *argv[];
{
char *prog;
int opt_t;
char *cfg = (char *)NULL;
struct passwd *pwd;
int s;

   strcpy(umsg, "Usage: ");
   strcat(umsg, argv[0]);
   strcat(umsg, " [-t|v] [config_file]\n");

   /* parse program name */
   prog = strrchr(*argv, '/');
   if(prog == (char *)NULL)
   	prog = *argv;
   else
   	prog++;
   argv++;
   argc--;

   /* Any options */
   if(argc)
        if(argv[0][0] == '-') {
             switch (argv[0][1]) {
                  case 't' : opt_t = 1;
                             argv++;
                             argc--;
                             break;
                  case 'v' : fprintf(stderr, VERSION"\n"); 
                             exit(EXIT_SUCCESS);
                             break;
                  default  : fprintf(stderr, VERSION"\n"); 
                             fprintf(stderr, umsg); 
                             exit(EXIT_FAILURE);
             }
        }

   /* Did they specify an alternate configuration file? */
   if(argc) {
   	cfg = *argv++;
   	argc--;
   }

   /* Read the configuration settings */
   if(readconfig(cfg, opt_t)) {
   	fprintf(stderr, "httpd: Error reading configuration file.\n");
   	return(-1);
   }

   /* Option t is to test configuration only */
   if(opt_t)
	return(0);

   /* Open log file for append if it exists */
   if(LogFile != NULL)
	if((stdlog = fopen(LogFile, "r")) != (FILE *)NULL) {
		fclose(stdlog);
		stdlog = fopen(LogFile, "a");
	}

   /* Open debug log file for append if it exists */
   if(DbgFile != NULL)
	if((dbglog = fopen(DbgFile, "r")) != (FILE *)NULL) {
		fclose(dbglog);
		dbglog = fopen(DbgFile, "a");
   }

#if 0
   /* Get some network information */
   GetNetInfo();
#endif

   /* If user defined then prepare to secure as user given */
   if(User != NULL)
	if((pwd = getpwnam(User)) == (struct passwd *)NULL) {
   		fprintf(stderr, "httpd: unable to find user %s\n", User);
  	 	return(-1);
	}

   /* If Chroot defined then secure even more by doing a chroot */
   if(Chroot != NULL) {
	if(chroot(Chroot)) {
		fprintf(stderr, "httpd: unable to chroot\n");
		return(-1);
	}
	if(chdir("/")) {
   		fprintf(stderr, "httpd: unable to chroot\n");
   		return(-1);
	}
   }

   /* If user defined then secure as user given */
   if(User != NULL)
	if(setgid(pwd->pw_gid) || setuid(pwd->pw_uid)) {
   		fprintf(stderr, "httpd: unable to set user\n");
   		return(-1);
	}

#if DAEMON
   /* Standalone? */
   if (strncmp(prog, "in.", 3) != 0) {
       /* Does not start with "in.", so not started from inetd/tcpd. */
       /* XXX - Port name/number should be a config file option. */
       daemonloop("http");
   }
#endif

   /* Get some network information */
   GetNetInfo();

   /* log a connection */
   if(dbglog != (FILE *)NULL) {
	fprintf(dbglog, "CONNECT: %d %s %s\n", getpid(),
		rmthostname, logdate((time_t *)NULL));
	fflush(dbglog);
   }

   /* loop getting, processing and replying to requests */
   while(!(s = getrequest(&request))) {
	if(processrequest(&request, &reply)) break;
	if(stdlog != (FILE *)NULL) {
		fprintf(stdlog, "%s %s %d %d %s\n",
			logdate((time_t *)NULL), rmthostname,
			request.method, reply.status, request.url);
		fflush(stdlog);
	}
	if(sendreply(&reply, &request)) break;
	if(!reply.keepopen) break;
   }
   if(s == 1 && stdlog != (FILE *)NULL) {
	fprintf(stdlog, "%s %s %d %d %s\n",
		logdate((time_t *)NULL), rmthostname,
		request.method, 999, request.url);
	fflush(stdlog);
   }

   return(0);
}
