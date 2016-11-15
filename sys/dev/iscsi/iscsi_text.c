/*	$NetBSD: iscsi_text.c,v 1.9 2015/05/30 16:12:34 joerg Exp $	*/

/*-
 * Copyright (c) 2005,2006,2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "iscsi_globals.h"
#include "base64.h"
#include <sys/md5.h>
#include <sys/cprng.h>

#define isdigit(x) ((x) >= '0' && (x) <= '9')
#define toupper(x) ((x) & ~0x20)

/*****************************************************************************/

#define MAX_STRING   255	/* Maximum length of parameter value */
#define MAX_LIST     4		/* Maximum number of list elements we'll ever send */

/* Maximum number of negotiation parameters in the operational negotiation phase */
/* 48 should be more than enough even with the target defining its own keys */
#define MAX_NEG      48

#define CHAP_CHALLENGE_LEN    32	/* Number of bytes to send in challenge */
#define CHAP_MD5_SIZE         16	/* Number of bytes in MD5 hash */

/*****************************************************************************/

/* authentication states */

typedef enum
{
	AUTH_INITIAL,				/* sending choice of algorithms */
	AUTH_METHOD_SELECTED,		/* received choice, sending first parameter */
	/* from here it's alg dependent */
	AUTH_CHAP_ALG_SENT,			/* CHAP: Algorithm selected */
	AUTH_CHAP_RSP_SENT,			/* CHAP: Response sent */
	/* for all algorithms */
	AUTH_DONE					/* in parameter negotiation stage */
} auth_state_t;


/* enumeration of all the keys we know, and a place for the ones we don't */

typedef enum
{
	K_AuthMethod,
	K_Auth_CHAP_Algorithm,
	K_Auth_CHAP_Challenge,
	K_Auth_CHAP_Identifier,
	K_Auth_CHAP_Name,
	K_Auth_CHAP_Response,
	K_DataDigest,
	K_DataPDUInOrder,
	K_DataSequenceInOrder,
	K_DefaultTime2Retain,
	K_DefaultTime2Wait,
	K_ErrorRecoveryLevel,
	K_FirstBurstLength,
	K_HeaderDigest,
	K_IFMarker,
	K_IFMarkInt,
	K_ImmediateData,
	K_InitialR2T,
	K_InitiatorAlias,
	K_InitiatorName,
	K_MaxBurstLength,
	K_MaxConnections,
	K_MaxOutstandingR2T,
	K_MaxRecvDataSegmentLength,
	K_OFMarker,
	K_OFMarkInt,
	K_SendTargets,
	K_SessionType,
	K_TargetAddress,
	K_TargetAlias,
	K_TargetName,
	K_TargetPortalGroupTag,
	K_NotUnderstood
} text_key_t;

/* maximum known key */
#define MAX_KEY   K_TargetPortalGroupTag


#undef DEBOUT
#define DEBOUT(x)	printf x



/* value types */
typedef enum
{						/* Value is... */
	T_NUM,					/* numeric */
	T_BIGNUM,				/* large numeric */
	T_STRING,				/* string */
	T_YESNO,				/* boolean (Yes or No) */
	T_AUTH,					/* authentication type (CHAP or None for now) */
	T_DIGEST,				/* digest (None or CRC32C) */
	T_RANGE,				/* numeric range */
	T_SENDT,				/* send target options (ALL, target-name, empty) */
	T_SESS					/* session type (Discovery or Normal) */
} val_kind_t;


/* table of negotiation key strings with value type and default */

typedef struct
{
	const uint8_t *name;				/* the key name */
	val_kind_t val;				/* the value type */
	uint32_t defval;			/* default value */
} key_entry_t;

STATIC key_entry_t entries[] = {
	{"AuthMethod", T_AUTH, 0},
	{"CHAP_A", T_NUM, 5},
	{"CHAP_C", T_BIGNUM, 0},
	{"CHAP_I", T_NUM, 0},
	{"CHAP_N", T_STRING, 0},
	{"CHAP_R", T_BIGNUM, 0},
	{"DataDigest", T_DIGEST, 0},
	{"DataPDUInOrder", T_YESNO, 1},
	{"DataSequenceInOrder", T_YESNO, 1},
	{"DefaultTime2Retain", T_NUM, 20},
	{"DefaultTime2Wait", T_NUM, 2},
	{"ErrorRecoveryLevel", T_NUM, 0},
	{"FirstBurstLength", T_NUM, 64 * 1024},
	{"HeaderDigest", T_DIGEST, 0},
	{"IFMarker", T_YESNO, 0},
	{"IFMarkInt", T_RANGE, 2048},
	{"ImmediateData", T_YESNO, 1},
	{"InitialR2T", T_YESNO, 1},
	{"InitiatorAlias", T_STRING, 0},
	{"InitiatorName", T_STRING, 0},
	{"MaxBurstLength", T_NUM, 256 * 1024},
	{"MaxConnections", T_NUM, 1},
	{"MaxOutstandingR2T", T_NUM, 1},
	{"MaxRecvDataSegmentLength", T_NUM, 8192},
	{"OFMarker", T_YESNO, 0},
	{"OFMarkInt", T_RANGE, 2048},
	{"SendTargets", T_SENDT, 0},
	{"SessionType", T_SESS, 0},
	{"TargetAddress", T_STRING, 0},
	{"TargetAlias", T_STRING, 0},
	{"TargetName", T_STRING, 0},
	{"TargetPortalGroupTag", T_NUM, 0},
	{NULL, T_STRING, 0}
};

/* a negotiation parameter: key and values (there may be more than 1 for lists) */
typedef struct
{
	text_key_t key;				/* the key */
	int list_num;				/* number of elements in list, doubles as */
	/* data size for large numeric values */
	union
	{
		uint32_t nval[MAX_LIST];	/* numeric or enumeration values */
		uint8_t *sval;				/* string or data pointer */
	} val;
} negotiation_parameter_t;


/* Negotiation state flags */
#define NS_SENT      0x01		/* key was sent to target */
#define NS_RECEIVED  0x02		/* key was received from target */

