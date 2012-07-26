/* $Id: dllstub.c,v 1.1.1.1 2003-06-04 00:27:51 marka Exp $ */

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

WSAEVENT   WSAAPI
WPUCompleteOverlappedRequest(SOCKET a0, LPWSAOVERLAPPED a1, DWORD a2, DWORD a3, LPINT a4)
{
    static WSAEVENT   (WSAAPI *fp)(SOCKET a0, LPWSAOVERLAPPED a1, DWORD a2, DWORD a3, LPINT a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WPUCompleteOverlappedRequest() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WPUCompleteOverlappedRequest")) == NULL) {
	    FATAL("cannot find entry WPUCompleteOverlappedRequest (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

SOCKET   WINAPI
WSAAccept(SOCKET a0, struct sockaddr * a1, LPINT a2, LPCONDITIONPROC a3, DWORD a4)
{
    static SOCKET   (WINAPI *fp)(SOCKET a0, struct sockaddr * a1, LPINT a2, LPCONDITIONPROC a3, DWORD a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAccept() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAccept")) == NULL) {
	    FATAL("cannot find entry WSAAccept (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

INT   WINAPI
WSAAddressToStringA(LPSOCKADDR a0, DWORD a1, LPWSAPROTOCOL_INFOA a2, LPSTR a3, LPDWORD a4)
{
    static INT   (WINAPI *fp)(LPSOCKADDR a0, DWORD a1, LPWSAPROTOCOL_INFOA a2, LPSTR a3, LPDWORD a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAddressToStringA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAddressToStringA")) == NULL) {
	    FATAL("cannot find entry WSAAddressToStringA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

INT   WINAPI
WSAAddressToStringW(LPSOCKADDR a0, DWORD a1, LPWSAPROTOCOL_INFOW a2, LPWSTR a3, LPDWORD a4)
{
    static INT   (WINAPI *fp)(LPSOCKADDR a0, DWORD a1, LPWSAPROTOCOL_INFOW a2, LPWSTR a3, LPDWORD a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAAddressToStringW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAAddressToStringW")) == NULL) {
	    FATAL("cannot find entry WSAAddressToStringW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

BOOL   WINAPI
WSACloseEvent(WSAEVENT a0)
{
    static BOOL   (WINAPI *fp)(WSAEVENT a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSACloseEvent() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSACloseEvent")) == NULL) {
	    FATAL("cannot find entry WSACloseEvent (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   WINAPI
WSAConnect(SOCKET a0, const struct sockaddr * a1, int a2, LPWSABUF a3, LPWSABUF a4, LPQOS a5, LPQOS a6)
{
    static int   (WINAPI *fp)(SOCKET a0, const struct sockaddr * a1, int a2, LPWSABUF a3, LPWSABUF a4, LPQOS a5, LPQOS a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAConnect() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAConnect")) == NULL) {
	    FATAL("cannot find entry WSAConnect (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

WSAEVENT   WINAPI
WSACreateEvent(void)
{
    static WSAEVENT   (WINAPI *fp)(void);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSACreateEvent() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSACreateEvent")) == NULL) {
	    FATAL("cannot find entry WSACreateEvent (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)();
}

int   WINAPI
WSADuplicateSocketA(SOCKET a0, DWORD a1, LPWSAPROTOCOL_INFOA a2)
{
    static int   (WINAPI *fp)(SOCKET a0, DWORD a1, LPWSAPROTOCOL_INFOA a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSADuplicateSocketA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSADuplicateSocketA")) == NULL) {
	    FATAL("cannot find entry WSADuplicateSocketA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSADuplicateSocketW(SOCKET a0, DWORD a1, LPWSAPROTOCOL_INFOW a2)
{
    static int   (WINAPI *fp)(SOCKET a0, DWORD a1, LPWSAPROTOCOL_INFOW a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSADuplicateSocketW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSADuplicateSocketW")) == NULL) {
	    FATAL("cannot find entry WSADuplicateSocketW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
WSAEnumNameSpaceProvidersA(LPDWORD a0, LPWSANAMESPACE_INFOA a1)
{
    static INT   (WINAPI *fp)(LPDWORD a0, LPWSANAMESPACE_INFOA a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAEnumNameSpaceProvidersA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAEnumNameSpaceProvidersA")) == NULL) {
	    FATAL("cannot find entry WSAEnumNameSpaceProvidersA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

INT   WINAPI
WSAEnumNameSpaceProvidersW(LPDWORD a0, LPWSANAMESPACE_INFOW a1)
{
    static INT   (WINAPI *fp)(LPDWORD a0, LPWSANAMESPACE_INFOW a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAEnumNameSpaceProvidersW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAEnumNameSpaceProvidersW")) == NULL) {
	    FATAL("cannot find entry WSAEnumNameSpaceProvidersW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   WINAPI
WSAEnumNetworkEvents(SOCKET a0, WSAEVENT a1, LPWSANETWORKEVENTS a2)
{
    static int   (WINAPI *fp)(SOCKET a0, WSAEVENT a1, LPWSANETWORKEVENTS a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAEnumNetworkEvents() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAEnumNetworkEvents")) == NULL) {
	    FATAL("cannot find entry WSAEnumNetworkEvents (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSAEnumProtocolsA(LPINT a0, LPWSAPROTOCOL_INFOA a1, LPDWORD a2)
{
    static int   (WINAPI *fp)(LPINT a0, LPWSAPROTOCOL_INFOA a1, LPDWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAEnumProtocolsA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAEnumProtocolsA")) == NULL) {
	    FATAL("cannot find entry WSAEnumProtocolsA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSAEnumProtocolsW(LPINT a0, LPWSAPROTOCOL_INFOW a1, LPDWORD a2)
{
    static int   (WINAPI *fp)(LPINT a0, LPWSAPROTOCOL_INFOW a1, LPDWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAEnumProtocolsW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAEnumProtocolsW")) == NULL) {
	    FATAL("cannot find entry WSAEnumProtocolsW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSAEventSelect(SOCKET a0, WSAEVENT a1, long a2)
{
    static int   (WINAPI *fp)(SOCKET a0, WSAEVENT a1, long a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAEventSelect() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAEventSelect")) == NULL) {
	    FATAL("cannot find entry WSAEventSelect (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

BOOL   WINAPI
WSAGetOverlappedResult(SOCKET a0, LPWSAOVERLAPPED a1, LPDWORD a2, BOOL a3, LPDWORD a4)
{
    static BOOL   (WINAPI *fp)(SOCKET a0, LPWSAOVERLAPPED a1, LPDWORD a2, BOOL a3, LPDWORD a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetOverlappedResult() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetOverlappedResult")) == NULL) {
	    FATAL("cannot find entry WSAGetOverlappedResult (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

BOOL   WINAPI
WSAGetQOSByName(SOCKET a0, LPWSABUF a1, LPQOS a2)
{
    static BOOL   (WINAPI *fp)(SOCKET a0, LPWSABUF a1, LPQOS a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetQOSByName() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetQOSByName")) == NULL) {
	    FATAL("cannot find entry WSAGetQOSByName (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
WSAGetServiceClassInfoA(LPGUID a0, LPGUID a1, LPDWORD a2, LPWSASERVICECLASSINFOA a3)
{
    static INT   (WINAPI *fp)(LPGUID a0, LPGUID a1, LPDWORD a2, LPWSASERVICECLASSINFOA a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetServiceClassInfoA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetServiceClassInfoA")) == NULL) {
	    FATAL("cannot find entry WSAGetServiceClassInfoA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

INT   WINAPI
WSAGetServiceClassInfoW(LPGUID a0, LPGUID a1, LPDWORD a2, LPWSASERVICECLASSINFOW a3)
{
    static INT   (WINAPI *fp)(LPGUID a0, LPGUID a1, LPDWORD a2, LPWSASERVICECLASSINFOW a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetServiceClassInfoW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetServiceClassInfoW")) == NULL) {
	    FATAL("cannot find entry WSAGetServiceClassInfoW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

INT   WINAPI
WSAGetServiceClassNameByClassIdA(LPGUID a0, LPSTR a1, LPDWORD a2)
{
    static INT   (WINAPI *fp)(LPGUID a0, LPSTR a1, LPDWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetServiceClassNameByClassIdA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetServiceClassNameByClassIdA")) == NULL) {
	    FATAL("cannot find entry WSAGetServiceClassNameByClassIdA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
WSAGetServiceClassNameByClassIdW(LPGUID a0, LPWSTR a1, LPDWORD a2)
{
    static INT   (WINAPI *fp)(LPGUID a0, LPWSTR a1, LPDWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAGetServiceClassNameByClassIdW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAGetServiceClassNameByClassIdW")) == NULL) {
	    FATAL("cannot find entry WSAGetServiceClassNameByClassIdW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSAHtonl(SOCKET a0, unsigned long a1, unsigned long * a2)
{
    static int   (WINAPI *fp)(SOCKET a0, unsigned long a1, unsigned long * a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAHtonl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAHtonl")) == NULL) {
	    FATAL("cannot find entry WSAHtonl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSAHtons(SOCKET a0, unsigned short a1, unsigned short * a2)
{
    static int   (WINAPI *fp)(SOCKET a0, unsigned short a1, unsigned short * a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAHtons() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAHtons")) == NULL) {
	    FATAL("cannot find entry WSAHtons (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
WSAInstallServiceClassA(LPWSASERVICECLASSINFOA a0)
{
    static INT   (WINAPI *fp)(LPWSASERVICECLASSINFOA a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAInstallServiceClassA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAInstallServiceClassA")) == NULL) {
	    FATAL("cannot find entry WSAInstallServiceClassA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

INT   WINAPI
WSAInstallServiceClassW(LPWSASERVICECLASSINFOW a0)
{
    static INT   (WINAPI *fp)(LPWSASERVICECLASSINFOW a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAInstallServiceClassW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAInstallServiceClassW")) == NULL) {
	    FATAL("cannot find entry WSAInstallServiceClassW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   WINAPI
WSAIoctl(SOCKET a0, DWORD a1, LPVOID a2, DWORD a3, LPVOID a4, DWORD a5, LPDWORD a6, LPWSAOVERLAPPED a7, LPWSAOVERLAPPED_COMPLETION_ROUTINE a8)
{
    static int   (WINAPI *fp)(SOCKET a0, DWORD a1, LPVOID a2, DWORD a3, LPVOID a4, DWORD a5, LPDWORD a6, LPWSAOVERLAPPED a7, LPWSAOVERLAPPED_COMPLETION_ROUTINE a8);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAIoctl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAIoctl")) == NULL) {
	    FATAL("cannot find entry WSAIoctl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
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

SOCKET   WINAPI
WSAJoinLeaf(SOCKET a0, const struct sockaddr * a1, int a2, LPWSABUF a3, LPWSABUF a4, LPQOS a5, LPQOS a6, DWORD a7)
{
    static SOCKET   (WINAPI *fp)(SOCKET a0, const struct sockaddr * a1, int a2, LPWSABUF a3, LPWSABUF a4, LPQOS a5, LPQOS a6, DWORD a7);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAJoinLeaf() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAJoinLeaf")) == NULL) {
	    FATAL("cannot find entry WSAJoinLeaf (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7);
}

INT   WINAPI
_org_WSALookupServiceBeginA(LPWSAQUERYSETA a0, DWORD a1, LPHANDLE a2)
{
    static INT   (WINAPI *fp)(LPWSAQUERYSETA a0, DWORD a1, LPHANDLE a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSALookupServiceBeginA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSALookupServiceBeginA")) == NULL) {
	    FATAL("cannot find entry WSALookupServiceBeginA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
_org_WSALookupServiceBeginW(LPWSAQUERYSETW a0, DWORD a1, LPHANDLE a2)
{
    static INT   (WINAPI *fp)(LPWSAQUERYSETW a0, DWORD a1, LPHANDLE a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSALookupServiceBeginW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSALookupServiceBeginW")) == NULL) {
	    FATAL("cannot find entry WSALookupServiceBeginW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
_org_WSALookupServiceEnd(HANDLE a0)
{
    static INT   (WINAPI *fp)(HANDLE a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSALookupServiceEnd() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSALookupServiceEnd")) == NULL) {
	    FATAL("cannot find entry WSALookupServiceEnd (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

INT   WINAPI
_org_WSALookupServiceNextA(HANDLE a0, DWORD a1, LPDWORD a2, LPWSAQUERYSETA a3)
{
    static INT   (WINAPI *fp)(HANDLE a0, DWORD a1, LPDWORD a2, LPWSAQUERYSETA a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSALookupServiceNextA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSALookupServiceNextA")) == NULL) {
	    FATAL("cannot find entry WSALookupServiceNextA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

INT   WINAPI
_org_WSALookupServiceNextW(HANDLE a0, DWORD a1, LPDWORD a2, LPWSAQUERYSETW a3)
{
    static INT   (WINAPI *fp)(HANDLE a0, DWORD a1, LPDWORD a2, LPWSAQUERYSETW a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_WSALookupServiceNextW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSALookupServiceNextW")) == NULL) {
	    FATAL("cannot find entry WSALookupServiceNextW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int   WINAPI
WSANtohl(SOCKET a0, unsigned long a1, unsigned long * a2)
{
    static int   (WINAPI *fp)(SOCKET a0, unsigned long a1, unsigned long * a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSANtohl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSANtohl")) == NULL) {
	    FATAL("cannot find entry WSANtohl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSANtohs(SOCKET a0, unsigned short a1, unsigned short * a2)
{
    static int   (WINAPI *fp)(SOCKET a0, unsigned short a1, unsigned short * a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSANtohs() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSANtohs")) == NULL) {
	    FATAL("cannot find entry WSANtohs (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WSAAPI
WSAProviderConfigChange(LPHANDLE a0, LPWSAOVERLAPPED a1, LPWSAOVERLAPPED_COMPLETION_ROUTINE a2)
{
    static int   (WSAAPI *fp)(LPHANDLE a0, LPWSAOVERLAPPED a1, LPWSAOVERLAPPED_COMPLETION_ROUTINE a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAProviderConfigChange() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAProviderConfigChange")) == NULL) {
	    FATAL("cannot find entry WSAProviderConfigChange (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

int   WINAPI
WSARecv(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, LPDWORD a4, LPWSAOVERLAPPED a5, LPWSAOVERLAPPED_COMPLETION_ROUTINE a6)
{
    static int   (WINAPI *fp)(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, LPDWORD a4, LPWSAOVERLAPPED a5, LPWSAOVERLAPPED_COMPLETION_ROUTINE a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSARecv() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSARecv")) == NULL) {
	    FATAL("cannot find entry WSARecv (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

int   WINAPI
WSARecvDisconnect(SOCKET a0, LPWSABUF a1)
{
    static int   (WINAPI *fp)(SOCKET a0, LPWSABUF a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSARecvDisconnect() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSARecvDisconnect")) == NULL) {
	    FATAL("cannot find entry WSARecvDisconnect (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   WINAPI
WSARecvFrom(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, LPDWORD a4, struct sockaddr * a5, LPINT a6, LPWSAOVERLAPPED a7, LPWSAOVERLAPPED_COMPLETION_ROUTINE a8)
{
    static int   (WINAPI *fp)(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, LPDWORD a4, struct sockaddr * a5, LPINT a6, LPWSAOVERLAPPED a7, LPWSAOVERLAPPED_COMPLETION_ROUTINE a8);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSARecvFrom() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSARecvFrom")) == NULL) {
	    FATAL("cannot find entry WSARecvFrom (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

INT   WINAPI
WSARemoveServiceClass(LPGUID a0)
{
    static INT   (WINAPI *fp)(LPGUID a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSARemoveServiceClass() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSARemoveServiceClass")) == NULL) {
	    FATAL("cannot find entry WSARemoveServiceClass (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

BOOL   WINAPI
WSAResetEvent(WSAEVENT a0)
{
    static BOOL   (WINAPI *fp)(WSAEVENT a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAResetEvent() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAResetEvent")) == NULL) {
	    FATAL("cannot find entry WSAResetEvent (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

int   WINAPI
WSASend(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, DWORD a4, LPWSAOVERLAPPED a5, LPWSAOVERLAPPED_COMPLETION_ROUTINE a6)
{
    static int   (WINAPI *fp)(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, DWORD a4, LPWSAOVERLAPPED a5, LPWSAOVERLAPPED_COMPLETION_ROUTINE a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASend() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASend")) == NULL) {
	    FATAL("cannot find entry WSASend (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

int   WINAPI
WSASendDisconnect(SOCKET a0, LPWSABUF a1)
{
    static int   (WINAPI *fp)(SOCKET a0, LPWSABUF a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASendDisconnect() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASendDisconnect")) == NULL) {
	    FATAL("cannot find entry WSASendDisconnect (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   WINAPI
WSASendTo(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, DWORD a4, const struct sockaddr * a5, int a6, LPWSAOVERLAPPED a7, LPWSAOVERLAPPED_COMPLETION_ROUTINE a8)
{
    static int   (WINAPI *fp)(SOCKET a0, LPWSABUF a1, DWORD a2, LPDWORD a3, DWORD a4, const struct sockaddr * a5, int a6, LPWSAOVERLAPPED a7, LPWSAOVERLAPPED_COMPLETION_ROUTINE a8);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASendTo() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASendTo")) == NULL) {
	    FATAL("cannot find entry WSASendTo (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7, a8);
}

BOOL   WINAPI
WSASetEvent(WSAEVENT a0)
{
    static BOOL   (WINAPI *fp)(WSAEVENT a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASetEvent() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASetEvent")) == NULL) {
	    FATAL("cannot find entry WSASetEvent (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
}

INT   WSAAPI
WSASetServiceA(LPWSAQUERYSETA a0, WSAESETSERVICEOP a1, DWORD a2)
{
    static INT   (WSAAPI *fp)(LPWSAQUERYSETA a0, WSAESETSERVICEOP a1, DWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASetServiceA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASetServiceA")) == NULL) {
	    FATAL("cannot find entry WSASetServiceA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

INT   WINAPI
WSASetServiceW(LPWSAQUERYSETW a0, WSAESETSERVICEOP a1, DWORD a2)
{
    static INT   (WINAPI *fp)(LPWSAQUERYSETW a0, WSAESETSERVICEOP a1, DWORD a2);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASetServiceW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASetServiceW")) == NULL) {
	    FATAL("cannot find entry WSASetServiceW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2);
}

SOCKET   WINAPI
WSASocketA(int a0, int a1, int a2, LPWSAPROTOCOL_INFOA a3, GROUP a4, DWORD a5)
{
    static SOCKET   (WINAPI *fp)(int a0, int a1, int a2, LPWSAPROTOCOL_INFOA a3, GROUP a4, DWORD a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASocketA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASocketA")) == NULL) {
	    FATAL("cannot find entry WSASocketA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

SOCKET   WINAPI
WSASocketW(int a0, int a1, int a2, LPWSAPROTOCOL_INFOW a3, GROUP a4, DWORD a5)
{
    static SOCKET   (WINAPI *fp)(int a0, int a1, int a2, LPWSAPROTOCOL_INFOW a3, GROUP a4, DWORD a5);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSASocketW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSASocketW")) == NULL) {
	    FATAL("cannot find entry WSASocketW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5);
}

INT   WINAPI
WSAStringToAddressA(LPSTR a0, INT a1, LPWSAPROTOCOL_INFOA a2, LPSOCKADDR a3, LPINT a4)
{
    static INT   (WINAPI *fp)(LPSTR a0, INT a1, LPWSAPROTOCOL_INFOA a2, LPSOCKADDR a3, LPINT a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAStringToAddressA() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAStringToAddressA")) == NULL) {
	    FATAL("cannot find entry WSAStringToAddressA (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

INT   WINAPI
WSAStringToAddressW(LPWSTR a0, INT a1, LPWSAPROTOCOL_INFOW a2, LPSOCKADDR a3, LPINT a4)
{
    static INT   (WINAPI *fp)(LPWSTR a0, INT a1, LPWSAPROTOCOL_INFOW a2, LPSOCKADDR a3, LPINT a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAStringToAddressW() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAStringToAddressW")) == NULL) {
	    FATAL("cannot find entry WSAStringToAddressW (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

DWORD   WINAPI
WSAWaitForMultipleEvents(DWORD a0, const WSAEVENT * a1, BOOL a2, DWORD a3, BOOL a4)
{
    static DWORD   (WINAPI *fp)(DWORD a0, const WSAEVENT * a1, BOOL a2, DWORD a3, BOOL a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSAWaitForMultipleEvents() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSAWaitForMultipleEvents")) == NULL) {
	    FATAL("cannot find entry WSAWaitForMultipleEvents (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   WINAPI
WSCDeinstallProvider(LPGUID a0, LPINT a1)
{
    static int   (WINAPI *fp)(LPGUID a0, LPINT a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCDeinstallProvider() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCDeinstallProvider")) == NULL) {
	    FATAL("cannot find entry WSCDeinstallProvider (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   WINAPI
WSCEnableNSProvider(LPGUID a0, BOOL a1)
{
    static int   (WINAPI *fp)(LPGUID a0, BOOL a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCEnableNSProvider() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCEnableNSProvider")) == NULL) {
	    FATAL("cannot find entry WSCEnableNSProvider (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   WINAPI
WSCEnumProtocols(LPINT a0, LPWSAPROTOCOL_INFOW a1, LPDWORD a2, LPINT a3)
{
    static int   (WINAPI *fp)(LPINT a0, LPWSAPROTOCOL_INFOW a1, LPDWORD a2, LPINT a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCEnumProtocols() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCEnumProtocols")) == NULL) {
	    FATAL("cannot find entry WSCEnumProtocols (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int   WINAPI
WSCGetProviderPath(LPGUID a0, LPWSTR a1, LPINT a2, LPINT a3)
{
    static int   (WINAPI *fp)(LPGUID a0, LPWSTR a1, LPINT a2, LPINT a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCGetProviderPath() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCGetProviderPath")) == NULL) {
	    FATAL("cannot find entry WSCGetProviderPath (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

int   WINAPI
WSCInstallNameSpace(LPWSTR a0, LPWSTR a1, DWORD a2, DWORD a3, LPGUID a4)
{
    static int   (WINAPI *fp)(LPWSTR a0, LPWSTR a1, DWORD a2, DWORD a3, LPGUID a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCInstallNameSpace() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCInstallNameSpace")) == NULL) {
	    FATAL("cannot find entry WSCInstallNameSpace (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   WINAPI
WSCInstallProvider(const LPGUID a0, const LPWSTR a1, const LPWSAPROTOCOL_INFOW a2, DWORD a3, LPINT a4)
{
    static int   (WINAPI *fp)(const LPGUID a0, const LPWSTR a1, const LPWSAPROTOCOL_INFOW a2, DWORD a3, LPINT a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCInstallProvider() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCInstallProvider")) == NULL) {
	    FATAL("cannot find entry WSCInstallProvider (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   WINAPI
WSCUnInstallNameSpace(LPGUID a0)
{
    static int   (WINAPI *fp)(LPGUID a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCUnInstallNameSpace() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCUnInstallNameSpace")) == NULL) {
	    FATAL("cannot find entry WSCUnInstallNameSpace (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0);
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
WSCWriteNameSpaceOrder(int a0, int a1)
{
    static int  (PASCAL *fp)(int a0, int a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCWriteNameSpaceOrder() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCWriteNameSpaceOrder")) == NULL) {
	    FATAL("cannot find entry WSCWriteNameSpaceOrder (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   PASCAL
WSCWriteProviderOrder(LPDWORD a0, DWORD a1)
{
    static int   (PASCAL *fp)(LPDWORD a0, DWORD a1);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCWriteProviderOrder() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCWriteProviderOrder")) == NULL) {
	    FATAL("cannot find entry WSCWriteProviderOrder (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1);
}

int   PASCAL
WSANSPIoctl(HANDLE a0, DWORD a1, LPVOID a2, DWORD a3, LPVOID a4, DWORD a5, LPDWORD a6, LPVOID a7)
{
    static int   (PASCAL *fp)(HANDLE a0, DWORD a1, LPVOID a2, DWORD a3, LPVOID a4, DWORD a5, LPDWORD a6, LPVOID a7);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSANSPIoctl() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSANSPIoctl")) == NULL) {
	    FATAL("cannot find entry WSANSPIoctl (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6, a7);
}

int   PASCAL
WSCUpdateProvider(LPGUID a0, const WCHAR FAR* a1, const LPVOID a2, DWORD a3, LPINT a4)
{
    static int   (PASCAL *fp)(LPGUID a0, const WCHAR FAR* a1, const LPVOID a2, DWORD a3, LPINT a4);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub WSCUpdateProvider() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "WSCUpdateProvider")) == NULL) {
	    FATAL("cannot find entry WSCUpdateProvider (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4);
}

int   PASCAL
_org_getaddrinfo(const char* a0, const char* a1, LPVOID a2, LPVOID a3)
{
    static int   (PASCAL *fp)(const char* a0, const char* a1, LPVOID a2, LPVOID a3);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_getaddrinfo() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getaddrinfo")) == NULL) {
	    FATAL("cannot find entry getaddrinfo (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3);
}

void   PASCAL
_org_freeaddrinfo(LPVOID a0)
{
    static void   (PASCAL *fp)(LPVOID a0);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_freeaddrinfo() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "freeaddrinfo")) == NULL) {
	    FATAL("cannot find entry freeaddrinfo (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
     (*fp)(a0);
}

int   PASCAL
_org_getnameinfo(LPVOID a0, DWORD a1, char* a2, DWORD a3, char* a4, DWORD a5, int a6)
{
    static int   (PASCAL *fp)(LPVOID a0, DWORD a1, char* a2, DWORD a3, char* a4, DWORD a5, int a6);

#ifdef DEBUG_STUB
    idnLogPrintf(idn_log_level_trace, "stub _org_getnameinfo() called\n");
#endif
    if (fp == NULL) {
	void *p;
	if ((p = GetProcAddress(DLLHANDLE, "getnameinfo")) == NULL) {
	    FATAL("cannot find entry getnameinfo (%d)\n", GetLastError());
	    abort();
	}
	fp = p;
    }
    return (*fp)(a0, a1, a2, a3, a4, a5, a6);
}

