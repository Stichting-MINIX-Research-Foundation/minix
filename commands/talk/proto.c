/* proto.c Copyright Michael Temari 08/01/1996 All Rights Reserved */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <net/hton.h>
#include <net/gen/socket.h>
#include <net/gen/in.h>
#include <net/gen/inet.h>
#include <net/gen/tcp.h>
#include <net/gen/udp.h>

#include "talk.h"
#include "proto.h"
#include "net.h"
#include "screen.h"

_PROTOTYPE(static int TalkChk, (int gotreply, struct talk_reply *reply, char *msg));
_PROTOTYPE(static int TalkTrans, (int type, long id, struct talk_reply *reply, int here));

static char *AnswerMsgs[] = {
	"Success",
	"User Not Logged In",
	"Failure",
	"Remote Does Not Know who we are",
	"User is not accepting calls",
	"Are request was not know",
	"Incorrect Version",
	"Bad Address",
	"Bad Control Address"
};

static int TalkChk(gotreply, reply, msg)
int gotreply;
struct talk_reply *reply;
char *msg;
{
   if(!gotreply) {
	ScreenMsg(msg);
	return(-1);
   }
   if(reply->answer == SUCCESS) return(0);
   if(reply->answer < (sizeof(AnswerMsgs) / sizeof(AnswerMsgs[0])))
  	ScreenMsg(AnswerMsgs[reply->answer]);
   else
  	ScreenMsg("Bad Answer");

   return(-1);
}

static int TalkTrans(type, id, reply, here)
int type;
long id;
struct talk_reply *reply;
int here;
{
struct talk_request request;
int tries;
int gotreply;

   memset(&request, 0, sizeof(request));

   request.version = TALK_VERSION;
   request.type = type;
   request.id = id;
   request.addr.sa_family = htons(AF_INET);
   request.addr.sin_port = dataport;
   request.addr.sin_addr = laddr;
   request.ctl_addr.sa_family = htons(AF_INET);
   request.ctl_addr.sin_port = ctlport;
   request.ctl_addr.sin_addr = laddr;
   request.pid = getpid();
   strncpy(request.luser, luser, USER_SIZE);
   strncpy(request.ruser, ruser, USER_SIZE);
   strncpy(request.rtty,  rtty,  TTY_SIZE);

   tries = 0;
   gotreply = 0;
   while(!ScreenDone && tries++ < 3 && !gotreply) {
	if(!sendrequest(&request, here))
		if(!getreply(reply, 5))
			gotreply = 1;
	if(!gotreply) continue;
	if(reply->version != request.version ||
	   reply->type    != request.type)
	   	gotreply = 0;
   }
   return(gotreply);
}

int TalkInit()
{
struct talk_reply reply;
long id = 0;
long rid;
int s;
int ring;
char buff[32];

   /* Check if someone was calling us */
   ScreenMsg("Initiating Talk Protocol");

   /* Check is someone was calling us */
   s = TalkTrans(LOOK_UP, ++id, &reply, 0);

   /* Someone was calling us */
   if(s && reply.answer == SUCCESS) {
   	s = NetConnect(reply.addr.sin_port);
   	if(s == 1) {
   		ScreenMsg("Your party has hung up");
   		TalkTrans(DELETE, reply.id, &reply, 0);
   	}
   	return(s == 0 ? 0 : -1);
   }

   ScreenMsg("Ringing User");

   ring = 0;
   while(!ScreenDone && ring++ < 5) {
   	if(TalkChk(TalkTrans(ANNOUNCE, -1, &reply, 0),
   			&reply, "No response to are ring"))
   		return(-1);
   	rid = reply.id;
	sprintf(buff, "Ring #%d", ring);
	ScreenMsg(buff);
	if(ring == 1) {
   		if(TalkChk(TalkTrans(LEAVE_INVITE, ++id, &reply, 1),
   				&reply, "Could not leave are invitaion locally"))
			return(-1);
	}
   	s = NetListen(RING_WAIT);
   	if(s <= 0) {
   		TalkTrans(DELETE, reply.id, &reply, 1);
   		TalkTrans(DELETE, rid, &reply, 0);
   		return(s);
   	}
   }

   return(-1);
}
