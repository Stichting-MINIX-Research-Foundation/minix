/* cgiexec.c by Michael Temari 02/17/96
 *
 * This file is part of httpd.
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 * 02/08/2005 			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "http.h"
#include "config.h"
#include "net.h"

_PROTOTYPE(char **cgienv, (struct http_request *rq, struct http_reply *rp));
_PROTOTYPE(static int addenv, (char *name, char *value, char **buf, int *len));

int cgiexec(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
struct stat st;
char *prog;
int cmdpid;
int status;
char *argv[5];
int ifds[2];
int ofds[2];
static char cmd[2048];
char **cmdenv;
int dirflag = 0;

    if(stat(rp->realurl, &st)) {
    	if(errno == EACCES)
    		rp->status = HTTP_STATUS_FORBIDDEN;
    	else
    		rp->status = HTTP_STATUS_NOT_FOUND;
    	strcpy(rp->statusmsg, strerror(errno));
    	return(-1);
    }

    if((st.st_mode & S_IFMT) == S_IFDIR)
    	if(direxec != NULL) {
    		prog = direxec; dirflag = 1;
    	} else
    		return(0);
    else
    	prog = rp->realurl;

    /* check if prog is allowed to be exec'd */
    if(!dirflag && !(rp->urlaccess & URLA_EXEC))
    	return(0);

    /* if cannot exec mode then return */
    if( (st.st_mode & S_IXUSR) == 0 &&
    	(st.st_mode & S_IXGRP) == 0 &&
    	(st.st_mode & S_IXOTH) == 0 )
	return(0);

   if((cmdenv = cgienv(rq, rp)) == NULL) {
   	rp->status = HTTP_STATUS_SERVER_ERROR;
   	strcpy(rp->statusmsg, "Could not setup cgi environment");
   	return(-1);
   }

   argv[0] = prog;
   argv[1] = rp->realurl;
   argv[2] = rq->url;
   argv[3] = (char *)NULL;

   if(pipe(ifds) < 0) {
	rp->status = HTTP_STATUS_NOT_FOUND;
    	strcpy(rp->statusmsg, strerror(errno));
	return(-1);
   }

   if(pipe(ofds) < 0) {
	rp->status = HTTP_STATUS_NOT_FOUND;
    	strcpy(rp->statusmsg, strerror(errno));
    	close(ifds[0]); close(ifds[1]);
	return(-1);
   }

   if((cmdpid = fork()) < 0) {
	close(ifds[0]); close(ofds[0]);
	close(ifds[1]); close(ofds[1]);
	rp->status = HTTP_STATUS_NOT_FOUND;
    	strcpy(rp->statusmsg, strerror(errno));
	return(-1);
   }

   /* We don't know how much data is going to be passed back */
   rp->size = 0;

   if(cmdpid == 0) { /* Child */
#if 0
   	if((cmdpid = fork()) < 0) {
		close(ifds[0]); close(ofds[0]);
		close(ifds[1]); close(ofds[1]);
   		exit(-1);
   	}
   	if(cmdpid != 0) {
		close(ifds[0]); close(ofds[0]);
		close(ifds[1]); close(ofds[1]);
		exit(0);
	}
#endif
	setsid();
	close(ifds[0]); close(ofds[1]);
	dup2(ofds[0], 0);
	dup2(ifds[1], 1);
	dup2(ifds[1], 2);
	close(ifds[1]); close(ofds[0]);
	execve(argv[0], argv, cmdenv);
	exit(0);
   }

#if 0
   /* Get rid of Zombie child */
   (void) wait(&status);
#endif

   close(ifds[1]); close(ofds[0]);

   rp->fd = ifds[0];
   rp->ofd = ofds[1];
   rp->pid = cmdpid;

   if(rp->urlaccess & URLA_HEADERS)
   	rp->headers = -1;

   return(-1);
}

