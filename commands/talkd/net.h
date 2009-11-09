/* net.h Copyright Michael Temari 07/22/1996 All Rights Reserved */

_PROTOTYPE(int NetInit, (void));
_PROTOTYPE(int getrequest, (struct talk_request *request));
_PROTOTYPE(int sendreply, (struct talk_request *request, struct talk_reply *reply));
