/*
 * TNET		A server program for MINIX which implements the TCP/IP
 *		suite of networking protocols.  It is based on the
 *		TCP/IP code written by Phil Karn et al, as found in
 *		his NET package for Packet Radio communications.
 *
 *		This module handles telnet option processing.
 *
 * Author:	Michael Temari, <temari@temari.ae.ge.com>  01/13/93
 *
 */
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "telnetd.h"
#include "telnet.h"
#include <stdio.h>

#define	IN_DATA	0
#define	IN_CR	1
#define	IN_IAC	2
#define	IN_IAC2	3

_PROTOTYPE(static void dowill, (int c));
_PROTOTYPE(static void dowont, (int c));
_PROTOTYPE(static void dodo, (int c));
_PROTOTYPE(static void dodont, (int c));
_PROTOTYPE(static void respond, (int ack, int option));

#define	LASTTELOPT	TELOPT_SGA

static int TelROpts[LASTTELOPT+1];
static int TelLOpts[LASTTELOPT+1];

static int telfdout;

void tel_init()
{
int i;

   for(i = 0; i <= LASTTELOPT; i++) {
	TelROpts[i] = 0;
	TelLOpts[i] = 0;
   }
}

void telopt(fdout, what, option)
int fdout;
int what;
int option;
{
char buf[3];
int len;

   buf[0] = IAC;
   buf[1] = what;
   buf[2] = option;
   len = 0;

   switch(what) {
	case DO:
		if(option <= LASTTELOPT) {
			TelROpts[option] = 1;
			len = 3;
		}
		break;
	case DONT:
		if(option <= LASTTELOPT) {
			TelROpts[option] = 1;
			len = 3;
		}
		break;
	case WILL:
		if(option <= LASTTELOPT) {
			TelLOpts[option] = 1;
			len = 3;
		}
		break;
	case WONT:
		if(option <= LASTTELOPT) {
			TelLOpts[option] = 1;
			len = 3;
		}
		break;
   }
   if(len > 0)
	(void) write(fdout, buf, len);
}

int tel_in(fdout, telout, buffer, len)
int fdout;
int telout;
char *buffer;
int len;
{
static int InState = IN_DATA;
static int ThisOpt = 0;
char *p;
char *p2;
int size;
int c;

   telfdout = telout;
   p = p2 = buffer;
   size = 0;

   while(len > 0) {
   	c = (unsigned char)*p++; len--;
	switch(InState) {
   		case IN_CR:
   			InState = IN_DATA;
   			if(c == 0 || c == '\n')
   				break;
   			/* fall through */
   		case IN_DATA:
   			if(c == IAC) {
   				InState = IN_IAC;
   				break;
   			}
   			*p2++ = c; size++;
   			if(c == '\r') InState = IN_CR;
   			break;
   		case IN_IAC:
   			switch(c) {
   				case IAC:
	   				*p2++ = c; size++;
   					InState = IN_DATA;
   					break;
   				case WILL:
   				case WONT:
   				case DO:
   				case DONT:
   					InState = IN_IAC2;
   					ThisOpt = c;
   					break;
   				case EOR:
   				case SE:
   				case NOP:
   				case BREAK:
   				case IP:
   				case AO:
   				case AYT:
   				case EC:
   				case EL:
   				case GA:
   				case SB:
   					break;
   				default:
   					break;
   			}
   			break;
   		case IN_IAC2:
   			if(size > 0) {
   				write(fdout, buffer, size);
   				p2 = buffer;
   				size = 0;
   			}
   			InState = IN_DATA;
   			switch(ThisOpt) {
   				case WILL:	dowill(c);	break;
   				case WONT:	dowont(c);	break;
   				case DO:	dodo(c);	break;
   				case DONT:	dodont(c);	break;
   			}
   			break;
   	}
   }

   if(size > 0)
   	write(fdout, buffer, size);
}

int tel_out(fdout, buf, size)
int fdout;
char *buf;
int size;
{
char *p;
int got_iac, len;

   p = buf;
   while(size > 0) {
	buf = p;
	got_iac = 0;
	if((p = (char *)memchr(buf, IAC, size)) != (char *)NULL) {
		got_iac = 1;
		p++;
	} else
		p = buf + size;
	len = p - buf;
	if(len > 0)
		(void) write(fdout, buf, len);
	if(got_iac)
		(void) write(fdout, p - 1, 1);
	size = size - len;
   }
}

static void dowill(c)
int c;
{
int ack;

   switch(c) {
	case TELOPT_BINARY:
	case TELOPT_ECHO:
	case TELOPT_SGA:
		if(TelROpts[c] == 1)
			return;
		TelROpts[c] = 1;
		ack = DO;
		break;
	default:
		ack = DONT;
   }
   respond(ack, c);
}

static void dowont(c)
int c;
{
   if(c <= LASTTELOPT) {
	if(TelROpts[c] == 0)
		return;
	TelROpts[c] = 0;
   }
   respond(DONT, c);
}

static void dodo(c)
int c;
{
int ack;

   switch(c) {
	default:
		ack = WONT;
   }
   respond(ack, c);
}

static void dodont(c)
int c;
{
   if(c <= LASTTELOPT) {
	if(TelLOpts[c] == 0)
		return;
	TelLOpts[c] = 0;
   }
   respond(WONT, c);
}

static void respond(ack, option)
int ack, option;
{
unsigned char c[3];

   c[0] = IAC;
   c[1] = ack;
   c[2] = option;
   /* write(telfdout, c, 3); */
}
