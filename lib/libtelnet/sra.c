/*-
 * Copyright (c) 1991, 1993
 *      Dave Safford.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifdef notdef
__FBSDID("$FreeBSD: src/contrib/telnet/libtelnet/sra.c,v 1.16 2002/05/06 09:48:02 markm Exp $");
#else
__RCSID("$NetBSD: sra.c,v 1.11 2012/01/09 15:25:34 christos Exp $");
#endif

#ifdef	SRA
#ifdef	ENCRYPTION
#include <sys/types.h>
#include <arpa/telnet.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ttyent.h>

#ifndef NOPAM
#include <security/pam_appl.h>
#else
#include <unistd.h>
#endif

#include "auth.h"
#include "misc.h"
#include "encrypt.h"
#include "pk.h"

char pka[HEXKEYBYTES+1], ska[HEXKEYBYTES+1], pkb[HEXKEYBYTES+1];
char *user, *pass, *xuser, *xpass;
char *passprompt, *xpassprompt;
DesData ck;
IdeaData ik;

extern int auth_debug_mode;
extern char *line; 		/* see sys_term.c */

static int sra_valid = 0;
static int passwd_sent = 0;

static unsigned char str_data[1024] = { IAC, SB, TELOPT_AUTHENTICATION, 0,
			  		AUTHTYPE_SRA, };

#define SMALL_LEN	256
#define XSMALL_LEN	513
#define SRA_KEY	0
#define SRA_USER 1
#define SRA_CONTINUE 2
#define SRA_PASS 3
#define SRA_ACCEPT 4
#define SRA_REJECT 5

static int check_user(char *, const char *);

/* support routine to send out authentication message */
static int
Data(Authenticator *ap, int type, void *d, int c)
{
        unsigned char *p = str_data + 4;
	unsigned char *cd = d;

	if (c == -1)
		c = strlen(d);

        if (auth_debug_mode) {
                printf("%s:%d: [%d] (%d)",
		    str_data[3] == TELQUAL_IS ? ">>>IS" : ">>>REPLY",
		    str_data[3], type, c);
                printd(d, c);
                printf("\r\n");
        }
	*p++ = ap->type;
	*p++ = ap->way;
	*p++ = type;
        while (c-- > 0) {
                if ((*p++ = *cd++) == IAC)
                        *p++ = IAC;
        }
        *p++ = IAC;
        *p++ = SE;
	if (str_data[3] == TELQUAL_IS)
		printsub('>', &str_data[2], p - (&str_data[2]));
        return telnet_net_write(str_data, p - str_data);
}

int
sra_init(Authenticator *ap __unused, int server)
{
	if (server)
		str_data[3] = TELQUAL_REPLY;
	else
		str_data[3] = TELQUAL_IS;

	user = malloc(SMALL_LEN);
	xuser = malloc(XSMALL_LEN);
	pass = malloc(SMALL_LEN);
	xpass = malloc(XSMALL_LEN);
	passprompt = malloc(SMALL_LEN);
	xpassprompt = malloc(XSMALL_LEN);

	if (user == NULL || xuser == NULL || pass == NULL || xpass ==
	    NULL || passprompt == NULL || xpassprompt == NULL)
		return 0; /* malloc failed */

	passwd_sent = 0;
	
	genkeys(pka, ska);
	return 1;
}

/* client received a go-ahead for sra */
int
sra_send(Authenticator *ap)
{
	/* send PKA */

	if (auth_debug_mode)
		printf("Sent PKA to server.\r\n" );
	printf("Trying SRA secure login:\r\n");
	if (!Data(ap, SRA_KEY, (void *)pka, HEXKEYBYTES)) {
		if (auth_debug_mode)
			printf("Not enough room for authentication data\r\n");
		return 0;
	}

	return 1;
}

