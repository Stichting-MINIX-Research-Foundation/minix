/*	$NetBSD: externs.h,v 1.37 2012/01/10 23:39:11 joerg Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)externs.h	8.3 (Berkeley) 5/30/95
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/termios.h>

#include <string.h>

#if defined(IPSEC)
#include <netipsec/ipsec.h>
#if defined(IPSEC_POLICY_IPSEC)
extern char *ipsec_policy_in;
extern char *ipsec_policy_out;
#endif
#endif

#ifndef	_POSIX_VDISABLE
# ifdef sun
#  include <sys/param.h>	/* pick up VDISABLE definition, mayby */
# endif
# ifdef VDISABLE
#  define _POSIX_VDISABLE VDISABLE
# else
#  define _POSIX_VDISABLE ((cc_t)'\377')
# endif
#endif

#define	SUBBUFSIZE	256

#include <sys/cdefs.h>

extern int
    autologin,		/* Autologin enabled */
    skiprc,		/* Don't process the ~/.telnetrc file */
    eight,		/* use eight bit mode (binary in and/or out */
    family,		/* address family of peer */
    flushout,		/* flush output */
    connected,		/* Are we connected to the other side? */
    globalmode,		/* Mode tty should be in */
    In3270,		/* Are we in 3270 mode? */
    telnetport,		/* Are we connected to the telnet port? */
    localflow,		/* Flow control handled locally */
    restartany,		/* If flow control, restart output on any character */
    localchars,		/* we recognize interrupt/quit */
    donelclchars,	/* the user has set "localchars" */
    showoptions,
    net,		/* Network file descriptor */
    tin,		/* Terminal input file descriptor */
    tout,		/* Terminal output file descriptor */
    crlf,		/* Should '\r' be mapped to <CR><LF> (or <CR><NUL>)? */
    autoflush,		/* flush output when interrupting? */
    autosynch,		/* send interrupt characters with SYNCH? */
    SYNCHing,		/* Is the stream in telnet SYNCH mode? */
    donebinarytoggle,	/* the user has put us in binary */
    dontlecho,		/* do we suppress local echoing right now? */
    crmod,
    netdata,		/* Print out network data flow */
    prettydump,		/* Print "netdata" output in user readable format */
#ifdef TN3270
    cursesdata,		/* Print out curses data flow */
    apitrace,		/* Trace API transactions */
#endif	/* defined(TN3270) */
    termdata,		/* Print out terminal data flow */
    telnet_debug,	/* Debug level */
    doaddrlookup,	/* do a reverse address lookup? */
    clienteof;		/* Client received EOF */

extern cc_t escape;	/* Escape to command mode */
extern cc_t rlogin;	/* Rlogin mode escape character */
#ifdef	KLUDGELINEMODE
extern cc_t echoc;	/* Toggle local echoing */
#endif

extern char
    *prompt;		/* Prompt for command. */

extern char
    doopt[],
    dont[],
    will[],
    wont[],
    options[],		/* All the little options */
    *hostname;		/* Who are we connected to? */

#ifdef	ENCRYPTION
extern void (*encrypt_output)(unsigned char *, int);
extern int (*decrypt_input)(int);
#endif	/* ENCRYPTION */

/*
 * We keep track of each side of the option negotiation.
 */

#define	MY_STATE_WILL		0x01
#define	MY_WANT_STATE_WILL	0x02
#define	MY_STATE_DO		0x04
#define	MY_WANT_STATE_DO	0x08

/*
 * Macros to check the current state of things
 */

#define	my_state_is_do(opt)		(options[opt]&MY_STATE_DO)
#define	my_state_is_will(opt)		(options[opt]&MY_STATE_WILL)
#define my_want_state_is_do(opt)	(options[opt]&MY_WANT_STATE_DO)
#define my_want_state_is_will(opt)	(options[opt]&MY_WANT_STATE_WILL)

#define	my_state_is_dont(opt)		(!my_state_is_do(opt))
#define	my_state_is_wont(opt)		(!my_state_is_will(opt))
#define my_want_state_is_dont(opt)	(!my_want_state_is_do(opt))
#define my_want_state_is_wont(opt)	(!my_want_state_is_will(opt))

#define	set_my_state_do(opt)		{options[opt] |= MY_STATE_DO;}
#define	set_my_state_will(opt)		{options[opt] |= MY_STATE_WILL;}
#define	set_my_want_state_do(opt)	{options[opt] |= MY_WANT_STATE_DO;}
#define	set_my_want_state_will(opt)	{options[opt] |= MY_WANT_STATE_WILL;}

