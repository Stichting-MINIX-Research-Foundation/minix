/* $NetBSD: trace_mr.h,v 1.1 2007/05/27 16:27:57 tls Exp $ */

ssize_t trace_mr_send (int, const void *, size_t, int);
ssize_t trace_mr_read_playback (struct sockaddr_in *, void *, size_t);
void trace_mr_read_record (struct sockaddr_in *, void *, ssize_t);
ssize_t trace_mr_recvfrom (int s, void *, size_t, int,
			   struct sockaddr *, SOCKLEN_T *);
ssize_t trace_mr_read (int, void *, size_t);
int trace_mr_connect (int s, struct sockaddr *, SOCKLEN_T);
int trace_mr_socket (int, int, int);
int trace_mr_bind (int, struct sockaddr *, SOCKLEN_T);
int trace_mr_close (int);
time_t trace_mr_time (time_t *);
int trace_mr_select (int, fd_set *, fd_set *, fd_set *, struct timeval *);
unsigned int trace_mr_res_randomid (unsigned int);