/* server received an IS -- could be SRA KEY, USER, or PASS */
void
sra_is(Authenticator *ap, unsigned char *data, int cnt)
{
	int valid;
	Session_Key skey;

	if (cnt-- < 1)
		goto bad;
	switch (*data++) {

	case SRA_KEY:
		if (cnt < HEXKEYBYTES) {
			Data(ap, SRA_REJECT, (void *)0, 0);
			auth_finished(ap, AUTH_USER);
			if (auth_debug_mode) {
				printf("SRA user rejected for bad PKB\r\n");
			}
			return;
		}
		if (auth_debug_mode)
			printf("Sent pka\r\n");
		if (!Data(ap, SRA_KEY, (void *)pka, HEXKEYBYTES)) {
			if (auth_debug_mode)
				printf("Not enough room\r\n");
			return;
		}
		memcpy(pkb, data, HEXKEYBYTES);
		pkb[HEXKEYBYTES] = '\0';
		common_key(ska, pkb, &ik, &ck);
		return;

	case SRA_USER:
		/* decode KAB(u) */
		if (cnt > XSMALL_LEN - 1) /* Attempted buffer overflow */
			break;
		memcpy(xuser, data, cnt);
		xuser[cnt] = '\0';
		pk_decode(xuser, user, &ck);
		auth_encrypt_user(user);
#ifndef NOPAM
		(void)check_user(user, "*");
#endif
		pk_encode(passprompt, xpassprompt, &ck);
		Data(ap, SRA_CONTINUE, xpassprompt, XSMALL_LEN - 1);

		return;

	case SRA_PASS:
		if (cnt > XSMALL_LEN - 1) /* Attempted buffer overflow */
			break;
		/* decode KAB(P) */
		memcpy(xpass, data, cnt);
		xpass[cnt] = '\0';
		pk_decode(xpass, pass, &ck);

		/* check user's password */
		valid = check_user(user, pass);

		if(valid) {
			/* PAM (via check_user()) may have changed 'user' */
			auth_encrypt_user(user);
			Data(ap, SRA_ACCEPT, (void *)0, 0);
			skey.data = ck;
			skey.type = SK_DES;
			skey.length = 8;
			encrypt_session_key(&skey, 1);

			sra_valid = 1;
			auth_finished(ap, AUTH_VALID);
			if (auth_debug_mode) {
				printf("SRA user accepted\r\n");
			}
		}
		else {
			pk_encode(passprompt, xpassprompt, &ck);
			Data(ap, SRA_CONTINUE, (void *)xpassprompt,
			    XSMALL_LEN - 1);
			if (auth_debug_mode) {
				printf("SRA user failed\r\n");
			}
		}
		return;

	default:
		if (auth_debug_mode)
			printf("Unknown SRA option %d\r\n", data[-1]);
	}
bad:
	Data(ap, SRA_REJECT, 0, 0);
	sra_valid = 0;
	auth_finished(ap, AUTH_REJECT);
}

/* client received REPLY -- could be SRA KEY, CONTINUE, ACCEPT, or REJECT */
void
sra_reply(Authenticator *ap, unsigned char *data, int cnt)
{
	char uprompt[SMALL_LEN], tuser[SMALL_LEN];
	Session_Key skey;
	size_t i;

	if (cnt-- < 1)
		return;
	switch (*data++) {

	case SRA_KEY:
		/* calculate common key */
		if (cnt < HEXKEYBYTES) {
			if (auth_debug_mode) {
				printf("SRA user rejected for bad PKB\r\n");
			}
			return;
		}
		memcpy(pkb, data, HEXKEYBYTES);
		pkb[HEXKEYBYTES] = '\0'; 		

		common_key(ska, pkb, &ik, &ck);

	enc_user:

		/* encode user */
		memset(tuser, 0, sizeof(tuser));
		snprintf(uprompt, sizeof(uprompt), "User (%s): ",
		    UserNameRequested);
		if (telnet_gets(uprompt, tuser, SMALL_LEN - 1, 1) == NULL) {
			printf("\n");
			exit(1);
		}
		if (tuser[0] == '\n' || tuser[0] == '\r' )
			strlcpy(user, UserNameRequested, SMALL_LEN);
		else {
			/* telnet_gets leaves the newline on */
			for(i = 0; i < sizeof(tuser); i++) {
				if (tuser[i] == '\n') {
					tuser[i] = '\0';
					break;
				}
			}
			strlcpy(user, tuser, SMALL_LEN);
		}
		pk_encode(user, xuser, &ck);

		/* send it off */
		if (auth_debug_mode)
			printf("Sent KAB(U)\r\n");
		if (!Data(ap, SRA_USER, (void *)xuser, strlen(xuser))) {
			if (auth_debug_mode)
				printf("Not enough room\r\n");
			return;
		}
		break;

	case SRA_CONTINUE:
		if (passwd_sent) {
			passwd_sent = 0;
			printf("[ SRA login failed ]\r\n");
			goto enc_user;
		}
		if (cnt > XSMALL_LEN - 1) { 
			break;
		} else if (cnt > 0) {
			(void)memcpy(xpassprompt, data, cnt);
			pk_decode(xpassprompt, passprompt, &ck);
		} else {
			(void)strlcpy(passprompt, "Password: ", SMALL_LEN);
		}
		/* encode password */
		memset(pass, 0, SMALL_LEN);
		if (telnet_gets(passprompt, pass, SMALL_LEN - 1, 0) == NULL) {
			printf("\n");
			exit(1);
		}
		pk_encode(pass, xpass, &ck);
		/* send it off */
		if (auth_debug_mode)
			printf("Sent KAB(P)\r\n");
		if (!Data(ap, SRA_PASS, (void *)xpass, strlen(xpass))) {
			if (auth_debug_mode)
				printf("Not enough room\r\n");
			return;
		}
		passwd_sent = 1;
		break;

	case SRA_REJECT:
		printf("[ SRA refuses authentication ]\r\n");
		printf("Trying plaintext login:\r\n");
		auth_finished(0, AUTH_REJECT);
		return;

	case SRA_ACCEPT:
		printf("[ SRA accepts you ]\r\n");
		skey.data = ck;
		skey.type = SK_DES;
		skey.length = 8;
		encrypt_session_key(&skey, 0);

		auth_finished(ap, AUTH_VALID);
		return;
	default:
		if (auth_debug_mode)
			printf("Unknown SRA option %d\r\n", data[-1]);
		return;
	}
}