#define	set_my_state_dont(opt)		{options[opt] &= ~MY_STATE_DO;}
#define	set_my_state_wont(opt)		{options[opt] &= ~MY_STATE_WILL;}
#define	set_my_want_state_dont(opt)	{options[opt] &= ~MY_WANT_STATE_DO;}
#define	set_my_want_state_wont(opt)	{options[opt] &= ~MY_WANT_STATE_WILL;}

/*
 * Make everything symmetrical
 */

#define	HIS_STATE_WILL			MY_STATE_DO
#define	HIS_WANT_STATE_WILL		MY_WANT_STATE_DO
#define HIS_STATE_DO			MY_STATE_WILL
#define HIS_WANT_STATE_DO		MY_WANT_STATE_WILL

#define	his_state_is_do			my_state_is_will
#define	his_state_is_will		my_state_is_do
#define his_want_state_is_do		my_want_state_is_will
#define his_want_state_is_will		my_want_state_is_do

#define	his_state_is_dont		my_state_is_wont
#define	his_state_is_wont		my_state_is_dont
#define his_want_state_is_dont		my_want_state_is_wont
#define his_want_state_is_wont		my_want_state_is_dont

#define	set_his_state_do		set_my_state_will
#define	set_his_state_will		set_my_state_do
#define	set_his_want_state_do		set_my_want_state_will
#define	set_his_want_state_will		set_my_want_state_do

#define	set_his_state_dont		set_my_state_wont
#define	set_his_state_wont		set_my_state_dont
#define	set_his_want_state_dont		set_my_want_state_wont
#define	set_his_want_state_wont		set_my_want_state_dont


extern FILE
    *NetTrace;		/* Where debugging output goes */
extern char
    NetTraceFile[];	/* Name of file where debugging output goes */

extern jmp_buf
    toplevel;		/* For error conditions. */


/* authenc.c */
int telnet_net_write(unsigned char *, int);
void net_encrypt(void);
int telnet_spin(void);
char *telnet_getenv(char *);
char *telnet_gets(char *, char *, int, int);

/* commands.c */
int send_tncmd(void (*)(int, int), const char *, char *);
void _setlist_init(void);
void set_escape_char(char *);
int set_mode(int);
int clear_mode(int);
int modehelp(int);
int suspend(int, char *[]);
int shell(int, char *[]);
int quit(int, char *[]);
int logout(int, char *[]);
int env_cmd(int, char *[]);
struct env_lst *env_find(const unsigned char *);
void env_init(void);
struct env_lst *env_define(const unsigned char *, unsigned char *);
struct env_lst *env_undefine(const unsigned char *, unsigned char *);
struct env_lst *env_export(const unsigned char *, unsigned char *);
struct env_lst *env_unexport(const unsigned char *, unsigned char *);
struct env_lst *env_send(const unsigned char *, unsigned char *);
struct env_lst *env_list(const unsigned char *, unsigned char *);
unsigned char *env_default(int, int );
unsigned char *env_getvalue(const unsigned char *);
void env_varval(const unsigned char *);
int auth_cmd(int, char *[]);
int ayt_status(void);
int encrypt_cmd(int, char *[]);
int tn(int, char *[]);
void command(int, const char *, int);
void cmdrc(const char *, const char *);
struct addrinfo;
int sourceroute(struct addrinfo *, char *, char **, int *, int*);

/* main.c */
void tninit(void);
void usage(void) __dead;

/* network.c */
void init_network(void);
int stilloob(void);
void setneturg(void);
int netflush(void);

/* sys_bsd.c */
void init_sys(void);
int TerminalWrite(char *, int);
int TerminalRead(unsigned char *, int);
int TerminalAutoFlush(void);
int TerminalSpecialChars(int);
void TerminalFlushOutput(void);
void TerminalSaveState(void);
cc_t *tcval(int);
void TerminalDefaultChars(void);
void TerminalRestoreState(void);
void TerminalNewMode(int);
void TerminalSpeeds(long *, long *);
int TerminalWindowSize(long *, long *);
int NetClose(int);
void NetNonblockingIO(int, int);
void NetSigIO(int, int);
void NetSetPgrp(int);
void sys_telnet_init(void);
int process_rings(int , int , int , int , int , int);

