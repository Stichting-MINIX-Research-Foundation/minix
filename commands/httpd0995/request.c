/* request.c
 *
 * This file is part of httpd.
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#ifdef _MINIX
#include <minix/minlib.h>
#endif
#include <errno.h>

#include "http.h"
#include "utility.h"
#include "config.h"

_PROTOTYPE(static void Timeout, (int sig));
_PROTOTYPE(static int getline, (char *buffer, int size));
_PROTOTYPE(static void authorize, (char *p, struct http_request *rq));
_PROTOTYPE(static void decurl, (char *u));

static int TimeOut;

static void Timeout(sig)
int sig;
{
   TimeOut = 1;
}

static int getline(buffer, size)
char *buffer;
int size;
{
char *p;
int s;

   p = buffer;

   while(p < (buffer + size - 1)) {
   	TimeOut = 0;
   	signal(SIGALRM, Timeout);
   	alarm(5*60);
	s = read(0, p, 1);
	alarm(0);
	if(TimeOut)
		return(-1);
	if(s != 1)
		return(-1);
	if(*p == '\n') break;
	p++;
   }
   *++p = '\0';

   p = &buffer[strlen(buffer) - 1];
   if(p >= buffer && (*p == '\r' || *p == '\n')) *p-- ='\0';
   if(p >= buffer && (*p == '\r' || *p == '\n')) *p-- ='\0';

   return(strlen(buffer));
}

static void authorize(p, rq)
char *p;
struct http_request *rq;
{
char *s;

   if(toupper(*p++) == 'B' &&
      toupper(*p++) == 'A' &&
      toupper(*p++) == 'S' &&
      toupper(*p++) == 'I' &&
      toupper(*p++) == 'C' &&
      toupper(*p++) == ' ') ;
   else
   	return;

   s = decode64(p);

   if((p = strchr(s, ':')) == (char *)NULL)
   	p = "";
   else
   	*p++ = '\0';

   strncpy(rq->authuser, s, sizeof(rq->authuser));
   strncpy(rq->authpass, p, sizeof(rq->authpass));

   return;
}

int getrequest(rq)
struct http_request *rq;
{
static char line[4096];
char *p, *p2, *ps;
int s, len;
struct vhost *ph;

   /* get request, it may be simple */

   s = getline(line, sizeof(line));
   if(s < 0)
	return(-1);

   if(dbglog != (FILE *)NULL) {
	fprintf(dbglog, "REQUEST: %s\n", line);
	fflush(dbglog);
   }

   /* clear http_request */
   memset(rq, 0, sizeof(*rq));
   rq->ifmodsince = (time_t) -1;

   /* assume simple request */
   rq->type = HTTP_REQUEST_TYPE_SIMPLE;

   /* parse the method */
   p = line;
   while(*p && !LWS(*p)) {
   	*p = toupper(*p);
   	p++;
   }
   if(*p) *p++ = '\0';

   if(!strcmp(line, "GET"))
	rq->method = HTTP_METHOD_GET; else
   if(!strcmp(line, "HEAD"))
	rq->method = HTTP_METHOD_HEAD; else
   if(!strcmp(line, "POST"))
	rq->method = HTTP_METHOD_POST; else
   if(!strcmp(line, "PUT"))
	rq->method = HTTP_METHOD_PUT; else
#if 0
   if(!strcmp(line, "OPTIONS"))
	rq->method = HTTP_METHOD_OPTIONS; else
   if(!strcmp(line, "PATCH"))
	rq->method = HTTP_METHOD_PATCH; else
   if(!strcmp(line, "COPY"))
	rq->method = HTTP_METHOD_COPY; else
   if(!strcmp(line, "MOVE"))
	rq->method = HTTP_METHOD_MOVE; else
   if(!strcmp(line, "DELETE"))
	rq->method = HTTP_METHOD_DELETE; else
   if(!strcmp(line, "LINK"))
	rq->method = HTTP_METHOD_LINK; else
   if(!strcmp(line, "UNLINK"))
	rq->method = HTTP_METHOD_UNLINK; else
   if(!strcmp(line, "TRACE"))
	rq->method = HTTP_METHOD_TRACE; else
   if(!strcmp(line, "WRAPPED"))
	rq->method = HTTP_METHOD_WRAPPED; else