int
sra_status(Authenticator *ap __unused, char *name, size_t len, int level)
{
	if (level < AUTH_USER)
		return level;
	if (UserNameRequested && sra_valid) {
		strlcpy(name, UserNameRequested, len);
		return AUTH_VALID;
	} else
		return AUTH_USER;
}

#define	BUMP(buf, len)		while (*(buf)) { ++(buf), --(len); }
#define	ADDC(buf, len, c)	if ((len) > 0) { *(buf)++ = (c); --(len); }

void
sra_printsub(unsigned char *data, int cnt, unsigned char *ubuf, int buflen)
{
	char lbuf[32], *buf = (char *)ubuf;
	int i;

	buf[buflen - 1] = '\0'; 		/* make sure its NULL terminated */
	buflen -= 1;

	switch(data[3]) {

	case SRA_CONTINUE:
		strncpy(buf, " CONTINUE ", buflen);
		goto common;

	case SRA_REJECT:		/* Rejected (reason might follow) */
		strncpy(buf, " REJECT ", buflen);
		goto common;

	case SRA_ACCEPT:		/* Accepted (name might follow) */
		strncpy(buf, " ACCEPT ", buflen);

	common:
		BUMP(buf, buflen);
		if (cnt <= 4)
			break;
		ADDC(buf, buflen, '"');
		for (i = 4; i < cnt; i++)
			ADDC(buf, buflen, data[i]);
		ADDC(buf, buflen, '"');
		ADDC(buf, buflen, '\0');
		break;

	case SRA_KEY:			/* Authentication data follows */
		strncpy(buf, " KEY ", buflen);
		goto common2;

	case SRA_USER:
		strncpy(buf, " USER ", buflen);
		goto common2;

	case SRA_PASS:
		strncpy(buf, " PASS ", buflen);
		goto common2;

	default:
		snprintf(lbuf, sizeof(lbuf), " %d (unknown)", data[3]);
		strncpy(buf, lbuf, buflen);
	common2:
		BUMP(buf, buflen);
		for (i = 4; i < cnt; i++) {
			snprintf(lbuf, sizeof(lbuf), " %d", data[i]);
			strncpy(buf, lbuf, buflen);
			BUMP(buf, buflen);
		}
		break;
	}
}

#ifdef NOPAM
static int
isroot(const char *usr)
{
	struct passwd pws, *pwd;
	char pwbuf[1024];

	if (getpwnam_r(usr, &pws, pwbuf, sizeof(pwbuf), &pwd) != 0 ||
	    pwd == NULL)
		return 0;
	return (!pwd->pw_uid);
}

