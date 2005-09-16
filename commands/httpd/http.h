/* http.h
 *
 * This file is part of httpd.
 *
 * 02/17/1996 			Michael Temari <Michael@TemWare.Com>
 * 07/07/1996 Initial Release	Michael Temari <Michael@TemWare.Com>
 * 12/29/2002			Michael Temari <Michael@TemWare.Com>
 *
 */

#define	INDEX_FILE_NAME	"index.html"

#define	HTTP_REQUEST_TYPE_SIMPLE	0
#define	HTTP_REQUEST_TYPE_FULL		1
#define	HTTP_REQUEST_TYPE_PROXY		2

#define	HTTP_METHOD_UNKNOWN	0
#define	HTTP_METHOD_OPTIONS	1
#define	HTTP_METHOD_GET		2
#define	HTTP_METHOD_HEAD	3
#define	HTTP_METHOD_POST	4
#define	HTTP_METHOD_PUT		5
#define	HTTP_METHOD_PATCH	6
#define	HTTP_METHOD_COPY	7
#define	HTTP_METHOD_MOVE	8
#define	HTTP_METHOD_DELETE	9
#define	HTTP_METHOD_LINK	10
#define	HTTP_METHOD_UNLINK	11
#define	HTTP_METHOD_TRACE	12
#define	HTTP_METHOD_WRAPPED	13

#define	HTTP_STATUS_OK			200
#define	HTTP_STATUS_CREATED		201
#define	HTTP_STATUS_ACCEPTED		202
#define	HTTP_STATUS_NO_CONTENT		204
#define	HTTP_STATUS_MOVED_PERM		301
#define	HTTP_STATUS_MOVED_TEMP		302
#define	HTTP_STATUS_NOT_MODIFIED	304
#define	HTTP_STATUS_USE_PROXY		305
#define	HTTP_STATUS_BAD_REQUEST		400
#define	HTTP_STATUS_UNAUTHORIZED	401
#define	HTTP_STATUS_FORBIDDEN		403
#define	HTTP_STATUS_NOT_FOUND		404
#define	HTTP_STATUS_METHOD_NOT_ALLOWED	405
#define	HTTP_STATUS_PROXY_AUTH_REQRD	407
#define	HTTP_STATUS_LENGTH_REQUIRED	411
#define	HTTP_STATUS_SERVER_ERROR	500
#define	HTTP_STATUS_NOT_IMPLEMENTED	501
#define	HTTP_STATUS_BAD_GATEWAY		502
#define	HTTP_STATUS_SERVICE_UNAVAILABLE	503
#define	HTTP_STATUS_GATEWAY_TIMEOUT	504
#define	HTTP_STATUS_UNSUPPORTED_VERSION	505

struct http_request {
	int type;
	int method;
	char uri[256];
	char url[256];
	char query[256];
	char host[256];
	int port;
	char useragent[256];
	int vmajor;
	int vminor;
	time_t ifmodsince;
	off_t size;
	time_t msgdate;
	int keepopen;
	char wwwauth[128];
	char authuser[128];
	char authpass[128];
	char cookie[128];
};

struct http_reply {
	int status;
	char statusmsg[128];
	int keepopen;
	int headers;
	char *mtype;
	char realurl[256];
	struct auth *auth;
	int urlaccess;
	off_t size;
	time_t modtime;
	int fd;
	int ofd;
	int pid;
};

/* from httpd.c */

extern FILE *stdlog;
extern FILE *dbglog;

/* from reply.c */

_PROTOTYPE(int sendreply, (struct http_reply *rp, struct http_request *rq));

/* from request.c */

_PROTOTYPE(int getrequest, (struct http_request *rq));

/* from process.c */

_PROTOTYPE(int processrequest, (struct http_request *rq, struct http_reply *rp));

/* from police.c */

_PROTOTYPE(int police, (struct http_request *rq, struct http_reply *rp));

/* from cgiexec.c */

_PROTOTYPE(int cgiexec, (struct http_request *rq, struct http_reply *rp));

/* from proxy.c */

_PROTOTYPE(void proxy, (struct http_request *rq, struct http_reply *rp));
