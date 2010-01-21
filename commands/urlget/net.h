/* net.h Copyright 2000 by Michael Temari All Rights Reserved */
/* 04/05/2000 Michael Temari <Michael@TemWare.Com> */

/* avoid clash with POSIX connect */
#define connect _connect
_PROTOTYPE(int connect, (char *host, int port));