char **cgienv(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
static char buffer[4096];
char *p, *p2;
char **e;
int len;
char temp[20];

   p = buffer;
   len = sizeof(buffer);

   if(addenv("PATH", "/usr/local/bin:/bin:/usr/bin", &p, &len)) return(NULL);
   if(getenv("TZ") != (char *)NULL)
   	if(addenv("TZ", getenv("TZ"), &p, &len)) return(NULL);

   /* HACK - some of these are hardcoded and should not be MAT 3/17/96 */

   /* HTTP_ */

   if(addenv("SERVER_SOFTWARE", "Temari httpd/1.0", &p, &len)) return(NULL);
   if(addenv("SERVER_NAME", myhostname, &p, &len)) return(NULL);
   if(addenv("GATEWAY_INTERFACE", "CGI/1.1", &p, &len)) return(NULL);
   if(addenv("SERVER_PROTOCOL", "HTTP/1.0", &p, &len)) return(NULL);
   if(rq->port)
	sprintf(temp, "%u", rq->port);
   else
   	strcpy(temp, "80");
   if(addenv("SERVER_PORT", temp, &p, &len)) return(NULL);
   switch(rq->method) {
   	case HTTP_METHOD_GET:
		if(addenv("REQUEST_METHOD", "GET", &p, &len)) return(NULL);
		break;
   	case HTTP_METHOD_POST:
		if(addenv("REQUEST_METHOD", "POST", &p, &len)) return(NULL);
		break;
   	case HTTP_METHOD_HEAD:
		if(addenv("REQUEST_METHOD", "HEAD", &p, &len)) return(NULL);
		break;
   	case HTTP_METHOD_PUT:
		if(addenv("REQUEST_METHOD", "PUT", &p, &len)) return(NULL);
		break;
	default:
		if(addenv("REQUEST_METHOD", "UNKNOWN", &p, &len)) return(NULL);
   }
   if(addenv("PATH_INFO", "?", &p, &len)) return(NULL);
   if(addenv("PATH_TRANSLATED", "?", &p, &len)) return(NULL);
   if(addenv("SCRIPT_NAME", rq->url, &p, &len)) return(NULL);
   if(addenv("QUERY_STRING", rq->query, &p, &len)) return(NULL);
   if(addenv("REMOTE_HOST", rmthostname, &p, &len)) return(NULL);
   if(addenv("REMOTE_ADDR", rmthostaddr, &p, &len)) return(NULL);
   if(rq->authuser != (char *)NULL)
	if(addenv("AUTH_USER", rq->authuser, &p, &len)) return(NULL);
   /* AUTH_TYPE */
   /* REMOTE_USER */
   /* REMOTE_IDENT */
   if(rq->method == HTTP_METHOD_POST) {
	if(addenv("CONTENT_TYPE", "application/x-www-form-urlencoded", &p, &len)) return(NULL);
	sprintf(temp, "%lu", rq->size);
	if(addenv("CONTENT_LENGTH", temp, &p, &len)) return(NULL);
   }
   /* COOKIE */
   if(rq->cookie[0] != '\0')
   	if(addenv("COOKIE", rq->cookie, &p, &len)) return(NULL);
   /* HOST */
   if(addenv("HOST", rq->host, &p, &len)) return(NULL);

   if(len < 1) return(NULL);
   *p++ = '\0';

   p2 = buffer;
   e = (char **)p;
   while(*p2) {
  	if(len < sizeof(e)) return(NULL);
  	len -= sizeof(e);
  	*e++ = p2;
  	while(*p2) p2++;
  	p2++;
   }
   if(len < sizeof(e)) return(NULL);
   *e++ = NULL;

   return((char **)p);
}

static int addenv(name, value, buf, len)
char *name;
char *value;
char **buf;
int *len;
{
char *p;
int size;

   p = *buf;

   size = strlen(name)+1+strlen(value)+1;

   if(size > *len)
   	return(-1);

   sprintf(p, "%s=%s", name, value);

   p += size;
   *buf = p;
   *len -= size;

   return(0);
}