#endif
	rq->method = HTTP_METHOD_UNKNOWN;

   /* parse the requested URI */
   p2 = rq->uri;
   len = sizeof(rq->uri) - 1;
   while(*p && !LWS(*p) && len > 0) {
	*p2++ = *p++;
	len--;
   }
   *p2 = '\0';

   /* eat up any leftovers if uri was too big */
   while(*p && !LWS(*p))
	p++;

   /* save for continued processing later */
   ps = p;

   /* parse the requested URL */
   p = rq->uri;
   p2 = rq->url;
   len = sizeof(rq->url) - 1;
   while(*p && !LWS(*p) && *p != '?' && len > 0) {
	*p2++ = *p++;
	len--;
   }
   *p2 = '\0';

   /* See if there is a query string */
   if(*p == '?') {
   	p++;
   	p2 = rq->query;
   	len = sizeof(rq->query) - 1;
   	while(*p && !LWS(*p) && len > 0) {
   		*p2++ = *p++;
		len--;
	}
   }

   /* eat up any leftovers */
   while(*p && !LWS(*p)) p++;

   if(rq->url[0] == '\0') {
   	rq->url[0] = '/';
   	rq->url[1] = '\0';
   }

   /* url is a decoded copy of the uri */
   decurl(rq->url);

   /* restore and continue processing */
   p = ps;

   /* if this is true it is a simple request */
   if(*p == '\0')
	return(0);

   /* parse HTTP version */
   while(*p && LWS(*p)) p++;
   if(toupper(*p++) != 'H') return(0);
   if(toupper(*p++) != 'T') return(0);
   if(toupper(*p++) != 'T') return(0);
   if(toupper(*p++) != 'P') return(0);
   if(        *p++  != '/') return(0);

   /* version major */
   rq->vmajor = 0;
   while((*p >= '0') && (*p <= '9'))
	rq->vmajor = rq->vmajor * 10 + (*p++ - '0');
   if(*p != '.')
	return(0);
   p++;

   /* version minor */
   rq->vminor = 0;
   while((*p >= '0') && (*p <= '9'))
	rq->vminor = rq->vminor * 10 + (*p++ - '0');
   if(*p)
	return(0);

   rq->type = HTTP_REQUEST_TYPE_FULL;

   p = rq->uri;

   /* check if it is a proxy request */
   if(toupper(*p++) == 'H' &&
      toupper(*p++) == 'T' &&
      toupper(*p++) == 'T' &&
      toupper(*p++) == 'P' &&
      toupper(*p++) == ':')
      	rq->type = HTTP_REQUEST_TYPE_PROXY;

   /* parse any header fields */
   while((s = getline(line, sizeof(line))) > 0) {
   	if(toupper(line[0]) == 'A' &&
   	   toupper(line[1]) == 'U')
		if(dbglog != (FILE *)NULL) {
			fprintf(dbglog, "REQUEST: Authorization:\n");
			fflush(dbglog);
		} else ;
	else
		if(dbglog != (FILE *)NULL) {
			fprintf(dbglog, "REQUEST: %s\n", line);
			fflush(dbglog);
		}
	p = line;
	while(*p && *p != ':') {
		*p = toupper(*p);
		p++;
	}
	if(*p != ':') continue;		/* bad header field, skip it */
	*p++ = '\0';
	while(*p && LWS(*p)) p++;

	/* header field value parsing here */
	if(!strcmp(line, "HOST")) {
		strncpy(rq->host, p, sizeof(rq->host));
		p2 = strrchr(rq->host, ':');
		if(p2 != (char *)NULL) {
			*p2++ = '\0';
			rq->port = atoi(p2);
		}
		/* if unknown virtual host then exit quietly */
		for(ph = vhost; ph != NULL; ph = ph->next) {
			if(!strcasecmp(ph->hname, "*")) break;
			if(!strcasecmp(ph->hname, rq->host)) break;
		}
		if(rq->type != HTTP_REQUEST_TYPE_PROXY)
			if(ph == NULL && vhost != NULL) return(1);
	} else
	if(!strcmp(line, "USER-AGENT"))
		strncpy(rq->useragent, p, sizeof(rq->useragent)); else
	if(!strcmp(line, "CONNECTION"))
		rq->keepopen = strcasecmp(p, "Keep-Alive") ? 0 : 1; else
	if(!strcmp(line, "IF-MODIFIED-SINCE"))
		rq->ifmodsince = httptime(p); else
	if(!strcmp(line, "CONTENT-LENGTH"))
		rq->size = atol(p); else
	if(!strcmp(line, "AUTHORIZATION")) {
		strncpy(rq->wwwauth, p, sizeof(rq->wwwauth));
		if(rq->type != HTTP_REQUEST_TYPE_PROXY)
			authorize(p, rq);
	} else
	if(!strcmp(line, "PROXY-AUTHORIZATION")) {
		if(rq->type == HTTP_REQUEST_TYPE_PROXY)
			authorize(p, rq);
	} else
	if(!strcmp(line, "DATE"))
		rq->msgdate = httptime(p); else
	if(!strcmp(line, "COOKIE")) {
		strncpy(rq->cookie, p, sizeof(rq->cookie)-1);
		rq->cookie[sizeof(rq->cookie)-1] = '\0';
	}
   }

   if(rq->type != HTTP_REQUEST_TYPE_PROXY)
	if(*rq->host == '\0' && vhost != NULL) return(1);

   if(dbglog != (FILE *)NULL && rq->authuser[0] != '\0') {
	fprintf(dbglog, "REQUEST: AuthUser=%s\n", rq->authuser);
	fflush(dbglog);
   }

   if(s < 0) {
	fprintf(stderr, "httpd: getrequest: Error getline (header fields)\n");
	return(-1);
   }

   return(0);
}

static void decurl(u)
char *u;
{
char *p;
char h1, h2;
char c;

   p = u;
   while(*p) {
	switch(*p) {
   		case '\0':
   			c = '\0';
   			break;
   		case '+':
   			c = ' ';
   			p++;
   			break;
   		case '%':
			h1 = '0';
			h2 = '0';
			p++;
			h1 = tolower(*p);
			if(*p) p++;
			h2 = tolower(*p);
			if(*p) p++;
			c = (h1 > '9') ? (10 + h1 - 'a') : (h1 - '0');
			c = 16 * c + ((h2 > '9') ? (10 + h2 - 'a') : (h2 - '0'));
			break;
		default:
			c = *p++;
	}
	*u++ = c;
   }
   *u = '\0';
}