typedef struct
{
	negotiation_parameter_t pars[MAX_NEG];	/* the parameters to send */
	negotiation_parameter_t *cpar;			/* the last parameter set */
	uint16_t num_pars;						/* number of parameters to send */
	auth_state_t auth_state;				/* authentication state */
	iscsi_auth_types_t auth_alg;			/* authentication algorithm */
	uint8_t kflags[MAX_KEY + 2];			/* negotiation flags for each key */
	uint8_t password[MAX_STRING + 1];		/* authentication secret */
	uint8_t target_password[MAX_STRING + 1];	/* target authentication secret */
	uint8_t user_name[MAX_STRING + 1];		/* authentication user ID */
	uint8_t temp_buf[MAX_STRING + 1];		/* scratch buffer */

	bool HeaderDigest;
	bool DataDigest;
	bool InitialR2T;
	bool ImmediateData;
	uint32_t ErrorRecoveryLevel;
	uint32_t MaxRecvDataSegmentLength;
	uint32_t MaxConnections;
	uint32_t DefaultTime2Wait;
	uint32_t DefaultTime2Retain;
	uint32_t MaxBurstLength;
	uint32_t FirstBurstLength;
	uint32_t MaxOutstandingR2T;

} negotiation_state_t;


#define TX(state, key) (state->kflags [key] & NS_SENT)
#define RX(state, key) (state->kflags [key] & NS_RECEIVED)

/*****************************************************************************/


STATIC void
chap_md5_response(uint8_t *buffer, uint8_t identifier, uint8_t *secret,
				  uint8_t *challenge, int challenge_size)
{
	MD5_CTX md5;

	MD5Init(&md5);
	MD5Update(&md5, &identifier, 1);
	MD5Update(&md5, secret, strlen(secret));
	MD5Update(&md5, challenge, challenge_size);
	MD5Final(buffer, &md5);
}


/*****************************************************************************/

/*
 * hexdig:
 *    Return value of hex digit.
 *    Note: a null character is acceptable, and returns 0.
 *
 *    Parameter:
 *          c     The character
 *
 *    Returns:    The value, -1 on error.
 */

static __inline int
hexdig(uint8_t c)
{

	if (!c) {
		return 0;
	}
	if (isdigit(c)) {
		return c - '0';
	}
	c = toupper(c);
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

/*
 * skiptozero:
 *    Skip to next zero character in buffer.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *
 *    Returns:    The pointer to the character after the zero character.
 */

static __inline uint8_t *
skiptozero(uint8_t *buf)
{

	while (*buf) {
		buf++;
	}
	return buf + 1;
}


/*
 * get_bignumval:
 *    Get a large numeric value.
 *    NOTE: Overwrites source string.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          par      The parameter
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_bignumval(uint8_t *buf, negotiation_parameter_t *par)
{
	int val;
	char c;
	uint8_t *dp = buf;

	par->val.sval = buf;

	if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
		buf += 2;
		while ((c = *buf) != 0x0) {
			buf++;
			val = (hexdig(c) << 4) | hexdig(*buf);
			if (val < 0) {
				return NULL;
			}
			*dp++ = (uint8_t) val;
			if (*buf) {
				buf++;
			}
		}
		buf++;
		par->list_num = dp - par->val.sval;
	} else if (buf[0] == '0' && (buf[1] == 'b' || buf[1] == 'B')) {
		buf = base64_decode(&buf[2], par->val.sval, &par->list_num);
	} else {
		DEBOUT(("Ill-formatted large number <%s>\n", buf));
		return NULL;
	}

	return buf;
}


/*
 * get_numval:
 *    Get a numeric value.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pval     The pointer to the result.
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_numval(uint8_t *buf, uint32_t *pval)
{
	uint32_t val = 0;
	char c;

	if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
		buf += 2;
		while (*buf && *buf != '~') {
			int n;

			if ((n = hexdig(*buf++)) < 0)
				return NULL;
			val = (val << 4) | n;
		}
	} else
		while (*buf && *buf != '~') {
			c = *buf++;
			if (!isdigit(c))
				return NULL;
			val = val * 10 + (c - '0');
		}

	*pval = val;

	return buf + 1;
}


/*
 * get_range:
 *    Get a numeric range.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pval1    The pointer to the first result.
 *          pval2    The pointer to the second result.
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_range(uint8_t *buf, uint32_t *pval1, uint32_t *pval2)
{

	if ((buf = get_numval(buf, pval1)) == NULL)
		return NULL;
	if (!*buf)
		return NULL;
	if ((buf = get_numval(buf, pval2)) == NULL)
		return NULL;
	return buf;
}


/*
 * get_ynval:
 *    Get a yes/no selection.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pval     The pointer to the result.
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_ynval(uint8_t *buf, uint32_t *pval)
{

	if (strcmp(buf, "Yes") == 0)
		*pval = 1;
	else if (strcmp(buf, "No") == 0)
		*pval = 0;
	else
		return NULL;

	return skiptozero(buf);
}


/*
 * get_digestval:
 *    Get a digest selection.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pval     The pointer to the result.
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_digestval(uint8_t *buf, uint32_t *pval)
{

	if (strcmp(buf, "CRC32C") == 0)
		*pval = 1;
	else if (strcmp(buf, "None") == 0)
		*pval = 0;
	else
		return NULL;

	return skiptozero(buf);
}


/*
 * get_authval:
 *    Get an authentication method.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pval     The pointer to the result.
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_authval(uint8_t *buf, uint32_t *pval)
{

	if (strcmp(buf, "None") == 0)
		*pval = ISCSI_AUTH_None;
	else if (strcmp(buf, "CHAP") == 0)
		*pval = ISCSI_AUTH_CHAP;
	else if (strcmp(buf, "KRB5") == 0)
		*pval = ISCSI_AUTH_KRB5;
	else if (strcmp(buf, "SRP") == 0)
		*pval = ISCSI_AUTH_SRP;
	else
		return NULL;

	return skiptozero(buf);
}


/*
 * get_strval:
 *    Get a string value (returns pointer to original buffer, not a copy).
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pval     The pointer to the result pointer.
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_strval(uint8_t *buf, uint8_t **pval)
{

	if (strlen(buf) > MAX_STRING)
		return NULL;

	*pval = buf;

	return skiptozero(buf);
}


/*
 * get_parameter:
 *    Analyze a key=value string.
 *    NOTE: The string is modified in the process.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          par      The parameter descriptor to be filled in
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC uint8_t *
get_parameter(uint8_t *buf, negotiation_parameter_t *par)
{
	uint8_t *bp = buf;
	int i;

	while (*bp && *bp != '=') {
		bp++;
	}
	if (!*bp) {
		DEBOUT(("get_parameter: Premature end of parameter\n"));
		return NULL;
	}

	*bp++ = 0;

	for (i = 0; i <= MAX_KEY; i++)
		if (!strcmp(buf, entries[i].name))
			break;

	par->key = i;
	par->list_num = 1;

	if (i > MAX_KEY) {
		DEBOUT(("get_parameter: unrecognized key <%s>\n", buf));
		if (strlen(buf) > MAX_STRING) {
			DEBOUT(("get_parameter: key name > MAX_STRING\n"));
			return NULL;
		}
		par->val.sval = buf;
		return skiptozero(bp);
	}

	DEB(10, ("get_par: key <%s>=%d, val=%d, ret %p\n",
			buf, i, entries[i].val, bp));
	DEB(10, ("get_par: value '%s'\n",bp));

	switch (entries[i].val) {
	case T_NUM:
		bp = get_numval(bp, &par->val.nval[0]);
		break;

	case T_BIGNUM:
		bp = get_bignumval(bp, par);
		break;

	case T_STRING:
		bp = get_strval(bp, &par->val.sval);
		break;

	case T_YESNO:
		bp = get_ynval(bp, &par->val.nval[0]);
		break;

	case T_AUTH:
		bp = get_authval(bp, &par->val.nval[0]);
		break;

	case T_DIGEST:
		bp = get_digestval(bp, &par->val.nval[0]);
		break;

	case T_RANGE:
		bp = get_range(bp, &par->val.nval[0], &par->val.nval[1]);
		break;

	default:
		/* Target sending any other types is wrong */
		bp = NULL;
		break;
	}
	return bp;
}

