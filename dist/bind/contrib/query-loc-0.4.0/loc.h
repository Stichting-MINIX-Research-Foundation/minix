/* $Id: loc.h,v 1.1 2008-02-15 01:47:15 marka Exp $ */

#define VERSION "0.4.0"

#include "config.h"

/* Probably too many inclusions but this is to keep 'gcc -Wall' happy... */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#if SIZEOF_LONG == 4
#define u_int32_t unsigned long
#ifndef int32_t 
#define int32_t   long
#endif
#else
#define u_int32_t unsigned int
#ifndef int32_t 
#define int32_t   int
#endif
#endif

#if SIZEOF_CHAR == 1
#define u_int8_t unsigned char
#ifndef int8_t 
#define int8_t   char
#endif
#else 
#if SIZEOF_SHORT == 1
#define u_int8_t unsigned short
#ifndef int8_t 
#define int8_t   short
#endif
#else
#error "No suitable native type for storing bytes"
#endif
#endif

#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

struct list_in_addr
  {
    struct in_addr addr;
    void *next;
  };

void usage ();
void panic ();

char *getlocbyname ();
char *getlocbyaddr ();
char *getlocbynet ();
char *findRR ();
struct list_in_addr *findA ();

extern char *progname;
extern short debug;
