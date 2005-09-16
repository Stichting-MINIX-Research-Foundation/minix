/* reply.c
 *
 * This file is part of httpd.
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002 			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "http.h"
#include "utility.h"
#include "net.h"
#include "config.h"

#define	SERVER	"Server: "VERSION

_PROTOTYPE(static void GotAlarm, (int sig));
_PROTOTYPE(static int sendout, (char *data));

static void GotAlarm(sig)
int sig;
{
}

static int sendout(data)
char *data;
{
   if(strlen(data) > 0)
	write(1, data, strlen(data));
   write(1, "\r\n", 2);
   if(dbglog != (FILE *)NULL) {
	fprintf(dbglog, "REPLY: %s\n", data);
	fflush(dbglog);
   }

   return(0);
}

int sendreply(rp, rq)
struct http_reply *rp;
struct http_request *rq;
{
int s;
int s2;
int e;
static char buffer[8192];

   if(rq->type != HTTP_REQUEST_TYPE_PROXY)
   /* We're receiving data from a */
   if(rq->method == HTTP_METHOD_POST ||
     (rq->method == HTTP_METHOD_PUT && rp->status == HTTP_STATUS_OK)) {
   	if(rq->type != HTTP_REQUEST_TYPE_FULL)
   		return(0);
   	if(rq->method == HTTP_METHOD_PUT)
   		rp->status = HTTP_STATUS_CREATED;
   	else
   		rp->status = HTTP_STATUS_OK;
   	while(rq->size != 0) {
   		s = read(0, buffer, (rq->size > sizeof(buffer)) ? sizeof(buffer) : rq->size);
   		if(s <= 0) {
   			rp->status = HTTP_STATUS_SERVER_ERROR;
   			strcpy(rp->statusmsg, strerror(errno));
   			close(rp->fd);
   			close(rp->ofd);
   			break;
   		}
   		rq->size -= s;
   		s2 = write(rp->ofd, buffer, s);
   		if(s2 != s) break;
   	}
   }

   if(rp->status != HTTP_STATUS_OK && rp->status != HTTP_STATUS_CREATED &&
      rp->status != HTTP_STATUS_NOT_MODIFIED)
	rp->keepopen = 0;

   if(rp->status == HTTP_STATUS_NOT_MODIFIED) {
	sprintf(buffer, "<h2>Error %03d %s</h2>",
		rp->status, rp->statusmsg);
	rp->size = strlen(buffer);
	rp->keepopen = rq->keepopen;
   }

   if(!rp->headers) {

   if((rq->type == HTTP_REQUEST_TYPE_PROXY && rp->status != HTTP_STATUS_OK) ||
       rq->type == HTTP_REQUEST_TYPE_FULL) {
	sprintf(buffer, "HTTP/%d.%d %03d %s",
		rq->vmajor, rq->vminor, rp->status, rp->statusmsg);
	sendout(buffer);
	sendout(SERVER);
	if(rp->status == HTTP_STATUS_MOVED_PERM ||
	   rp->status == HTTP_STATUS_MOVED_TEMP) {
#if 1
	   	sprintf(buffer, "Location: %s", rq->url);
#else
	   	sprintf(buffer, "Location: http://%s%s", myhostname, rq->url);
#endif
	   	sendout(buffer);
	}
	if(rp->keepopen)
		sendout("Connection: Keep-Alive");
	else
		sendout("Connection: Close");
	if(rp->status == HTTP_STATUS_UNAUTHORIZED && rp->auth != NULL) {
		sprintf(buffer, "WWW-Authenticate: Basic realm=\"%s\"", rp->auth->desc);
		sendout(buffer);
	}
	if(rp->status == HTTP_STATUS_PROXY_AUTH_REQRD && proxyauth != NULL) {
		sprintf(buffer, "Proxy-Authenticate: Basic realm=\"%s\"", proxyauth->desc);
		sendout(buffer);
	}
	if(rp->modtime != (time_t) -1) {
		sprintf(buffer, "Last-Modified: %s", httpdate(&rp->modtime));
		sendout(buffer);
	}
	if(rp->size != 0) {
		sprintf(buffer, "Content-Length: %lu", rp->size);
		sendout(buffer);
	}
	if(rp->status == HTTP_STATUS_OK) {
		sprintf(buffer, "Content-Type: %s", rp->mtype);
		sendout(buffer);
	} else
		sendout("Content-Type: text/html");
	if(!rp->headers)
		sendout("");
   } else
	if(rp->status != HTTP_STATUS_OK)
		return(0);
   }

   if(rp->status != HTTP_STATUS_OK && rp->status != HTTP_STATUS_CREATED) {
	sprintf(buffer, "<h2>Error %03d %s</h2>",
		rp->status, rp->statusmsg);
	sendout(buffer);
	return(0);
   }

   if(rq->type == HTTP_REQUEST_TYPE_PROXY) {
   	proxy(rq, rp);
   	return(0);
   }

   /* send out entity body */
   if(rq->method == HTTP_METHOD_GET || rq->method == HTTP_METHOD_POST) {
   	errno = 0;
   	while(1) {
   		alarm(0);
   		signal(SIGALRM, GotAlarm);
   		alarm(10);
   		s = read(rp->fd, buffer, sizeof(buffer));
   		e = errno;
   		alarm(0);
   		if(s > 0) {
			s2 = write(1, buffer, s);
			e = errno;
			if(s2 != s) break;
			continue;
		}
		if(s == 0) break;
		if(s < 0 && e != EINTR) break;
   		signal(SIGALRM, GotAlarm);
   		alarm(2);
   		s = read(0, buffer, 1);
   		e = errno;
   		alarm(0);
   		if(s < 0 && e != EINTR) break;
	}
   }

   close(rp->fd);
   rp->fd = -1;
   if(rp->ofd != -1)
	close(rp->ofd);
   if(rp->pid != 0 && e != 0) {
   	kill(-rp->pid, SIGHUP);
   	rp->pid = 0;
   }

   return(0);
}