/*****************************************************************************/

/*
 * my_strcpy:
 *    Replacement for strcpy that returns the end of the result string
 *
 *    Parameter:
 *          dest     The destination buffer pointer
 *          src      The source string
 *
 *    Returns:    A pointer to the terminating zero of the result.
 */

static __inline unsigned
my_strcpy(uint8_t *dest, const uint8_t *src)
{
	unsigned	cc;

	for (cc = 0 ; (*dest = *src) != 0x0 ; cc++) {
		dest++;
		src++;
	}
	return cc;
}

/*
 * put_bignumval:
 *    Write a large numeric value.
 *    NOTE: Overwrites source string.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          par      The parameter
 *
 *    Returns:    The pointer to the next parameter, NULL on error.
 */

STATIC unsigned
put_bignumval(negotiation_parameter_t *par, uint8_t *buf)
{
	return base64_encode(par->val.sval, par->list_num, buf);
}

/*
 * put_parameter:
 *    Create a key=value string.
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          par      The parameter descriptor
 *
 *    Returns:    The pointer to the next free buffer space, NULL on error.
 */

STATIC unsigned
put_parameter(uint8_t *buf, unsigned len, negotiation_parameter_t *par)
{
	int i;
	unsigned	cc, cl;
	const uint8_t *sp;

	DEB(10, ("put_par: key <%s>=%d, val=%d\n",
		entries[par->key].name, par->key, entries[par->key].val));

	if (par->key > MAX_KEY) {
		return snprintf(buf, len, "%s=NotUnderstood", par->val.sval);
	}

	cc = snprintf(buf, len, "%s=", entries[par->key].name);
	if (cc >= len)
		return len;

	for (i = 0; i < par->list_num; i++) {
		switch (entries[par->key].val) {
		case T_NUM:
			cl = snprintf(&buf[cc], len - cc, "%d",
			               par->val.nval[i]);
			break;

		case T_BIGNUM:
			cl = put_bignumval(par, &buf[cc]);
			i = par->list_num;
			break;

		case T_STRING:
			cl =  my_strcpy(&buf[cc], par->val.sval);
			break;

		case T_YESNO:
			cl = my_strcpy(&buf[cc],
				(par->val.nval[i]) ? "Yes" : "No");
			break;

		case T_AUTH:
			switch (par->val.nval[i]) {
			case ISCSI_AUTH_CHAP:
				sp = "CHAP";
				break;
			case ISCSI_AUTH_KRB5:
				sp = "KRB5";
				break;
			case ISCSI_AUTH_SRP:
				sp = "SRP";
				break;
			default:
				sp = "None";
				break;
			}
			cl = my_strcpy(&buf[cc], sp);
			break;

		case T_DIGEST:
			cl = my_strcpy(&buf[cc],
				(par->val.nval[i]) ? "CRC32C" : "None");
			break;

		case T_RANGE:
			if ((i + 1) >= par->list_num) {
				cl = my_strcpy(&buf[cc], "Reject");
			} else {
				cl = snprintf(&buf[cc], len - cc,
						"%d~%d", par->val.nval[i],
						par->val.nval[i + 1]);
				i++;
			}
			break;

		case T_SENDT:
			cl = my_strcpy(&buf[cc], par->val.sval);
			break;

		case T_SESS:
			cl = my_strcpy(&buf[cc],
				(par->val.nval[i]) ? "Normal" : "Discovery");
			break;

		default:
			cl = 0;
			/* We should't be here... */
			DEBOUT(("Invalid type %d in put_parameter!\n",
					entries[par->key].val));
			break;
		}

		DEB(10, ("put_par: value '%s'\n",&buf[cc]));

		cc += cl;
		if (cc >= len)
			return len;
		if ((i + 1) < par->list_num) {
			if (cc >= len)
				return len;
			buf[cc++] = ',';
		}
	}

	if (cc >= len)
		return len;
	buf[cc] = 0x0;				/* make sure it's terminated */
	return cc + 1;				/* return next place in list */
}


