/* net.h Copyright Michael Temari 08/01/1996 All Rights Reserved */

extern char luser[], ruser[];
extern char lhost[], rhost[];
extern char ltty[], rtty[];
extern udpport_t ctlport;
extern tcpport_t dataport;
extern ipaddr_t laddr, raddr;
extern int tcp_fd;

_PROTOTYPE(int NetInit, (void));
_PROTOTYPE(int getreply, (struct talk_reply *reply, int timeout));
_PROTOTYPE(int sendrequest, (struct talk_request *request, int here));
_PROTOTYPE(int NetConnect, (U16_t port));
_PROTOTYPE(int NetListen, (int timeout));
