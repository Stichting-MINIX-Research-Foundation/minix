/* police.c
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
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "http.h"
#include "utility.h"
#include "config.h"
#include "pass.h"

#define	MATCH_NONE	0
#define	MATCH_WILD	1
#define	MATCH_FULL	2

_PROTOTYPE(static int authaccess, (struct http_request *rq, struct http_reply *rp));
_PROTOTYPE(static void purl, (struct http_request *rq, struct http_reply *rp));
_PROTOTYPE(static char *virt, (char *to, char *host));

static int authaccess(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
struct auth *auth;
struct authuser *pu;

   /* set authorization to be checked against */
   if(rq->type == HTTP_REQUEST_TYPE_PROXY)
   	auth = proxyauth;
   else
   	auth = rp->auth;

   /* no authorization so no access to anyone */
   if(auth == NULL) {
   	rp->status = HTTP_STATUS_FORBIDDEN;
   	strcpy(rp->statusmsg, "No Authoriation");
   	return(-1);
   }

   /* access must be R for PROXY */
   if(rq->type == HTTP_REQUEST_TYPE_PROXY)
	if(!(auth->urlaccess & URLA_READ)) {
   		rp->status = HTTP_STATUS_FORBIDDEN;
   		strcpy(rp->statusmsg, "Proxy not authorized");
   		return(-1);
	}

   /* no password file so it is a free for all */
   if(auth->passwdfile == NULL)
	return(0);

   /* they did not give us an authorized user */
   if(rq->authuser[0] == '\0') {
	if(rq->type == HTTP_REQUEST_TYPE_PROXY)
   		rp->status = HTTP_STATUS_PROXY_AUTH_REQRD;
   	else
   		rp->status = HTTP_STATUS_UNAUTHORIZED;
   	strcpy(rp->statusmsg, "No Authorized User Given");
   	return(-1);
   }

   /* check if user okay */
   pu = auth->users;
   if(pu == NULL)
	;	/* no user list we allow anyone in file */
   else {
	while(pu != NULL) {
		if(!strcmp(pu->user, rq->authuser))
			break;
		pu = pu->next;
	}
	/* user is not in list so no access */
	if(pu == NULL) {
		if(rq->type == HTTP_REQUEST_TYPE_PROXY)
   			rp->status = HTTP_STATUS_PROXY_AUTH_REQRD;
   		else
			rp->status = HTTP_STATUS_UNAUTHORIZED;
		strcpy(rp->statusmsg, "Forbidden User not authorized");
		return(-1);
	}
   }

   /* check if password file exists, if not no access */
   if(passfile(auth->passwdfile)) {
   	rp->status = HTTP_STATUS_FORBIDDEN;
   	strcpy(rp->statusmsg, "Invalid passwd file");
   	return(-1);
   }

   /* check if user in password file, if not no access */
   if(passuser(auth->passwdfile, rq->authuser)) {
	if(rq->type == HTTP_REQUEST_TYPE_PROXY)
   		rp->status = HTTP_STATUS_PROXY_AUTH_REQRD;
   	else
		rp->status = HTTP_STATUS_UNAUTHORIZED;
	strcpy(rp->statusmsg, "Forbidden Bad User");
	return(-1);
   }

   /* check if a password exists, if not no access */
   if(passnone(auth->passwdfile, rq->authuser)) {
	if(rq->type == HTTP_REQUEST_TYPE_PROXY)
   		rp->status = HTTP_STATUS_PROXY_AUTH_REQRD;
   	else
		rp->status = HTTP_STATUS_UNAUTHORIZED;
	strcpy(rp->statusmsg, "Forbidden no password");
	return(-1);
   }

   /* check if password matches, if not no access */
   if(passpass(auth->passwdfile, rq->authuser, rq->authpass)) {
	if(rq->type == HTTP_REQUEST_TYPE_PROXY)
   		rp->status = HTTP_STATUS_PROXY_AUTH_REQRD;
   	else
		rp->status = HTTP_STATUS_UNAUTHORIZED;
	strcpy(rp->statusmsg, "Forbidden bad password");
	return(-1);
   }

   /* whew, all the checks passed so I guess we let them have it */
   return(0);
}