/*
 * put_par_block:
 *    Fill a parameter block
 *
 *    Parameter:
 *          buf      The buffer pointer
 *          pars     The parameter descriptor array
 *          n        The number of elements
 *
 *    Returns:    result from put_parameter (ptr to buffer, NULL on error)
 */

static __inline unsigned
put_par_block(uint8_t *buf, unsigned len, negotiation_parameter_t *pars, int n)
{
	unsigned	cc;
	int i;

	for (cc = 0, i = 0; i < n; i++) {
		cc += put_parameter(&buf[cc], len - cc, pars++);
		if (cc >= len) {
			break;
		}
	}
	return cc;
}

/*
 * parameter_size:
 *    Determine the size of a key=value string.
 *
 *    Parameter:
 *          par      The parameter descriptor
 *
 *    Returns:    The size of the resulting string.
 */

STATIC int
parameter_size(negotiation_parameter_t *par)
{
	int i, size;
	char buf[24];	/* max. 2 10-digit numbers + sep. */

	if (par->key > MAX_KEY) {
		return strlen(par->val.sval) + 15;
	}
	/* count '=' and terminal zero */
	size = strlen(entries[par->key].name) + 2;

	for (i = 0; i < par->list_num; i++) {
		switch (entries[par->key].val) {
		case T_NUM:
			size += snprintf(buf, sizeof(buf), "%d",
					par->val.nval[i]);
			break;

		case T_BIGNUM:
			/* list_num holds value size */
			size += base64_enclen(par->list_num);
			i = par->list_num;
			break;

		case T_STRING:
		case T_SENDT:
			size += strlen(par->val.sval);
			break;

		case T_YESNO:
			size += (par->val.nval[i]) ? 3 : 2;
			break;

		case T_AUTH:
			size += (par->val.nval[i] == ISCSI_AUTH_SRP) ? 3 : 4;
			break;

		case T_DIGEST:
			size += (par->val.nval[i]) ? 6 : 4;
			break;

		case T_RANGE:
			assert((i + 1) < par->list_num);
			size += snprintf(buf, sizeof(buf), "%d~%d",
				par->val.nval[i],
							par->val.nval[i + 1]);
			i++;
			break;

		case T_SESS:
			size += (par->val.nval[i]) ? 6 : 9;
			break;

		default:
			/* We should't be here... */
			DEBOUT(("Invalid type %d in parameter_size!\n",
					entries[par->key].val));
			break;
		}
		if ((i + 1) < par->list_num) {
			size++;
		}
	}

	return size;
}


/*
 * total_size:
 *    Determine the size of a negotiation data block
 *
 *    Parameter:
 *          pars     The parameter descriptor array
 *          n        The number of elements
 *
 *    Returns:    The size of the block
 */

static __inline int
total_size(negotiation_parameter_t *pars, int n)
{
	int i, size;

	for (i = 0, size = 0; i < n; i++) {
		size += parameter_size(pars++);
	}
	return size;
}

/*****************************************************************************/


/*
 * complete_pars:
 *    Allocate space for text parameters, translate parameter values into
 *    text.
 *
 *    Parameter:
 *          state    Negotiation state
 *          pdu      The transmit PDU
 *
 *    Returns:    0     On success
 *                > 0   (an ISCSI error code) if an error occurred.
 */

STATIC int
complete_pars(negotiation_state_t *state, pdu_t *pdu)
{
	int len;
	uint8_t *bp;

	len = total_size(state->pars, state->num_pars);

	DEB(10, ("complete_pars: n=%d, len=%d\n", state->num_pars, len));

	if ((bp = malloc(len, M_TEMP, M_WAITOK)) == NULL) {
		DEBOUT(("*** Out of memory in complete_pars\n"));
		return ISCSI_STATUS_NO_RESOURCES;
	}
	pdu->temp_data = bp;

	if (put_par_block(pdu->temp_data, len, state->pars,
			state->num_pars) == 0) {
		DEBOUT(("Bad parameter in complete_pars\n"));
		return ISCSI_STATUS_PARAMETER_INVALID;
	}

	pdu->temp_data_len = len;
	return 0;
}


/*
 * set_key_n:
 *    Initialize a key and its numeric value.
 *
 *    Parameter:
 *          state    Negotiation state
 *          key      The key
 *          val      The value
 */

STATIC negotiation_parameter_t *
set_key_n(negotiation_state_t *state, text_key_t key, uint32_t val)
{
	negotiation_parameter_t *par;

	if (state->num_pars >= MAX_NEG) {
		DEBOUT(("set_key_n: num_pars (%d) >= MAX_NEG (%d)\n",
				state->num_pars, MAX_NEG));
		return NULL;
	}
	par = &state->pars[state->num_pars];
	par->key = key;
	par->list_num = 1;
	par->val.nval[0] = val;
	state->num_pars++;
	state->kflags[key] |= NS_SENT;

	return par;
}

/*
 * set_key_s:
 *    Initialize a key and its string value.
 *
 *    Parameter:
 *          state    Negotiation state
 *          key      The key
 *          val      The value
 */

STATIC negotiation_parameter_t *
set_key_s(negotiation_state_t *state, text_key_t key, uint8_t *val)
{
	negotiation_parameter_t *par;

	if (state->num_pars >= MAX_NEG) {
		DEBOUT(("set_key_s: num_pars (%d) >= MAX_NEG (%d)\n",
				state->num_pars, MAX_NEG));
		return NULL;
	}
	par = &state->pars[state->num_pars];
	par->key = key;
	par->list_num = 1;
	par->val.sval = val;
	state->num_pars++;
	state->kflags[key] |= NS_SENT;

	return par;
}


/*****************************************************************************/

/*
 * eval_parameter:
 *    Evaluate a received negotiation value.
 *
 *    Parameter:
 *          conn     The connection
 *          state    The negotiation state
 *          par      The parameter
 *
 *    Returns:    0 on success, else an ISCSI status value.
 */