/* telnet.c */
void init_telnet(void);
void send_do(int, int );
void send_dont(int, int );
void send_will(int, int );
void send_wont(int, int );
void willoption(int);
void wontoption(int);
char **mklist(char *, char *);
int is_unique(char *, char **, char **);
int setup_term(char *, int, int *);
char *gettermname(void);
void lm_will(unsigned char *, int);
void lm_wont(unsigned char *, int);
void lm_do(unsigned char *, int);
void lm_dont(unsigned char *, int);
void lm_mode(unsigned char *, int, int );
void slc_init(void);
void slcstate(void);
void slc_mode_export(int);
void slc_mode_import(int);
void slc_import(int);
void slc_export(void);
void slc(unsigned char *, int);
void slc_check(void);
void slc_start_reply(void);
void slc_add_reply(unsigned int, unsigned int, cc_t);
void slc_end_reply(void);
int slc_update(void);
void env_opt(unsigned char *, int);
void env_opt_start(void);
void env_opt_start_info(void);
void env_opt_add(unsigned char *);
int opt_welldefined(const char *);
void env_opt_end(int);
int telrcv(void);
int rlogin_susp(void);
int Scheduler(int);
void telnet(const char *);
void xmitAO(void);
void xmitEL(void);
void xmitEC(void);
int dosynch(char *);
int get_status(char *);
void intp(void);
void sendbrk(void);
void sendabort(void);
void sendsusp(void);
void sendeof(void);
void sendayt(void);
void sendnaws(void);
void tel_enter_binary(int);
void tel_leave_binary(int);

/* terminal.c */
void init_terminal(void);
int ttyflush(int);
int getconnmode(void);
void setconnmode(int);
void setcommandmode(void);

/* utilities.c */
void upcase(char *);
int SetSockOpt(int, int, int, int);
void SetNetTrace(char *);
void Dump(int, unsigned char *, int);
void printoption(const char *, int, int );
void optionstatus(void);
void printsub(int, unsigned char *, int);
void EmptyTerminal(void);
void SetForExit(void);
void Exit(int) __attribute__((__noreturn__));
void ExitString(const char *, int) __attribute__((__noreturn__));


extern struct	termios new_tc;

# define termEofChar		new_tc.c_cc[VEOF]
# define termEraseChar		new_tc.c_cc[VERASE]
# define termIntChar		new_tc.c_cc[VINTR]
# define termKillChar		new_tc.c_cc[VKILL]
# define termQuitChar		new_tc.c_cc[VQUIT]

#  define termSuspChar		new_tc.c_cc[VSUSP]
#  define termFlushChar		new_tc.c_cc[VDISCARD]
#  define termWerasChar		new_tc.c_cc[VWERASE]
#  define termRprntChar		new_tc.c_cc[VREPRINT]
#  define termLiteralNextChar	new_tc.c_cc[VLNEXT]
#  define termStartChar		new_tc.c_cc[VSTART]
#  define termStopChar		new_tc.c_cc[VSTOP]
#  define termForw1Char		new_tc.c_cc[VEOL]
#  define termForw2Char		new_tc.c_cc[VEOL]
#  define termAytChar		new_tc.c_cc[VSTATUS]

# define termEofCharp		&termEofChar
# define termEraseCharp		&termEraseChar
# define termIntCharp		&termIntChar
# define termKillCharp		&termKillChar
# define termQuitCharp		&termQuitChar
# define termSuspCharp		&termSuspChar
# define termFlushCharp		&termFlushChar
# define termWerasCharp		&termWerasChar
# define termRprntCharp		&termRprntChar
# define termLiteralNextCharp	&termLiteralNextChar
# define termStartCharp		&termStartChar
# define termStopCharp		&termStopChar
# define termForw1Charp		&termForw1Char
# define termForw2Charp		&termForw2Char
# define termAytCharp		&termAytChar


/* Tn3270 section */
#if	defined(TN3270)

extern int
    HaveInput,		/* Whether an asynchronous I/O indication came in */
    noasynchtty,	/* Don't do signals on I/O (SIGURG, SIGIO) */
    noasynchnet,	/* Don't do signals on I/O (SIGURG, SIGIO) */
    sigiocount,		/* Count of SIGIO receptions */
    shell_active;	/* Subshell is active */

extern char
    *Ibackp,		/* Oldest byte of 3270 data */
    Ibuf[],		/* 3270 buffer */
    *Ifrontp,		/* Where next 3270 byte goes */
    tline[200],
    *transcom;		/* Transparent command */

/* tn3270.c */
void init_3270(void);
int DataToNetwork(char *, int, int);
void inputAvailable(int);
void outputPurge(void);
int DataToTerminal(char *, int);
int Push3270(void);
void Finish3270(void);
void StringToTerminal(char *);
int _putchar(int);
void SetIn3270(void);
int tn3270_ttype(void);
int settranscom(int, char *[]);
int shell_continue(void);
int DataFromTerminal(char *, int);
int DataFromNetwork(char *, int, int);
void ConnectScreen(void);
int DoTerminalOutput(void);

#endif	/* defined(TN3270) */