static int
rootterm(const char *ttyname)
{
	struct ttyent *t;
	const char *ttyn;

	ttyn = ttyname;
	if (strncmp(ttyn, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
		ttyn += sizeof(_PATH_DEV) - 1;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

static int
check_user(char *name, const char *cred)
{
	struct passwd pws, *pw;
	char pwbuf[1024];
	char *xpasswd, *salt;

	if (isroot(name) && !rootterm(line))
	{
		crypt("AA", "*"); /* Waste some time to simulate success */
		return 0;
	}

	if (getpwnam_r(name, &pws, pwbuf, sizeof(pwbuf), &pw) == 0 &&
	    pw != NULL) {
		if (pw->pw_shell == NULL) {
			return 0;
		}

		salt = pw->pw_passwd;
		xpasswd = crypt(cred, salt);
		/* The strcmp does not catch null passwords! */
		if (*pw->pw_passwd == '\0' || strcmp(xpasswd, pw->pw_passwd)) {
			return 0;
		}
		return 1;
	}
	return 0;
}
#else	/* !NOPAM */

/*
 * The following is stolen from ftpd, which stole it from the imap-uw
 * PAM module and login.c. It is needed because we can't really
 * "converse" with the user, having already gone to the trouble of
 * getting their username and password through an encrypted channel.
 */

#define COPY_STRING(s) (s ? strdup(s) : NULL)

struct cred_t {
	const char *uname;
	const char *pass;
};
typedef struct cred_t cred_t;

static int
auth_conv(int num_msg, const struct pam_message **msg,
    struct pam_response **resp, void *appdata)
{
	int i;
	cred_t *cred = appdata;
	struct pam_response *reply = malloc(sizeof(*reply) * num_msg);

	if (reply == NULL)
		return PAM_BUF_ERR;

	for (i = 0; i < num_msg; i++) {
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_ON:        /* assume want user name */
			reply[i].resp_retcode = PAM_SUCCESS;
			reply[i].resp = COPY_STRING(cred->uname);
			/* PAM frees resp. */
			break;
		case PAM_PROMPT_ECHO_OFF:       /* assume want password */
		    (void)strlcpy(passprompt, msg[i]->msg, SMALL_LEN);
		    reply[i].resp_retcode = PAM_SUCCESS;
		    reply[i].resp = COPY_STRING(cred->pass);
		    /* PAM frees resp. */
		    break;
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			reply[i].resp_retcode = PAM_SUCCESS;
			reply[i].resp = NULL;
			break;
		default:                        /* unknown message style */
			free(reply);
			return PAM_CONV_ERR;
		}
	}

	*resp = reply;
	return PAM_SUCCESS;
}

/*
 * The PAM version as a side effect may put a new username in *name.
 */
static int
check_user(char *name, const char *cred)
{
	pam_handle_t *pamh = NULL;
	const void *item;
	int rval;
	int e;
	cred_t auth_cred = { name, cred };
	struct pam_conv conv = { &auth_conv, &auth_cred };

	e = pam_start("telnetd", name, &conv, &pamh);
	if (e != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_start: %s", pam_strerror(pamh, e));
		return 0;
	}

#if 0 /* Where can we find this value? */
	e = pam_set_item(pamh, PAM_RHOST, remotehost);
	if (e != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_set_item(PAM_RHOST): %s",
			pam_strerror(pamh, e));
		return 0;
	}
#endif

	e = pam_authenticate(pamh, 0);
	switch (e) {
	case PAM_SUCCESS:
		/*
		 * With PAM we support the concept of a "template"
		 * user.  The user enters a login name which is
		 * authenticated by PAM, usually via a remote service
		 * such as RADIUS or TACACS+.  If authentication
		 * succeeds, a different but related "template" name
		 * is used for setting the credentials, shell, and
		 * home directory.  The name the user enters need only
		 * exist on the remote authentication server, but the
		 * template name must be present in the local password
		 * database.
		 *
		 * This is supported by two various mechanisms in the
		 * individual modules.  However, from the application's
		 * point of view, the template user is always passed
		 * back as a changed value of the PAM_USER item.
		 */
		if ((e = pam_get_item(pamh, PAM_USER, &item)) == 
		    PAM_SUCCESS) {
			strlcpy(name, item, SMALL_LEN);
		} else
			syslog(LOG_ERR, "Couldn't get PAM_USER: %s",
			pam_strerror(pamh, e));
#if 0	/* pam_securetty(8) should be used to enforce this */
		if (isroot(name) && !rootterm(line))
			rval = 0;
		else
#endif
			rval = 1;
		break;

	case PAM_AUTH_ERR:
	case PAM_USER_UNKNOWN:
	case PAM_MAXTRIES:
		rval = 0;
	break;

	default:
		syslog(LOG_ERR, "auth_pam: %s", pam_strerror(pamh, e));
		rval = 0;
		break;
	}

	if ((e = pam_end(pamh, e)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_end: %s", pam_strerror(pamh, e));
		rval = 0;
	}
	return rval;
}

#endif /* !NOPAM */

#endif /* ENCRYPTION */
#endif /* SRA */