STATIC int
eval_parameter(connection_t *conn, negotiation_state_t *state,
			   negotiation_parameter_t *par)
{
	uint32_t n = par->val.nval[0];
	size_t sz;
	text_key_t key = par->key;
	bool sent = (state->kflags[key] & NS_SENT) != 0;

	state->kflags[key] |= NS_RECEIVED;

	switch (key) {
		/*
		 *  keys connected to security negotiation
		 */
	case K_AuthMethod:
		if (n) {
			DEBOUT(("eval_par: AuthMethod nonzero (%d)\n", n));
			return ISCSI_STATUS_NEGOTIATION_ERROR;
		}
		break;

	case K_Auth_CHAP_Algorithm:
	case K_Auth_CHAP_Challenge:
	case K_Auth_CHAP_Identifier:
	case K_Auth_CHAP_Name:
	case K_Auth_CHAP_Response:
		DEBOUT(("eval_par: Authorization Key in Operational Phase\n"));
		return ISCSI_STATUS_NEGOTIATION_ERROR;

		/*
		 * keys we always send
		 */
	case K_DataDigest:
		state->DataDigest = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_HeaderDigest:
		state->HeaderDigest = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_ErrorRecoveryLevel:
		state->ErrorRecoveryLevel = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_ImmediateData:
		state->ImmediateData = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_InitialR2T:
		state->InitialR2T = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_MaxRecvDataSegmentLength:
		state->MaxRecvDataSegmentLength = n;
		/* this is basically declarative, not negotiated */
		/* (each side has its own value) */
		break;

		/*
		 * keys we don't always send, so we may have to reflect the value
		 */
	case K_DefaultTime2Retain:
		state->DefaultTime2Retain = n = min(state->DefaultTime2Retain, n);
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_DefaultTime2Wait:
		state->DefaultTime2Wait = n = min(state->DefaultTime2Wait, n);
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_MaxConnections:
		if (state->MaxConnections)
			state->MaxConnections = n = min(state->MaxConnections, n);
		else
			state->MaxConnections = n;

		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_MaxOutstandingR2T:
		state->MaxOutstandingR2T = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_FirstBurstLength:
		state->FirstBurstLength = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_MaxBurstLength:
		state->MaxBurstLength = n;
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_IFMarker:
	case K_OFMarker:
		/* not (yet) supported */
		if (!sent)
			set_key_n(state, key, 0);
		break;

	case K_IFMarkInt:
	case K_OFMarkInt:
		/* it's a range, and list_num will be 1, so this will reply "Reject" */
		if (!sent)
			set_key_n(state, key, 0);
		break;

	case K_DataPDUInOrder:
	case K_DataSequenceInOrder:
		/* values are don't care */
		if (!sent)
			set_key_n(state, key, n);
		break;

	case K_NotUnderstood:
		/* return "NotUnderstood" */
		set_key_s(state, key, par->val.sval);
		break;

		/*
		 * Declarative keys (no response required)
		 */
	case K_TargetAddress:
		/* ignore for now... */
		break;

	case K_TargetAlias:
		if (conn->login_par->is_present.TargetAlias) {
			copyoutstr(par->val.sval, conn->login_par->TargetAlias,
				ISCSI_STRING_LENGTH - 1, &sz);
			/* do anything with return code?? */
		}
		break;

	case K_TargetPortalGroupTag:
		/* ignore for now... */
		break;

	default:
		DEBOUT(("eval_par: Invalid parameter type %d\n", par->key));
		return ISCSI_STATUS_NEGOTIATION_ERROR;
	}
	return 0;
}

/*****************************************************************************/


/*
 * init_session_parameters:
 *    Initialize session-related negotiation parameters from existing session
 *
 *    Parameter:
 *          sess     The session
 *          state    The negotiation state
 */

STATIC void
init_session_parameters(session_t *sess, negotiation_state_t *state)
{

	state->ErrorRecoveryLevel = sess->ErrorRecoveryLevel;
	state->InitialR2T = sess->InitialR2T;
	state->ImmediateData = sess->ImmediateData;
	state->MaxConnections = sess->MaxConnections;
	state->DefaultTime2Wait = sess->DefaultTime2Wait;
	state->DefaultTime2Retain = sess->DefaultTime2Retain;
	state->MaxBurstLength = sess->MaxBurstLength;
	state->FirstBurstLength = sess->FirstBurstLength;
	state->MaxOutstandingR2T = sess->MaxOutstandingR2T;
}



/*
 * assemble_login_parameters:
 *    Assemble the initial login negotiation parameters.
 *
 *    Parameter:
 *          conn     The connection
 *          ccb      The CCB for the login exchange
 *          pdu      The PDU to use for sending
 *
 *    Returns:    < 0   if more security negotiation is required
 *                0     if this is the last security negotiation block
 *                > 0   (an ISCSI error code) if an error occurred.
 */

