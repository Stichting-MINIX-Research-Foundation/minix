/* $Id: dllstub.c,v 1.1.1.1 2003-06-04 00:27:45 marka Exp $ */

SOCKET   PASCAL
accept(SOCKET a0, struct sockaddr* a1, int* a2)
{
    static SOCKET   (PASCAL *fp)(SOCKET a0, struct sockaddr* a1, int* a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub accept() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "accept")) == NULL) {
	    FATAL("cannot find entry accept (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   PASCAL
bind(SOCKET a0, const struct sockaddr* a1, int a2)
{
    static int   (PASCAL *fp)(SOCKET a0, const struct sockaddr* a1, int a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub bind() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "bind")) == NULL) {
	    FATAL("cannot find entry bind (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   PASCAL
closesocket(SOCKET a0)
{
    static int   (PASCAL *fp)(SOCKET a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub closesocket() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "closesocket")) == NULL) {
	    FATAL("cannot find entry closesocket (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   PASCAL
connect(SOCKET a0, const struct sockaddr* a1, int a2)
{
    static int   (PASCAL *fp)(SOCKET a0, const struct sockaddr* a1, int a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub connect() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "connect")) == NULL) {
	    FATAL("cannot find entry connect (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   PASCAL
getpeername(SOCKET a0, struct sockaddr* a1, int* a2)
{
    static int   (PASCAL *fp)(SOCKET a0, struct sockaddr* a1, int* a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getpeername() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getpeername")) == NULL) {
	    FATAL("cannot find entry getpeername (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   PASCAL
getsockname(SOCKET a0, struct sockaddr* a1, int* a2)
{
    static int   (PASCAL *fp)(SOCKET a0, struct sockaddr* a1, int* a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getsockname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getsockname")) == NULL) {
	    FATAL("cannot find entry getsockname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   PASCAL
getsockopt(SOCKET a0, int a1, int a2, char* a3, int* a4)
{
    static int   (PASCAL *fp)(SOCKET a0, int a1, int a2, char* a3, int* a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getsockopt() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getsockopt")) == NULL) {
	    FATAL("cannot find entry getsockopt (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

u_long   PASCAL
htonl(u_long a0)
{
    static u_long   (PASCAL *fp)(u_long a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub htonl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "htonl")) == NULL) {
	    FATAL("cannot find entry htonl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

u_short   PASCAL
htons(u_short a0)
{
    static u_short   (PASCAL *fp)(u_short a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub htons() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "htons")) == NULL) {
	    FATAL("cannot find entry htons (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

unsigned long   PASCAL
inet_addr(const char* a0)
{
    static unsigned long   (PASCAL *fp)(const char* a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub inet_addr() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "inet_addr")) == NULL) {
	    FATAL("cannot find entry inet_addr (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

 char * PASCAL
inet_ntoa(struct in_addr a0)
{
    static  char * (PASCAL *fp)(struct in_addr a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub inet_ntoa() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "inet_ntoa")) == NULL) {
	    FATAL("cannot find entry inet_ntoa (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   PASCAL
ioctlsocket(SOCKET a0, long a1, u_long * a2)
{
    static int   (PASCAL *fp)(SOCKET a0, long a1, u_long * a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub ioctlsocket() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "ioctlsocket")) == NULL) {
	    FATAL("cannot find entry ioctlsocket (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   PASCAL
listen(SOCKET a0, int a1)
{
    static int   (PASCAL *fp)(SOCKET a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub listen() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "listen")) == NULL) {
	    FATAL("cannot find entry listen (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

u_long   PASCAL
ntohl(u_long a0)
{
    static u_long   (PASCAL *fp)(u_long a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub ntohl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "ntohl")) == NULL) {
	    FATAL("cannot find entry ntohl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

u_short   PASCAL
ntohs(u_short a0)
{
    static u_short   (PASCAL *fp)(u_short a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub ntohs() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "ntohs")) == NULL) {
	    FATAL("cannot find entry ntohs (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   PASCAL
recv(SOCKET a0, char* a1, int a2, int a3)
{
    static int   (PASCAL *fp)(SOCKET a0, char* a1, int a2, int a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub recv() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "recv")) == NULL) {
	    FATAL("cannot find entry recv (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int   PASCAL
recvfrom(SOCKET a0, char* a1, int a2, int a3, struct sockaddr* a4, int* a5)
{
    static int   (PASCAL *fp)(SOCKET a0, char* a1, int a2, int a3, struct sockaddr* a4, int* a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub recvfrom() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "recvfrom")) == NULL) {
	    FATAL("cannot find entry recvfrom (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

int   PASCAL
select(int a0, fd_set* a1, fd_set* a2, fd_set* a3, const struct timeval* a4)
{
    static int   (PASCAL *fp)(int a0, fd_set* a1, fd_set* a2, fd_set* a3, const struct timeval* a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub select() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "select")) == NULL) {
	    FATAL("cannot find entry select (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   PASCAL
send(SOCKET a0, const char* a1, int a2, int a3)
{
    static int   (PASCAL *fp)(SOCKET a0, const char* a1, int a2, int a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub send() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "send")) == NULL) {
	    FATAL("cannot find entry send (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int   PASCAL
sendto(SOCKET a0, const char* a1, int a2, int a3, const struct sockaddr* a4, int a5)
{
    static int   (PASCAL *fp)(SOCKET a0, const char* a1, int a2, int a3, const struct sockaddr* a4, int a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub sendto() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "sendto")) == NULL) {
	    FATAL("cannot find entry sendto (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

int   PASCAL
setsockopt(SOCKET a0, int a1, int a2, const char* a3, int a4)
{
    static int   (PASCAL *fp)(SOCKET a0, int a1, int a2, const char* a3, int a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub setsockopt() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "setsockopt")) == NULL) {
	    FATAL("cannot find entry setsockopt (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   PASCAL
shutdown(SOCKET a0, int a1)
{
    static int   (PASCAL *fp)(SOCKET a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub shutdown() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "shutdown")) == NULL) {
	    FATAL("cannot find entry shutdown (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

SOCKET   PASCAL
socket(int a0, int a1, int a2)
{
    static SOCKET   (PASCAL *fp)(int a0, int a1, int a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub socket() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "socket")) == NULL) {
	    FATAL("cannot find entry socket (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int  PASCAL
MigrateWinsockConfiguration(int a0, int a1, int a2)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub MigrateWinsockConfiguration() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "MigrateWinsockConfiguration")) == NULL) {
	    FATAL("cannot find entry MigrateWinsockConfiguration (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

 struct hostent * PASCAL
_org_gethostbyaddr(const char* a0, int a1, int a2)
{
    static  struct hostent * (PASCAL *fp)(const char* a0, int a1, int a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_gethostbyaddr() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "gethostbyaddr")) == NULL) {
	    FATAL("cannot find entry gethostbyaddr (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

 struct hostent * PASCAL
_org_gethostbyname(const char* a0)
{
    static  struct hostent * (PASCAL *fp)(const char* a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_gethostbyname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "gethostbyname")) == NULL) {
	    FATAL("cannot find entry gethostbyname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

 struct protoent * PASCAL
getprotobyname(const char* a0)
{
    static  struct protoent * (PASCAL *fp)(const char* a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getprotobyname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getprotobyname")) == NULL) {
	    FATAL("cannot find entry getprotobyname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

 struct protoent * PASCAL
getprotobynumber(int a0)
{
    static  struct protoent * (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getprotobynumber() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getprotobynumber")) == NULL) {
	    FATAL("cannot find entry getprotobynumber (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

 struct servent * PASCAL
getservbyname(const char* a0, const char* a1)
{
    static  struct servent * (PASCAL *fp)(const char* a0, const char* a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getservbyname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getservbyname")) == NULL) {
	    FATAL("cannot find entry getservbyname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

 struct servent * PASCAL
getservbyport(int a0, const char* a1)
{
    static  struct servent * (PASCAL *fp)(int a0, const char* a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getservbyport() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getservbyport")) == NULL) {
	    FATAL("cannot find entry getservbyport (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   PASCAL
_org_gethostname(char* a0, int a1)
{
    static int   (PASCAL *fp)(char* a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_gethostname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "gethostname")) == NULL) {
	    FATAL("cannot find entry gethostname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   PASCAL
WSAAsyncSelect(SOCKET a0, HWND a1, u_int a2, long a3)
{
    static int   (PASCAL *fp)(SOCKET a0, HWND a1, u_int a2, long a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAsyncSelect() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncSelect")) == NULL) {
	    FATAL("cannot find entry WSAAsyncSelect (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

HANDLE   PASCAL
_org_WSAAsyncGetHostByAddr(HWND a0, u_int a1, const char* a2, int a3, int a4, char* a5, int a6)
{
    static HANDLE   (PASCAL *fp)(HWND a0, u_int a1, const char* a2, int a3, int a4, char* a5, int a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSAAsyncGetHostByAddr() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncGetHostByAddr")) == NULL) {
	    FATAL("cannot find entry WSAAsyncGetHostByAddr (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

HANDLE   PASCAL
_org_WSAAsyncGetHostByName(HWND a0, u_int a1, const char* a2, char* a3, int a4)
{
    static HANDLE   (PASCAL *fp)(HWND a0, u_int a1, const char* a2, char* a3, int a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSAAsyncGetHostByName() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncGetHostByName")) == NULL) {
	    FATAL("cannot find entry WSAAsyncGetHostByName (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

HANDLE   PASCAL
WSAAsyncGetProtoByNumber(HWND a0, u_int a1, int a2, char* a3, int a4)
{
    static HANDLE   (PASCAL *fp)(HWND a0, u_int a1, int a2, char* a3, int a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAsyncGetProtoByNumber() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncGetProtoByNumber")) == NULL) {
	    FATAL("cannot find entry WSAAsyncGetProtoByNumber (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

HANDLE   PASCAL
WSAAsyncGetProtoByName(HWND a0, u_int a1, const char* a2, char* a3, int a4)
{
    static HANDLE   (PASCAL *fp)(HWND a0, u_int a1, const char* a2, char* a3, int a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAsyncGetProtoByName() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncGetProtoByName")) == NULL) {
	    FATAL("cannot find entry WSAAsyncGetProtoByName (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

HANDLE   PASCAL
WSAAsyncGetServByPort(HWND a0, u_int a1, int a2, const char* a3, char* a4, int a5)
{
    static HANDLE   (PASCAL *fp)(HWND a0, u_int a1, int a2, const char* a3, char* a4, int a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAsyncGetServByPort() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncGetServByPort")) == NULL) {
	    FATAL("cannot find entry WSAAsyncGetServByPort (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

HANDLE   PASCAL
WSAAsyncGetServByName(HWND a0, u_int a1, const char* a2, const char* a3, char* a4, int a5)
{
    static HANDLE   (PASCAL *fp)(HWND a0, u_int a1, const char* a2, const char* a3, char* a4, int a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAsyncGetServByName() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAsyncGetServByName")) == NULL) {
	    FATAL("cannot find entry WSAAsyncGetServByName (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

int   PASCAL
WSACancelAsyncRequest(HANDLE a0)
{
    static int   (PASCAL *fp)(HANDLE a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSACancelAsyncRequest() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSACancelAsyncRequest")) == NULL) {
	    FATAL("cannot find entry WSACancelAsyncRequest (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

FARPROC   PASCAL
WSASetBlockingHook(FARPROC a0)
{
    static FARPROC   (PASCAL *fp)(FARPROC a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASetBlockingHook() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASetBlockingHook")) == NULL) {
	    FATAL("cannot find entry WSASetBlockingHook (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   PASCAL
WSAUnhookBlockingHook(void)
{
    static int   (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAUnhookBlockingHook() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAUnhookBlockingHook")) == NULL) {
	    FATAL("cannot find entry WSAUnhookBlockingHook (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

int   PASCAL
WSAGetLastError(void)
{
    static int   (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetLastError() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetLastError")) == NULL) {
	    FATAL("cannot find entry WSAGetLastError (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

void   PASCAL
WSASetLastError(int a0)
{
    static void   (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASetLastError() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASetLastError")) == NULL) {
	    FATAL("cannot find entry WSASetLastError (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
     (*fp)(a0);
}

int   PASCAL
WSACancelBlockingCall(void)
{
    static int   (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSACancelBlockingCall() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSACancelBlockingCall")) == NULL) {
	    FATAL("cannot find entry WSACancelBlockingCall (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

BOOL   PASCAL
WSAIsBlocking(void)
{
    static BOOL   (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAIsBlocking() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAIsBlocking")) == NULL) {
	    FATAL("cannot find entry WSAIsBlocking (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

int   PASCAL
WSAStartup(WORD a0, LPWSADATA a1)
{
    static int   (PASCAL *fp)(WORD a0, LPWSADATA a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAStartup() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAStartup")) == NULL) {
	    FATAL("cannot find entry WSAStartup (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   PASCAL
WSACleanup(void)
{
    static int   (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSACleanup() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSACleanup")) == NULL) {
	    FATAL("cannot find entry WSACleanup (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

int   PASCAL
__WSAFDIsSet(SOCKET a0, fd_set* a1)
{
    static int   (PASCAL *fp)(SOCKET a0, fd_set* a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub __WSAFDIsSet() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "__WSAFDIsSet")) == NULL) {
	    FATAL("cannot find entry __WSAFDIsSet (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int  PASCAL
WEP(void)
{
    static int  (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WEP() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WEP")) == NULL) {
	    FATAL("cannot find entry WEP (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

int  PASCAL
WSApSetPostRoutine(int a0)
{
    static int  (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSApSetPostRoutine() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSApSetPostRoutine")) == NULL) {
	    FATAL("cannot find entry WSApSetPostRoutine (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int  PASCAL
WsControl(int a0, int a1, int a2, int a3, int a4, int a5)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2, int a3, int a4, int a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WsControl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WsControl")) == NULL) {
	    FATAL("cannot find entry WsControl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

int  PASCAL
closesockinfo(int a0)
{
    static int  (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub closesockinfo() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "closesockinfo")) == NULL) {
	    FATAL("cannot find entry closesockinfo (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int  PASCAL
Arecv(int a0, int a1, int a2, int a3)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2, int a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub Arecv() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "Arecv")) == NULL) {
	    FATAL("cannot find entry Arecv (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int  PASCAL
Asend(int a0, int a1, int a2, int a3)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2, int a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub Asend() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "Asend")) == NULL) {
	    FATAL("cannot find entry Asend (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int  PASCAL
WSHEnumProtocols(void)
{
    static int  (PASCAL *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSHEnumProtocols() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSHEnumProtocols")) == NULL) {
	    FATAL("cannot find entry WSHEnumProtocols (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

int  PASCAL
inet_network(int a0)
{
    static int  (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub inet_network() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "inet_network")) == NULL) {
	    FATAL("cannot find entry inet_network (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int  PASCAL
getnetbyname(int a0)
{
    static int  (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub getnetbyname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getnetbyname")) == NULL) {
	    FATAL("cannot find entry getnetbyname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int  PASCAL
rcmd(int a0, int a1, int a2, int a3, int a4, int a5)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2, int a3, int a4, int a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub rcmd() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "rcmd")) == NULL) {
	    FATAL("cannot find entry rcmd (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

int  PASCAL
rexec(int a0, int a1, int a2, int a3, int a4, int a5)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2, int a3, int a4, int a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub rexec() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "rexec")) == NULL) {
	    FATAL("cannot find entry rexec (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

int  PASCAL
rresvport(int a0)
{
    static int  (PASCAL *fp)(int a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub rresvport() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "rresvport")) == NULL) {
	    FATAL("cannot find entry rresvport (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int  PASCAL
sethostname(int a0, int a1)
{
    static int  (PASCAL *fp)(int a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub sethostname() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "sethostname")) == NULL) {
	    FATAL("cannot find entry sethostname (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int  PASCAL
dn_expand(int a0, int a1, int a2, int a3, int a4)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2, int a3, int a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub dn_expand() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "dn_expand")) == NULL) {
	    FATAL("cannot find entry dn_expand (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   PASCAL
WSARecvEx(SOCKET a0, char* a1, int a2, int* a3)
{
    static int   (PASCAL *fp)(SOCKET a0, char* a1, int a2, int* a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSARecvEx() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSARecvEx")) == NULL) {
	    FATAL("cannot find entry WSARecvEx (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int  PASCAL
s_perror(int a0, int a1)
{
    static int  (PASCAL *fp)(int a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub s_perror() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "s_perror")) == NULL) {
	    FATAL("cannot find entry s_perror (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

INT   PASCAL
GetAddressByNameA(DWORD a0, LPGUID a1, LPSTR a2, LPINT a3, DWORD a4, LPSERVICE_ASYNC_INFO a5, LPVOID a6, LPDWORD a7, LPSTR a8, LPDWORD a9)
{
    static INT   (PASCAL *fp)(DWORD a0, LPGUID a1, LPSTR a2, LPINT a3, DWORD a4, LPSERVICE_ASYNC_INFO a5, LPVOID a6, LPDWORD a7, LPSTR a8, LPDWORD a9);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetAddressByNameA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetAddressByNameA")) == NULL) {
	    FATAL("cannot find entry GetAddressByNameA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

INT   PASCAL
GetAddressByNameW(DWORD a0, LPGUID a1, LPWSTR a2, LPINT a3, DWORD a4, LPSERVICE_ASYNC_INFO a5, LPVOID a6, LPDWORD a7, LPWSTR a8, LPDWORD a9)
{
    static INT   (PASCAL *fp)(DWORD a0, LPGUID a1, LPWSTR a2, LPINT a3, DWORD a4, LPSERVICE_ASYNC_INFO a5, LPVOID a6, LPDWORD a7, LPWSTR a8, LPDWORD a9);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetAddressByNameW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetAddressByNameW")) == NULL) {
	    FATAL("cannot find entry GetAddressByNameW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}

INT   PASCAL
EnumProtocolsA(LPINT a0, LPVOID a1, LPDWORD a2)
{
    static INT   (PASCAL *fp)(LPINT a0, LPVOID a1, LPDWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub EnumProtocolsA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "EnumProtocolsA")) == NULL) {
	    FATAL("cannot find entry EnumProtocolsA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   PASCAL
EnumProtocolsW(LPINT a0, LPVOID a1, LPDWORD a2)
{
    static INT   (PASCAL *fp)(LPINT a0, LPVOID a1, LPDWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub EnumProtocolsW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "EnumProtocolsW")) == NULL) {
	    FATAL("cannot find entry EnumProtocolsW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   PASCAL
GetTypeByNameA(LPSTR a0, LPGUID a1)
{
    static INT   (PASCAL *fp)(LPSTR a0, LPGUID a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetTypeByNameA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetTypeByNameA")) == NULL) {
	    FATAL("cannot find entry GetTypeByNameA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

INT   PASCAL
GetTypeByNameW(LPWSTR a0, LPGUID a1)
{
    static INT   (PASCAL *fp)(LPWSTR a0, LPGUID a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetTypeByNameW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetTypeByNameW")) == NULL) {
	    FATAL("cannot find entry GetTypeByNameW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

INT   PASCAL
GetNameByTypeA(LPGUID a0, LPSTR a1, DWORD a2)
{
    static INT   (PASCAL *fp)(LPGUID a0, LPSTR a1, DWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetNameByTypeA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetNameByTypeA")) == NULL) {
	    FATAL("cannot find entry GetNameByTypeA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   PASCAL
GetNameByTypeW(LPGUID a0, LPWSTR a1, DWORD a2)
{
    static INT   (PASCAL *fp)(LPGUID a0, LPWSTR a1, DWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetNameByTypeW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetNameByTypeW")) == NULL) {
	    FATAL("cannot find entry GetNameByTypeW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   PASCAL
SetServiceA(DWORD a0, DWORD a1, DWORD a2, LPSERVICE_INFOA a3, LPSERVICE_ASYNC_INFO a4, LPDWORD a5)
{
    static INT   (PASCAL *fp)(DWORD a0, DWORD a1, DWORD a2, LPSERVICE_INFOA a3, LPSERVICE_ASYNC_INFO a4, LPDWORD a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub SetServiceA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "SetServiceA")) == NULL) {
	    FATAL("cannot find entry SetServiceA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

INT   PASCAL
SetServiceW(DWORD a0, DWORD a1, DWORD a2, LPSERVICE_INFOW a3, LPSERVICE_ASYNC_INFO a4, LPDWORD a5)
{
    static INT   (PASCAL *fp)(DWORD a0, DWORD a1, DWORD a2, LPSERVICE_INFOW a3, LPSERVICE_ASYNC_INFO a4, LPDWORD a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub SetServiceW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "SetServiceW")) == NULL) {
	    FATAL("cannot find entry SetServiceW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

INT   PASCAL
GetServiceA(DWORD a0, LPGUID a1, LPSTR a2, DWORD a3, LPVOID a4, LPDWORD a5, LPSERVICE_ASYNC_INFO a6)
{
    static INT   (PASCAL *fp)(DWORD a0, LPGUID a1, LPSTR a2, DWORD a3, LPVOID a4, LPDWORD a5, LPSERVICE_ASYNC_INFO a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetServiceA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetServiceA")) == NULL) {
	    FATAL("cannot find entry GetServiceA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

INT   PASCAL
GetServiceW(DWORD a0, LPGUID a1, LPWSTR a2, DWORD a3, LPVOID a4, LPDWORD a5, LPSERVICE_ASYNC_INFO a6)
{
    static INT   (PASCAL *fp)(DWORD a0, LPGUID a1, LPWSTR a2, DWORD a3, LPVOID a4, LPDWORD a5, LPSERVICE_ASYNC_INFO a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetServiceW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetServiceW")) == NULL) {
	    FATAL("cannot find entry GetServiceW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

int  PASCAL
NPLoadNameSpaces(int a0, int a1, int a2)
{
    static int  (PASCAL *fp)(int a0, int a1, int a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub NPLoadNameSpaces() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "NPLoadNameSpaces")) == NULL) {
	    FATAL("cannot find entry NPLoadNameSpaces (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int  PASCAL
NSPStartup(int a0, int a1)
{
    static int  (PASCAL *fp)(int a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub NSPStartup() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "NSPStartup")) == NULL) {
	    FATAL("cannot find entry NSPStartup (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

BOOL   PASCAL
TransmitFile(SOCKET a0, HANDLE a1, DWORD a2, DWORD a3, LPOVERLAPPED a4, LPTRANSMIT_FILE_BUFFERS a5, DWORD a6)
{
    static BOOL   (PASCAL *fp)(SOCKET a0, HANDLE a1, DWORD a2, DWORD a3, LPOVERLAPPED a4, LPTRANSMIT_FILE_BUFFERS a5, DWORD a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub TransmitFile() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "TransmitFile")) == NULL) {
	    FATAL("cannot find entry TransmitFile (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

BOOL   PASCAL
AcceptEx(SOCKET a0, SOCKET a1, PVOID a2, DWORD a3, DWORD a4, DWORD a5, LPDWORD a6, LPOVERLAPPED a7)
{
    static BOOL   (PASCAL *fp)(SOCKET a0, SOCKET a1, PVOID a2, DWORD a3, DWORD a4, DWORD a5, LPDWORD a6, LPOVERLAPPED a7);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub AcceptEx() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "AcceptEx")) == NULL) {
	    FATAL("cannot find entry AcceptEx (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7);
}

VOID   PASCAL
GetAcceptExSockaddrs(PVOID a0, DWORD a1, DWORD a2, DWORD a3, struct sockaddr** a4, LPINT a5, struct sockaddr** a6, LPINT a7)
{
    static VOID   (PASCAL *fp)(PVOID a0, DWORD a1, DWORD a2, DWORD a3, struct sockaddr** a4, LPINT a5, struct sockaddr** a6, LPINT a7);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub GetAcceptExSockaddrs() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "GetAcceptExSockaddrs")) == NULL) {
	    FATAL("cannot find entry GetAcceptExSockaddrs (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
     (*fp)(a0, a1, a2, a3, a4, a5, a6, a7);
}

