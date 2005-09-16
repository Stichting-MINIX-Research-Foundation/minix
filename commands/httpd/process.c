/* process.c
 *
 * This file is part of httpd.
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Relase	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002			Michael Temari <Michael@TemWare.Com>
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "config.h"
#include "http.h"
#include "utility.h"

int processrequest(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
   /* clear out http_reply */
   memset(rp, 0, sizeof(*rp));
   rp->status = HTTP_STATUS_OK;
   strcpy(rp->statusmsg, "OK");
   rp->modtime = (time_t) -1;
   rp->urlaccess = -1;
   rp->size = 0;
   rp->fd = -1;
   rp->ofd = -1;
   rp->pid = 0;

   /* Simple requests can only be a GET */
   if(rq->type == HTTP_REQUEST_TYPE_SIMPLE && rq->method != HTTP_METHOD_GET) {
   	rp->status = HTTP_STATUS_BAD_REQUEST;
   	strcpy(rp->statusmsg, "Bad request");
   	return(0);
   }

   /* I don't know this method */
   if(rq->method == HTTP_METHOD_UNKNOWN) {
	rp->status = HTTP_STATUS_NOT_IMPLEMENTED;
	strcpy(rp->statusmsg, "Method not implemented");
	return(0);
   }

   /* Check for access and real location of url */
   if(police(rq, rp))
   	return(-1);

   /* We're done if there was an error accessing the url */
   if(rp->status != HTTP_STATUS_OK)
   	return(0);
		
   /* Check to see if we have a newer version for them */
   if(rq->method == HTTP_METHOD_GET)
	if(rq->ifmodsince != (time_t) -1)
   		if(rq->ifmodsince < time((time_t *)NULL))
   			if(rp->modtime != (time_t) -1 && rp->modtime <= rq->ifmodsince) {
   				rp->status = HTTP_STATUS_NOT_MODIFIED;
   				strcpy(rp->statusmsg, "Not modified");
   				close(rp->fd);
   				rp->fd = -1;
   				return(0);
   			}

   rp->status = HTTP_STATUS_OK;
   strcpy(rp->statusmsg, "OK");

   if(rp->size != 0)
	rp->keepopen = rq->keepopen;

   return(0);
}