int police(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
int size;
struct stat st;
struct dirsend *ds;

   purl(rq, rp);

   rp->mtype = "text/html";

#ifdef DEBUG
   fprintf(stderr, "httpd: Trying %s\n", rp->realurl);
#endif

   /* now check authorizations */
   if(authaccess(rq, rp)) {
	/* Don't give them any details why authorization failed */
   	strcpy(rp->statusmsg, "No Access Granted");
	return(0);
   }

   /* a proxy request only needs an authorization check */
   if(rq->type == HTTP_REQUEST_TYPE_PROXY)
	return(0);

   /* check access to real url */
   if(stat(rp->realurl, &st)) {
	if(errno == EACCES)
		rp->status = HTTP_STATUS_FORBIDDEN;
	else
		rp->status = HTTP_STATUS_NOT_FOUND;
	strcpy(rp->statusmsg, strerror(errno));
	/* a PUT and NOT FOUND is okay since we are creating */
	if(rq->method != HTTP_METHOD_PUT || rp->status != HTTP_STATUS_NOT_FOUND)
		return(0);
   }

   /* If it is a directory do the appropriate thang! */
   if(rq->method == HTTP_METHOD_GET || rq->method == HTTP_METHOD_HEAD)
   if((st.st_mode & S_IFMT) == S_IFDIR) {
	if(rq->url[strlen(rq->url) - 1] != '/') {
		strncat(rq->url, "/", sizeof(rq->url) - strlen(rq->url));
		rp->status = HTTP_STATUS_MOVED_TEMP;
		sprintf(rp->statusmsg, "Moved to %s", rq->url);
		return(0);
	}
	size = strlen(rq->url);
	ds = dirsend;
	while(ds != NULL) {
		strncpy(rq->url+size, ds->file, sizeof(rq->url)-size);
		purl(rq, rp);
		if(stat(rp->realurl, &st)) {
			if(errno == EACCES)
				rp->status = HTTP_STATUS_FORBIDDEN;
			else
			if(errno != ENOENT)
				rp->status = HTTP_STATUS_NOT_FOUND;
		} else
			break;
		if(rp->status != HTTP_STATUS_OK) {
			strcpy(rp->statusmsg, strerror(errno));
			return(0);
		}
		ds = ds->next;
	}
	if(ds == NULL) {
		rq->url[size] = '\0';
		purl(rq, rp);
		if(stat(rp->realurl, &st)) {
	   		if(errno == EACCES)
   				rp->status = HTTP_STATUS_FORBIDDEN;
   			else
				rp->status = HTTP_STATUS_NOT_FOUND;
			strcpy(rp->statusmsg, strerror(errno));
			return(0);
		}
	}
   }

   if(rq->method == HTTP_METHOD_PUT && !(rp->urlaccess & URLA_WRITE)) {
	rp->status = HTTP_STATUS_METHOD_NOT_ALLOWED;
	strcpy(rp->statusmsg, "Method not allowed");
	return(0);
   }

   if(rp->status == HTTP_STATUS_OK) {
	/* Here is where we check if it is a program or script to run */
	if(cgiexec(rq, rp))
   		return(0);

	if((st.st_mode & S_IFMT) == S_IFDIR) {
		rp->status = HTTP_STATUS_NOT_FOUND;
		strcpy(rp->statusmsg, "Directory listing not available");
		return(0);
	}

	if((st.st_mode & S_IFMT) != S_IFREG) {
		rp->status = HTTP_STATUS_NOT_FOUND;
		strcpy(rp->statusmsg, "Not a regular file");
		return(0);
	}
   }

   /* open the URL for updating */
   if(rq->method == HTTP_METHOD_PUT) {
	rp->status = HTTP_STATUS_OK;
	strcpy(rp->statusmsg, "OK");
	rp->ofd = open(rp->realurl, O_WRONLY | O_CREAT | O_TRUNC);
	if(rp->ofd < 0) {
   		if(errno == EACCES)
   			rp->status = HTTP_STATUS_FORBIDDEN;
   		else
			rp->status = HTTP_STATUS_NOT_FOUND;
		strcpy(rp->statusmsg, strerror(errno));
		return(0);
	}
	return(0);
   }

   if(!(rp->urlaccess & URLA_READ)) {
   	rp->status = HTTP_STATUS_FORBIDDEN;
   	strcpy(rp->statusmsg, "No way...");
	return(0);
   }

   rp->mtype = mimetype(rp->realurl);

   rp->size = st.st_size;
   rp->modtime = st.st_mtime;

   /* open the url if it is a file */
   rp->fd = open(rp->realurl, O_RDONLY);
   if(rp->fd < 0) {
	if(errno == EACCES)
   		rp->status = HTTP_STATUS_FORBIDDEN;
   	else
		rp->status = HTTP_STATUS_NOT_FOUND;
	strcpy(rp->statusmsg, strerror(errno));
	return(0);
   }

   return(0);
}