int
assemble_login_parameters(connection_t *conn, ccb_t *ccb, pdu_t *pdu)
{
	iscsi_login_parameters_t *par = conn->login_par;
	size_t sz;
	int rc, i, next;
	negotiation_state_t *state;
	negotiation_parameter_t *cpar;

	state = malloc(sizeof(*state), M_TEMP, M_WAITOK | M_ZERO);
	if (state == NULL) {
		DEBOUT(("*** Out of memory in assemble_login_params\n"));
		return ISCSI_STATUS_NO_RESOURCES;
	}
	ccb->temp_data = state;

	if (!iscsi_InitiatorName[0]) {
		DEBOUT(("No InitiatorName\n"));
		return ISCSI_STATUS_PARAMETER_MISSING;
	}
	set_key_s(state, K_InitiatorName, iscsi_InitiatorName);

	if (iscsi_InitiatorAlias[0])
		set_key_s(state, K_InitiatorAlias, iscsi_InitiatorAlias);

	conn->Our_MaxRecvDataSegmentLength =
		(par->is_present.MaxRecvDataSegmentLength)
		? par->MaxRecvDataSegmentLength : DEFAULT_MaxRecvDataSegmentLength;

	/* setup some values for authentication */
	if (par->is_present.password)
		copyinstr(par->password, state->password, MAX_STRING, &sz);
	if (par->is_present.target_password)
		copyinstr(par->target_password, state->target_password,
			MAX_STRING, &sz);
	if (par->is_present.user_name)
		copyinstr(par->user_name, state->user_name, MAX_STRING, &sz);
	else
		strlcpy(state->user_name, iscsi_InitiatorName,
			sizeof(state->user_name));

	next = TRUE;

	set_key_n(state, K_SessionType,
			  par->login_type > ISCSI_LOGINTYPE_DISCOVERY);

	cpar = set_key_n(state, K_AuthMethod, ISCSI_AUTH_None);

	if (cpar != NULL && par->is_present.auth_info &&
		par->auth_info.auth_number > 0) {
		if (par->auth_info.auth_number > ISCSI_AUTH_OPTIONS) {
			DEBOUT(("Auth number too big in asm_login\n"));
			return ISCSI_STATUS_PARAMETER_INVALID;
		}
		cpar->list_num = par->auth_info.auth_number;
		for (i = 0; i < cpar->list_num; i++) {
			cpar->val.nval[i] = par->auth_info.auth_type[i];
			if (par->auth_info.auth_type[i])
				next = FALSE;
		}
	}

	if (par->is_present.TargetName)
		copyinstr(par->TargetName, state->temp_buf, ISCSI_STRING_LENGTH - 1,
				  &sz);
	else {
		state->temp_buf[0] = 0;
		sz = 0;
	}

	if ((!sz || !state->temp_buf[0]) &&
		par->login_type != ISCSI_LOGINTYPE_DISCOVERY) {
		DEBOUT(("No TargetName\n"));
		return ISCSI_STATUS_PARAMETER_MISSING;
	}

	if (state->temp_buf[0]) {
		set_key_s(state, K_TargetName, state->temp_buf);
	}

	if ((rc = complete_pars(state, pdu)) != 0)
		return rc;

	return (next) ? 0 : -1;
}


/*
 * assemble_security_parameters:
 *    Assemble the security negotiation parameters.
 *
 *    Parameter:
 *          conn     The connection
 *          rx_pdu   The received login response PDU
 *          tx_pdu   The transmit PDU
 *
 *    Returns:    < 0   if more security negotiation is required
 *                0     if this is the last security negotiation block
 *                > 0   (an ISCSI error code) if an error occurred.
 */

int
assemble_security_parameters(connection_t *conn, ccb_t *ccb, pdu_t *rx_pdu,
							 pdu_t *tx_pdu)
{
	negotiation_state_t *state = (negotiation_state_t *) ccb->temp_data;
	iscsi_login_parameters_t *par = conn->login_par;
	negotiation_parameter_t rxp, *cpar;
	uint8_t *rxpars;
	int rc, next;
	uint8_t identifier = 0;
	uint8_t *challenge = NULL;
	int challenge_size = 0;
	uint8_t *response = NULL;
	int response_size = 0;

	state->num_pars = 0;
	next = 0;

	rxpars = (uint8_t *) rx_pdu->temp_data;
	if (rxpars == NULL) {
		DEBOUT(("No received parameters!\n"));
		return ISCSI_STATUS_NEGOTIATION_ERROR;
	}
	/* Note: There are always at least 2 extra bytes past temp_data_len */
	rxpars[rx_pdu->temp_data_len] = '\0';
	rxpars[rx_pdu->temp_data_len + 1] = '\0';

	while (*rxpars) {
		if ((rxpars = get_parameter(rxpars, &rxp)) == NULL) {
			DEBOUT(("get_parameter returned error\n"));
			return ISCSI_STATUS_NEGOTIATION_ERROR;
		}

		state->kflags[rxp.key] |= NS_RECEIVED;

		switch (rxp.key) {
		case K_AuthMethod:
			if (state->auth_state != AUTH_INITIAL) {
				DEBOUT(("AuthMethod received, auth_state = %d\n",
						state->auth_state));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}

			/* Note: if the selection is None, we shouldn't be here,
			 * the target should have transited the state to op-neg.
			 */
			if (rxp.val.nval[0] != ISCSI_AUTH_CHAP) {
				DEBOUT(("AuthMethod isn't CHAP (%d)\n", rxp.val.nval[0]));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}

			state->auth_state = AUTH_METHOD_SELECTED;
			state->auth_alg = rxp.val.nval[0];
			break;

		case K_Auth_CHAP_Algorithm:
			if (state->auth_state != AUTH_CHAP_ALG_SENT ||
				rxp.val.nval[0] != 5) {
				DEBOUT(("Bad algorithm, auth_state = %d, alg %d\n",
						state->auth_state, rxp.val.nval[0]));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}
			break;

		case K_Auth_CHAP_Challenge:
			if (state->auth_state != AUTH_CHAP_ALG_SENT || !rxp.list_num) {
				DEBOUT(("Bad Challenge, auth_state = %d, len %d\n",
						state->auth_state, rxp.list_num));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}
			challenge = rxp.val.sval;
			challenge_size = rxp.list_num;
			break;

		case K_Auth_CHAP_Identifier:
			if (state->auth_state != AUTH_CHAP_ALG_SENT) {
				DEBOUT(("Bad ID, auth_state = %d, id %d\n",
						state->auth_state, rxp.val.nval[0]));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}
			identifier = (uint8_t) rxp.val.nval[0];
			break;

		case K_Auth_CHAP_Name:
			if (state->auth_state != AUTH_CHAP_RSP_SENT) {
				DEBOUT(("Bad Name, auth_state = %d, name <%s>\n",
						state->auth_state, rxp.val.sval));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}
			/* what do we do with the name?? */
			break;

		case K_Auth_CHAP_Response:
			if (state->auth_state != AUTH_CHAP_RSP_SENT) {
				DEBOUT(("Bad Response, auth_state = %d, size %d\n",
						state->auth_state, rxp.list_num));
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			}
			response = rxp.val.sval;
			response_size = rxp.list_num;
			if (response_size != CHAP_MD5_SIZE)
				return ISCSI_STATUS_NEGOTIATION_ERROR;
			break;

		default:
			rc = eval_parameter(conn, state, &rxp);
			if (rc)
				return rc;
			break;
		}
	}

	switch (state->auth_state) {
	case AUTH_INITIAL:
		DEBOUT(("Didn't receive Method\n"));
		return ISCSI_STATUS_NEGOTIATION_ERROR;

	case AUTH_METHOD_SELECTED:
		set_key_n(state, K_Auth_CHAP_Algorithm, 5);
		state->auth_state = AUTH_CHAP_ALG_SENT;
		next = -1;
		break;

	case AUTH_CHAP_ALG_SENT:
		if (!RX(state, K_Auth_CHAP_Algorithm) ||
			!RX(state, K_Auth_CHAP_Identifier) ||
			!RX(state, K_Auth_CHAP_Challenge)) {
			DEBOUT(("Didn't receive all parameters\n"));
			return ISCSI_STATUS_NEGOTIATION_ERROR;
		}

		set_key_s(state, K_Auth_CHAP_Name, state->user_name);

		chap_md5_response(state->temp_buf, identifier, state->password,
						  challenge, challenge_size);

		cpar = set_key_s(state, K_Auth_CHAP_Response, state->temp_buf);
		if (cpar != NULL)
			cpar->list_num = CHAP_MD5_SIZE;

		if (par->auth_info.mutual_auth) {
			if (!state->target_password[0]) {
				DEBOUT(("No target password with mutual authentication!\n"));
				return ISCSI_STATUS_PARAMETER_MISSING;
			}

			cprng_strong(kern_cprng,
				     &state->temp_buf[CHAP_MD5_SIZE],
				     CHAP_CHALLENGE_LEN + 1, 0);
			set_key_n(state, K_Auth_CHAP_Identifier,
					  state->temp_buf[CHAP_MD5_SIZE]);
			cpar = set_key_s(state, K_Auth_CHAP_Challenge,
							 &state->temp_buf[CHAP_MD5_SIZE + 1]);
			if (cpar != NULL)
				cpar->list_num = CHAP_CHALLENGE_LEN;
			next = -1;
		}
		state->auth_state = AUTH_CHAP_RSP_SENT;
		break;

	case AUTH_CHAP_RSP_SENT:
		/* we can only be here for mutual authentication */
		if (!par->auth_info.mutual_auth || response == NULL) {
			DEBOUT(("Mutual authentication not requested\n"));
			return ISCSI_STATUS_NEGOTIATION_ERROR;
		}

		chap_md5_response(state->temp_buf,
				state->temp_buf[CHAP_MD5_SIZE],
				state->password,
				&state->temp_buf[CHAP_MD5_SIZE + 1],
				CHAP_CHALLENGE_LEN);

		if (memcmp(state->temp_buf, response, response_size)) {
			DEBOUT(("Mutual authentication mismatch\n"));
			return ISCSI_STATUS_AUTHENTICATION_FAILED;
		}
		break;

	default:
		break;
	}

	complete_pars(state, tx_pdu);

	return next;
}


