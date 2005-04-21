/* process.c Copyright Michael Temari 07/22/1996 All Rights Reserved */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <net/hton.h>
#include <net/gen/socket.h>
#include <net/gen/in.h>
#include <net/gen/netdb.h>

#include "talk.h"
#include "talkd.h"
#include "process.h"
#include "finduser.h"

struct entry {
	struct entry *prev;
	struct talk_request rq;
	time_t expire;
	struct entry *next;
};

_PROTOTYPE(static int announce, (struct talk_request *request, char *rhost));
_PROTOTYPE(static struct talk_request *lookup, (struct talk_request *request, int type));
_PROTOTYPE(static int addreq, (struct talk_request *request));
_PROTOTYPE(static delete_invite, (long id));
_PROTOTYPE(static long nextid, (void));
_PROTOTYPE(static void delete, (struct entry *e));

static struct entry *entry = (struct entry *)NULL;

int processrequest(request, reply)
struct talk_request *request;
struct talk_reply *reply;
{
char *p;
struct talk_request *rq;
struct hostent *hp;

   reply->version = TALK_VERSION;
   reply->type = request->type;
   reply->answer = 0;
   reply->junk = 0;
   reply->id = htonl(0);


   /* check version */
   if(request->version != TALK_VERSION) {
   	reply->answer = BADVERSION;
   	return(0);
   }

   /* check address family */
   if(ntohs(request->addr.sa_family) != AF_INET) {
   	reply->answer = BADADDR;
   	return(0);
   }

   /* check control address family */
   if(ntohs(request->ctl_addr.sa_family) != AF_INET) {
   	reply->answer = BADCTLADDR;
   	return(0);
   }

   /* check local name */
   p = request->luser;
   while(*p)
   	if(!isprint(*p)) {
   		reply->answer = FAILED;
   		return(0);
   	} else
   		p++;

   switch(request->type) {
   	case ANNOUNCE:
		reply->answer = find_user(request->ruser, request->rtty);
		if(reply->answer != SUCCESS) break;
		hp = gethostbyaddr((char *)&request->ctl_addr.sin_addr, sizeof(ipaddr_t), AF_INET);
		if(hp == (struct hostent *)NULL) {
			reply->answer = MACHINE_UNKNOWN;
			break;
		}
		if((rq = lookup(request, 1)) == (struct talk_request *)NULL) {
			reply->id = addreq(request);
			reply->answer = announce(request, hp->h_name);
			break;
		}
		if(ntohl(request->id) > ntohl(rq->id)) {
			rq->id = nextid();
			reply->id = rq->id;
			reply->answer = announce(request, hp->h_name);
		} else {
			reply->id = rq->id;
			reply->answer = SUCCESS;
		}
   		break;
   	case LEAVE_INVITE:
		rq = lookup(request, 1);
		if(rq == (struct talk_request *)NULL)
			reply->id = addreq(request);
		else {
			reply->id = rq->id;
			reply->answer = SUCCESS;
		}
   		break;
   	case LOOK_UP:
		if((rq = lookup(request, 0)) == (struct talk_request *)NULL)
			reply->answer = NOT_HERE;
		else {
			reply->id = rq->id;
			memcpy((char *)&reply->addr, (char *)&rq->addr, sizeof(reply->addr));
			reply->answer = SUCCESS;
		}
   		break;
   	case DELETE:
		reply->answer = delete_invite(request->id);
   		break;
   	default:
   		reply->answer = UNKNOWN_REQUEST;
   }

   return(0);
}

static int announce(request, rhost)
struct talk_request *request;
char *rhost;
{
char tty[5+TTY_SIZE+1];
struct stat st;
FILE *fp;
time_t now;
struct tm *tm;

   sprintf(tty, "/dev/%s", request->rtty);

   if(stat(tty, &st) < 0)
   	return(PERMISSION_DENIED);

   if(!(st.st_mode & S_IWGRP))
   	return(PERMISSION_DENIED);

   if((fp = fopen(tty, "w")) == (FILE *)NULL)
   	return(PERMISSION_DENIED);

   (void) time(&now);

   tm = localtime(&now);

   fprintf(fp, "\007\007\007\rtalkd: Message from talkd@%s at %d:%02d:%02d\r\n",
		myhostname, tm->tm_hour, tm->tm_min, tm->tm_sec);
   fprintf(fp, "talkd: %s@%s would like to talk to you\r\n",
   		request->luser, rhost);
   fprintf(fp, "talkd: to answer type:  talk %s@%s\r\n",
   		request->luser, rhost);

   fclose(fp);

   return(SUCCESS);
}

static struct talk_request *lookup(request, type)
struct talk_request *request;
int type;
{
time_t now;
struct entry *e;

   (void) time(&now);

   for(e = entry; e != (struct entry *)NULL; e = e->next) {
	if(now > e->expire) {
		delete(e);
		continue;
	}
	if(type == 0) {
		if(!strncmp(request->luser, e->rq.ruser, USER_SIZE) &&
		   !strncmp(request->ruser, e->rq.luser, USER_SIZE) &&
		   e->rq.type == LEAVE_INVITE)
			return(&e->rq);
	} else {
		if(request->type == e->rq.type &&
	  	   request->pid == e->rq.pid &&
		   !strncmp(request->luser, e->rq.luser, USER_SIZE) &&
		   !strncmp(request->ruser, e->rq.ruser, USER_SIZE)) {
			e->expire = now + MAX_LIFE;
			return(&e->rq);
		}
	}
   }
   return((struct talk_request *)NULL);
}

static int addreq(request)
struct talk_request *request;
{
time_t now;
struct entry *e;

   (void) time(&now);
   request->id = nextid();
   e = (struct entry *) malloc(sizeof(struct entry));
   if(e == (struct entry *)NULL) {
   	fprintf(stderr, "talkd: out of memory in insert table\n");
   	exit(1);
   }
   e->expire = now + MAX_LIFE;
   memcpy((char *)&e->rq, (char *)request, sizeof(struct talk_request));
   e->next = entry;
   if(e->next != (struct entry *)NULL)
   	e->next->prev = e;
   e->prev = (struct entry *)NULL;
   entry = e;
   return(request->id);
}

static int delete_invite(id)
long id;
{
time_t now;
struct entry *e;

   (void) time(&now);

   for(e = entry; e != (struct entry *)NULL; e = e->next) {
	if(now > e->expire) {
		delete(e);
		continue;
	}
	if(e->rq.id == id) {
		delete(e);
		return(SUCCESS);
	}
   }
   return(NOT_HERE);
}

static void delete(e)
struct entry *e;
{
   if(e == (struct entry *)NULL) return;

   if(entry == e)
	entry = e->next;
   else
	if(e->prev != (struct entry *)NULL)
		e->prev->next = e->next;

   if(e->next != (struct entry *)NULL)
	e->next->prev = e->prev;

   free((char *)e);

   return;
}

static long nextid()
{
static long id = 0;

   id++;
   if(id <= 0) id = 1;
   return(htonl(id));
}