static void purl(rq, rp)
struct http_request *rq;
struct http_reply *rp;
{
struct vpath *pv;
int gotreal, gotperm;
char *p;
int match;
int len;

   gotreal = 0; gotperm = 0;

#ifdef DEBUG
   fprintf(stderr, "httpd: Processing url = \"%s\"\n", rq->url);
#endif

   /* remove any .. references */
   p = rq->url;
   while(*p) {
	while(*p && *p != '/') p++;
	if(*p != '/') continue;
	p++;
	if(*p != '.') continue;
	p++;
	if(*p != '.') continue;
	p++;
	strcpy(p - 3, p);
	p = p - 3;
   }

   for(pv = vpath; pv != NULL; pv = pv->next) {
   	len = strlen(pv->from) - 1;
   	if(pv->from[len] == '*' || pv->from[len] == '$')
   		if(len == 0)
   			match = MATCH_WILD;
   		else
   			match = strncmp(rq->url, pv->from, len) ? MATCH_NONE : MATCH_WILD;
   	else
   		if(!strcmp(rq->url, pv->from))
   			match = MATCH_FULL;
   		else
   			match = MATCH_NONE;
#ifdef DEBUG
   	fprintf(stderr, "httpd: Trying \"%s\" %d %d %d %s\n",
		pv->from, match, gotreal, gotperm, pv->auth->name);
#endif
   	if(match != MATCH_NONE) {
		gotperm = 1;
		rp->auth = pv->auth;
		if(pv->urlaccess == -1 && rp->auth != NULL)
			rp->urlaccess = rp->auth->urlaccess;
		else
			rp->urlaccess = pv->urlaccess;
   		if(strcmp(pv->to, ".")) {
   			gotreal = 1;
   			strncpy(rp->realurl, virt(pv->to, rq->host), sizeof(rp->realurl));
			rp->realurl[sizeof(rp->realurl)-1] = '\0';
			if(match == MATCH_WILD && pv->from[len] != '$') {
   				strncat(rp->realurl, rq->url+len, sizeof(rp->realurl) - strlen(rp->realurl));
				rp->realurl[sizeof(rp->realurl)-1] = '\0';
			}
   		}
   	}
   	if(match == MATCH_FULL) break;
   }

   if(rp->urlaccess == -1) rp->urlaccess = mkurlaccess("");

   if(!gotreal) {
	strncpy(rp->realurl, rq->url, sizeof(rp->realurl));
	rp->realurl[sizeof(rp->realurl)-1] = '\0';
   }

   if(!gotperm)
   	rp->auth = NULL;

#ifdef DEBUG
   fprintf(stderr, "DEBUG: url = \"%s\"  realurl = \"%s\"  auth = \"%s\"\n",
	rq->url, rp->realurl, ((rp->auth == NULL) ? "No Access" : rp->auth->name));
   fprintf(stderr, "DEBUG: query = %s\n", rq->query);
#endif

   return;
}

static char *virt(to, host)
char *to;
char *host;
{
static char vroot[256];
struct vhost *ph;

#ifdef DEBUG
fprintf(stderr, "virt: %s %s\n", to, host);
#endif

   if(vhost == NULL) return(to);

   if(to[0] != '/') return(to);
   if(to[1] != '/') return(to);
   if(to[2] != '/') return(to);

   vroot[0] = '\0';

   for(ph = vhost; ph != NULL; ph = ph->next) {
#ifdef DEBUG
   	fprintf(stderr, "ph: %s %s %s\n", ph->hname, ph->root, vroot);
#endif
   	if(!strcmp(ph->hname, "*") && vroot[0] == '\0')
   		strncpy(vroot, ph->root, sizeof(vroot));
   	if(!strcasecmp(ph->hname, host)) {
   		strncpy(vroot, ph->root, sizeof(vroot));
   		break;
   	}
   }

   strncat(vroot, to+3, sizeof(vroot));

#ifdef DEBUG
   fprintf(stderr, "vroot: %s\n", vroot);
#endif

   return(vroot);
}