/*
 * set_first_opnegs:
 *    Set the operational negotiation parameters we want to negotiate in
 *    the first login request in op_neg phase.
 *
 *    Parameter:
 *          conn     The connection
 *          state    Negotiation state
 */

STATIC void
set_first_opnegs(connection_t *conn, negotiation_state_t *state)
{
	iscsi_login_parameters_t *lpar = conn->login_par;
	negotiation_parameter_t *cpar;

    /* Digests - suggest None,CRC32C unless the user forces a value */
	cpar = set_key_n(state, K_HeaderDigest,
					 (lpar->is_present.HeaderDigest) ? lpar->HeaderDigest : 0);
	if (cpar != NULL && !lpar->is_present.HeaderDigest) {
		cpar->list_num = 2;
		cpar->val.nval[1] = 1;
	}

	cpar = set_key_n(state, K_DataDigest, (lpar->is_present.DataDigest)
		? lpar->DataDigest : 0);
	if (cpar != NULL && !lpar->is_present.DataDigest) {
		cpar->list_num = 2;
		cpar->val.nval[1] = 1;
	}

	set_key_n(state, K_MaxRecvDataSegmentLength,
		conn->Our_MaxRecvDataSegmentLength);
	/* This is direction-specific, we may have a different default */
	state->MaxRecvDataSegmentLength =
		entries[K_MaxRecvDataSegmentLength].defval;

	/* First connection only */
	if (!conn->session->TSIH) {
		state->ErrorRecoveryLevel =
			(lpar->is_present.ErrorRecoveryLevel) ? lpar->ErrorRecoveryLevel
												  : 2;
		/*
		   Negotiate InitialR2T to FALSE and ImmediateData to TRUE, should
		   be slightly more efficient than the default InitialR2T=TRUE.
		 */
		state->InitialR2T = FALSE;
		state->ImmediateData = TRUE;

		/* We don't really care about this, so don't negotiate by default */
		state->MaxBurstLength = entries[K_MaxBurstLength].defval;
		state->FirstBurstLength = entries[K_FirstBurstLength].defval;
		state->MaxOutstandingR2T = entries[K_MaxOutstandingR2T].defval;

		set_key_n(state, K_ErrorRecoveryLevel, state->ErrorRecoveryLevel);
		set_key_n(state, K_InitialR2T, state->InitialR2T);
		set_key_n(state, K_ImmediateData, state->ImmediateData);

		if (lpar->is_present.MaxConnections) {
			state->MaxConnections = lpar->MaxConnections;
			set_key_n(state, K_MaxConnections, lpar->MaxConnections);
		}

		if (lpar->is_present.DefaultTime2Wait)
			set_key_n(state, K_DefaultTime2Wait, lpar->DefaultTime2Wait);
		else
			state->DefaultTime2Wait = entries[K_DefaultTime2Wait].defval;

		if (lpar->is_present.DefaultTime2Retain)
			set_key_n(state, K_DefaultTime2Retain, lpar->DefaultTime2Retain);
		else
			state->DefaultTime2Retain = entries[K_DefaultTime2Retain].defval;
	} else
		init_session_parameters(conn->session, state);

	DEBC(conn, 10, ("SetFirstOpnegs: recover=%d, MRDSL=%d\n",
		conn->recover, state->MaxRecvDataSegmentLength));
}


/*
 * assemble_negotiation_parameters:
 *    Assemble any negotiation parameters requested by the other side.
 *
 *    Parameter:
 *          conn     The connection
 *          ccb      The login ccb
 *          rx_pdu   The received login response PDU
 *          tx_pdu   The transmit PDU
 *
 *    Returns:    0     On success
 *                > 0   (an ISCSI error code) if an error occurred.
 */

int
assemble_negotiation_parameters(connection_t *conn, ccb_t *ccb, pdu_t *rx_pdu,
							    pdu_t *tx_pdu)
{
	negotiation_state_t *state = (negotiation_state_t *) ccb->temp_data;
	negotiation_parameter_t rxp;
	uint8_t *rxpars;
	int rc;

	state->num_pars = 0;

	DEBC(conn, 10, ("AsmNegParams: connState=%d, MRDSL=%d\n",
		conn->state, state->MaxRecvDataSegmentLength));

	if (conn->state == ST_SEC_NEG) {
		conn->state = ST_OP_NEG;
		set_first_opnegs(conn, state);
	}

	rxpars = (uint8_t *) rx_pdu->temp_data;
	if (rxpars != NULL) {
		/* Note: There are always at least 2 extra bytes past temp_data_len */
		rxpars[rx_pdu->temp_data_len] = '\0';
		rxpars[rx_pdu->temp_data_len + 1] = '\0';

		while (*rxpars) {
			if ((rxpars = get_parameter(rxpars, &rxp)) == NULL)
				return ISCSI_STATUS_NEGOTIATION_ERROR;

			rc = eval_parameter(conn, state, &rxp);
			if (rc)
				return rc;
		}
	}

	if (tx_pdu == NULL)
		return 0;

	complete_pars(state, tx_pdu);

	return 0;
}

/*
 * init_text_parameters:
 *    Initialize text negotiation.
 *
 *    Parameter:
 *          conn     The connection
 *          tx_pdu   The transmit PDU
 *
 *    Returns:    0     On success
 *                > 0   (an ISCSI error code) if an error occurred.
 */

int
init_text_parameters(connection_t *conn, ccb_t *ccb)
{
	negotiation_state_t *state;

	state = malloc(sizeof(*state), M_TEMP, M_WAITOK | M_ZERO);
	if (state == NULL) {
		DEBOUT(("*** Out of memory in init_text_params\n"));
		return ISCSI_STATUS_NO_RESOURCES;
	}
	ccb->temp_data = state;

	state->HeaderDigest = conn->HeaderDigest;
	state->DataDigest = conn->DataDigest;
	state->MaxRecvDataSegmentLength = conn->MaxRecvDataSegmentLength;
	init_session_parameters(conn->session, state);

	return 0;
}


/*
 * assemble_send_targets:
 *    Assemble send targets request
 *
 *    Parameter:
 *          pdu      The transmit PDU
 *          val      The SendTargets key value
 *
 *    Returns:    0     On success
 *                > 0   (an ISCSI error code) if an error occurred.
 */

int
assemble_send_targets(pdu_t *pdu, uint8_t *val)
{
	negotiation_parameter_t par;
	uint8_t *buf;
	int len;

	par.key = K_SendTargets;
	par.list_num = 1;
	par.val.sval = val;

	len = parameter_size(&par);

	if ((buf = malloc(len, M_TEMP, M_WAITOK)) == NULL) {
		DEBOUT(("*** Out of memory in assemble_send_targets\n"));
		return ISCSI_STATUS_NO_RESOURCES;
	}
	pdu->temp_data = buf;
	pdu->temp_data_len = len;

	if (put_parameter(buf, len, &par) == 0)
		return ISCSI_STATUS_PARAMETER_INVALID;

	return 0;
}


/*
 * set_negotiated_parameters:
 *    Copy the negotiated parameters into the connection and session structure.
 *
 *    Parameter:
 *          ccb      The ccb containing the state information
 */

void
set_negotiated_parameters(ccb_t *ccb)
{
	negotiation_state_t *state = (negotiation_state_t *) ccb->temp_data;
	connection_t *conn = ccb->connection;
	session_t *sess = ccb->session;

	conn->HeaderDigest = state->HeaderDigest;
	conn->DataDigest = state->DataDigest;
	sess->ErrorRecoveryLevel = state->ErrorRecoveryLevel;
	sess->InitialR2T = state->InitialR2T;
	sess->ImmediateData = state->ImmediateData;
	conn->MaxRecvDataSegmentLength = state->MaxRecvDataSegmentLength;
	sess->MaxConnections = state->MaxConnections;
	sess->DefaultTime2Wait = conn->Time2Wait = state->DefaultTime2Wait;
	sess->DefaultTime2Retain = conn->Time2Retain =
		state->DefaultTime2Retain;

	/* set idle connection timeout to half the Time2Retain window so we */
	/* don't miss it, unless Time2Retain is ridiculously small. */
	conn->idle_timeout_val = (conn->Time2Retain >= 10) ?
		(conn->Time2Retain / 2) * hz : CONNECTION_IDLE_TIMEOUT;

	sess->MaxBurstLength = state->MaxBurstLength;
	sess->FirstBurstLength = state->FirstBurstLength;
	sess->MaxOutstandingR2T = state->MaxOutstandingR2T;

	DEBC(conn, 10,("SetNegPar: MRDSL=%d, MBL=%d, FBL=%d, IR2T=%d, ImD=%d\n",
		state->MaxRecvDataSegmentLength, state->MaxBurstLength,
		state->FirstBurstLength, state->InitialR2T,
		state->ImmediateData));

	conn->max_transfer = min(sess->MaxBurstLength, conn->MaxRecvDataSegmentLength);

	conn->max_firstimmed = (!sess->ImmediateData) ? 0 :
				min(sess->FirstBurstLength, conn->max_transfer);

	conn->max_firstdata = (sess->InitialR2T || sess->FirstBurstLength < conn->max_firstimmed) ? 0 :
				min(sess->FirstBurstLength - conn->max_firstimmed, conn->max_transfer);

}
