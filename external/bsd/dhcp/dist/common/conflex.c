/*	$NetBSD: conflex.c,v 1.4 2014/07/12 12:09:37 spz Exp $	*/
/* conflex.c

   Lexical scanner for dhcpd config file... */

/*
 * Copyright (c) 2004-2014 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-2003 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *   Internet Systems Consortium, Inc.
 *   950 Charter Street
 *   Redwood City, CA 94063
 *   <info@isc.org>
 *   https://www.isc.org/
 *
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: conflex.c,v 1.4 2014/07/12 12:09:37 spz Exp $");

#include "dhcpd.h"
#include <ctype.h>

static int get_char (struct parse *);
static void unget_char(struct parse *, int);
static void skip_to_eol (struct parse *);
static enum dhcp_token read_whitespace(int c, struct parse *cfile);
static enum dhcp_token read_string (struct parse *);
static enum dhcp_token read_number (int, struct parse *);
static enum dhcp_token read_num_or_name (int, struct parse *);
static enum dhcp_token intern (char *, enum dhcp_token);

isc_result_t new_parse (cfile, file, inbuf, buflen, name, eolp)
	struct parse **cfile;
	int file;
	char *inbuf;
	unsigned buflen;
	const char *name;
	int eolp;
{
	isc_result_t status = ISC_R_SUCCESS;
	struct parse *tmp;

	tmp = dmalloc(sizeof(struct parse), MDL);
	if (tmp == NULL) {
		return (ISC_R_NOMEMORY);
	}

	/*
	 * We don't need to initialize things to zero here, since 
	 * dmalloc() returns memory that is set to zero.
	 */
	tmp->tlname = name;
	tmp->lpos = tmp -> line = 1;
	tmp->cur_line = tmp->line1;
	tmp->prev_line = tmp->line2;
	tmp->token_line = tmp->cur_line;
	tmp->cur_line[0] = tmp->prev_line[0] = 0;
	tmp->file = file;
	tmp->eol_token = eolp;

	if (inbuf != NULL) {
		tmp->inbuf = inbuf;
		tmp->buflen = buflen;
		tmp->bufsiz = 0;
	} else {
		struct stat sb;

		if (fstat(file, &sb) < 0) {
			status = ISC_R_IOERROR;
			goto cleanup;
		}

		if (sb.st_size == 0)
			goto cleanup;

		tmp->bufsiz = tmp->buflen = (size_t) sb.st_size;
		tmp->inbuf = mmap(NULL, tmp->bufsiz, PROT_READ, MAP_SHARED,
				  file, 0);

		if (tmp->inbuf == MAP_FAILED) {
			status = ISC_R_IOERROR;
			goto cleanup;
		}
	}

	*cfile = tmp;
	return (ISC_R_SUCCESS);

cleanup:
	dfree(tmp, MDL);
	return (status);
}

isc_result_t end_parse (cfile)
	struct parse **cfile;
{
	/* "Memory" config files have no file. */
	if ((*cfile)->file != -1) {
		munmap((*cfile)->inbuf, (*cfile)->bufsiz);
		close((*cfile)->file);
	}

	if ((*cfile)->saved_state != NULL) {
		dfree((*cfile)->saved_state, MDL);
	}
		
	dfree(*cfile, MDL);
	*cfile = NULL;
	return ISC_R_SUCCESS;
}

/*
 * Save the current state of the parser.
 *
 * Only one state may be saved. Any previous saved state is
 * lost.
 */
isc_result_t
save_parse_state(struct parse *cfile) {
	/*
	 * Free any previous saved state.
	 */
	if (cfile->saved_state != NULL) {
		dfree(cfile->saved_state, MDL);
	}

	/*
	 * Save our current state.
	 */
	cfile->saved_state = dmalloc(sizeof(struct parse), MDL);
	if (cfile->saved_state == NULL) {
		return ISC_R_NOMEMORY;
	}
	memcpy(cfile->saved_state, cfile, sizeof(*cfile));
	return ISC_R_SUCCESS;
}

/*
 * Return the parser to the previous saved state.
 *
 * You must call save_parse_state() before calling 
 * restore_parse_state(), but you can call restore_parse_state() any
 * number of times after that.
 */
isc_result_t
restore_parse_state(struct parse *cfile) {
	struct parse *saved_state;

	if (cfile->saved_state == NULL) {
		return DHCP_R_NOTYET;
	}

	saved_state = cfile->saved_state;
	memcpy(cfile, saved_state, sizeof(*cfile));
	cfile->saved_state = saved_state;
	return ISC_R_SUCCESS;
}

static int get_char (cfile)
	struct parse *cfile;
{
	/* My kingdom for WITH... */
	int c;

	if (cfile->bufix == cfile->buflen) {
#if !defined(LDAP_CONFIGURATION)
		c = EOF;
#else /* defined(LDAP_CONFIGURATION) */
		if (cfile->read_function != NULL)
			c = cfile->read_function(cfile);
		else
			c = EOF;
#endif
	} else {
		c = cfile->inbuf [cfile->bufix];
		cfile->bufix++;
	}

	if (!cfile->ugflag) {
		if (c == EOL) {
			if (cfile->cur_line == cfile->line1) {	
				cfile->cur_line = cfile->line2;
				cfile->prev_line = cfile->line1;
			} else {
				cfile->cur_line = cfile->line1;
				cfile->prev_line = cfile->line2;
			}
			cfile->line++;
			cfile->lpos = 1;
			cfile->cur_line [0] = 0;
		} else if (c != EOF) {
			if (cfile->lpos <= 80) {
				cfile->cur_line [cfile->lpos - 1] = c;
				cfile->cur_line [cfile->lpos] = 0;
			}
			cfile->lpos++;
		}
	} else
		cfile->ugflag = 0;
	return c;		
}

/*
 * Return a character to our input buffer.
 */
static void
unget_char(struct parse *cfile, int c) {
	if (c != EOF) {
		cfile->bufix--;
		cfile->ugflag = 1;	/* do not put characters into
					   our error buffer on the next
					   call to get_char() */
	}
}

/*
 * GENERAL NOTE ABOUT TOKENS
 *
 * We normally only want non-whitespace tokens. There are some 
 * circumstances where we *do* want to see whitespace (for example
 * when parsing IPv6 addresses).
 *
 * Generally we use the next_token() function to read tokens. This 
 * in turn calls get_next_token, which does *not* return tokens for
 * whitespace. Rather, it skips these.
 *
 * When we need to see whitespace, we us next_raw_token(), which also
 * returns the WHITESPACE token.
 *
 * The peek_token() and peek_raw_token() functions work as expected.
 *
 * Warning: if you invoke peek_token(), then if there is a whitespace
 * token, it will be lost, and subsequent use of next_raw_token() or
 * peek_raw_token() will NOT see it.
 */

static enum dhcp_token
get_raw_token(struct parse *cfile) {
	int c;
	enum dhcp_token ttok;
	static char tb [2];
	int l, p;

	do {
		l = cfile -> line;
		p = cfile -> lpos;

		c = get_char (cfile);
		if (!((c == '\n') && cfile->eol_token) && 
		    isascii(c) && isspace(c)) {
		    	ttok = read_whitespace(c, cfile);
			break;
		}
		if (c == '#') {
			skip_to_eol (cfile);
			continue;
		}
		if (c == '"') {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			ttok = read_string (cfile);
			break;
		}
		if ((isascii (c) && isdigit (c)) || c == '-') {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			ttok = read_number (c, cfile);
			break;
		} else if (isascii (c) && isalpha (c)) {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			ttok = read_num_or_name (c, cfile);
			break;
		} else if (c == EOF) {
			ttok = END_OF_FILE;
			cfile -> tlen = 0;
			break;
		} else {
			cfile -> lexline = l;
			cfile -> lexchar = p;
			tb [0] = c;
			tb [1] = 0;
			cfile -> tval = tb;
			cfile -> tlen = 1;
			ttok = c;
			break;
		}
	} while (1);
	return ttok;
}

/*
 * The get_next_token() function consumes the next token and
 * returns it to the caller.
 *
 * Since the code is almost the same for "normal" and "raw" 
 * input, we pass a flag to alter the way it works.
 */

static enum dhcp_token 
get_next_token(const char **rval, unsigned *rlen, 
	       struct parse *cfile, isc_boolean_t raw) {
	int rv;

	if (cfile -> token) {
		if (cfile -> lexline != cfile -> tline)
			cfile -> token_line = cfile -> cur_line;
		cfile -> lexchar = cfile -> tlpos;
		cfile -> lexline = cfile -> tline;
		rv = cfile -> token;
		cfile -> token = 0;
	} else {
		rv = get_raw_token(cfile);
		cfile -> token_line = cfile -> cur_line;
	}

	if (!raw) {
		while (rv == WHITESPACE) {
			rv = get_raw_token(cfile);
			cfile->token_line = cfile->cur_line;
		}
	}
	
	if (rval)
		*rval = cfile -> tval;
	if (rlen)
		*rlen = cfile -> tlen;
#ifdef DEBUG_TOKENS
	fprintf (stderr, "%s:%d ", cfile -> tval, rv);
#endif
	return rv;
}


/*
 * Get the next token from cfile and return it.
 *
 * If rval is non-NULL, set the pointer it contains to 
 * the contents of the token.
 *
 * If rlen is non-NULL, set the integer it contains to 
 * the length of the token.
 */

enum dhcp_token
next_token(const char **rval, unsigned *rlen, struct parse *cfile) {
	return get_next_token(rval, rlen, cfile, ISC_FALSE);
}


/*
 * The same as the next_token() function above, but will return space
 * as the WHITESPACE token.
 */

enum dhcp_token
next_raw_token(const char **rval, unsigned *rlen, struct parse *cfile) {
	return get_next_token(rval, rlen, cfile, ISC_TRUE);
}


/*
 * The do_peek_token() function checks the next token without
 * consuming it, and returns it to the caller.
 *
 * Since the code is almost the same for "normal" and "raw" 
 * input, we pass a flag to alter the way it works. (See the 
 * warning in the GENERAL NOTES ABOUT TOKENS above though.)
 */

static enum dhcp_token
do_peek_token(const char **rval, unsigned int *rlen,
	      struct parse *cfile, isc_boolean_t raw) {
	int x;

	if (!cfile->token || (!raw && (cfile->token == WHITESPACE))) {
		cfile -> tlpos = cfile -> lexchar;
		cfile -> tline = cfile -> lexline;

		do {
			cfile->token = get_raw_token(cfile);
		} while (!raw && (cfile->token == WHITESPACE));

		if (cfile -> lexline != cfile -> tline)
			cfile -> token_line = cfile -> prev_line;

		x = cfile -> lexchar;
		cfile -> lexchar = cfile -> tlpos;
		cfile -> tlpos = x;

		x = cfile -> lexline;
		cfile -> lexline = cfile -> tline;
		cfile -> tline = x;
	}
	if (rval)
		*rval = cfile -> tval;
	if (rlen)
		*rlen = cfile -> tlen;
#ifdef DEBUG_TOKENS
	fprintf (stderr, "(%s:%d) ", cfile -> tval, cfile -> token);
#endif
	return cfile -> token;
}


/*
 * Get the next token from cfile and return it, leaving it for a 
 * subsequent call to next_token().
 *
 * Note that it WILL consume whitespace tokens.
 *
 * If rval is non-NULL, set the pointer it contains to 
 * the contents of the token.
 *
 * If rlen is non-NULL, set the integer it contains to 
 * the length of the token.
 */

enum dhcp_token
peek_token(const char **rval, unsigned *rlen, struct parse *cfile) {
	return do_peek_token(rval, rlen, cfile, ISC_FALSE);
}


/*
 * The same as the peek_token() function above, but will return space
 * as the WHITESPACE token.
 */

enum dhcp_token
peek_raw_token(const char **rval, unsigned *rlen, struct parse *cfile) {
	return do_peek_token(rval, rlen, cfile, ISC_TRUE);
}

static void skip_to_eol (cfile)
	struct parse *cfile;
{
	int c;
	do {
		c = get_char (cfile);
		if (c == EOF)
			return;
		if (c == EOL) {
			return;
		}
	} while (1);
}

static enum dhcp_token
read_whitespace(int c, struct parse *cfile) {
	int ofs;

	/*
	 * Read as much whitespace as we have available.
	 */
	ofs = 0;
	do {
		if (ofs >= sizeof(cfile->tokbuf)) {
			/*
			 * As the file includes a huge amount of whitespace,
			 * it's probably broken.
			 * Print out a warning and bail out.
			 */
			parse_warn(cfile,
				   "whitespace too long, buffer overflow.");
			log_fatal("Exiting");
		}
		cfile->tokbuf[ofs++] = c;
		c = get_char(cfile);
	} while (!((c == '\n') && cfile->eol_token) && 
		 isascii(c) && isspace(c));

	/*
	 * Put the last (non-whitespace) character back.
	 */
	unget_char(cfile, c);

	/*
	 * Return our token.
	 */
	cfile->tokbuf[ofs] = '\0';
	cfile->tlen = ofs;
	cfile->tval = cfile->tokbuf;
	return WHITESPACE;
}

static enum dhcp_token read_string (cfile)
	struct parse *cfile;
{
	int i;
	int bs = 0;
	int c;
	int value = 0;
	int hex = 0;

	for (i = 0; i < sizeof cfile -> tokbuf; i++) {
	      again:
		c = get_char (cfile);
		if (c == EOF) {
			parse_warn (cfile, "eof in string constant");
			break;
		}
		if (bs == 1) {
			switch (c) {
			      case 't':
				cfile -> tokbuf [i] = '\t';
				break;
			      case 'r':
				cfile -> tokbuf [i] = '\r';
				break;
			      case 'n':
				cfile -> tokbuf [i] = '\n';
				break;
			      case 'b':
				cfile -> tokbuf [i] = '\b';
				break;
			      case '0':
			      case '1':
			      case '2':
			      case '3':
				hex = 0;
				value = c - '0';
				++bs;
				goto again;
			      case 'x':
				hex = 1;
				value = 0;
				++bs;
				goto again;
			      default:
				cfile -> tokbuf [i] = c;
				break;
			}
			bs = 0;
		} else if (bs > 1) {
			if (hex) {
				if (c >= '0' && c <= '9') {
					value = value * 16 + (c - '0');
				} else if (c >= 'a' && c <= 'f') {
					value = value * 16 + (c - 'a' + 10);
				} else if (c >= 'A' && c <= 'F') {
					value = value * 16 + (c - 'A' + 10);
				} else {
					parse_warn (cfile,
						    "invalid hex digit: %x",
						    c);
					bs = 0;
					continue;
				}
				if (++bs == 4) {
					cfile -> tokbuf [i] = value;
					bs = 0;
				} else
					goto again;
			} else {
				if (c >= '0' && c <= '7') {
					value = value * 8 + (c - '0');
				} else {
				    if (value != 0) {
					parse_warn (cfile,
						    "invalid octal digit %x",
						    c);
					continue;
				    } else
					cfile -> tokbuf [i] = 0;
				    bs = 0;
				}
				if (++bs == 4) {
					cfile -> tokbuf [i] = value;
					bs = 0;
				} else
					goto again;
			}
		} else if (c == '\\') {
			bs = 1;
			goto again;
		} else if (c == '"')
			break;
		else
			cfile -> tokbuf [i] = c;
	}
	/* Normally, I'd feel guilty about this, but we're talking about
	   strings that'll fit in a DHCP packet here... */
	if (i == sizeof cfile -> tokbuf) {
		parse_warn (cfile,
			    "string constant larger than internal buffer");
		--i;
	}
	cfile -> tokbuf [i] = 0;
	cfile -> tlen = i;
	cfile -> tval = cfile -> tokbuf;
	return STRING;
}

static enum dhcp_token read_number (c, cfile)
	int c;
	struct parse *cfile;
{
	int i = 0;
	int token = NUMBER;

	cfile -> tokbuf [i++] = c;
	for (; i < sizeof cfile -> tokbuf; i++) {
		c = get_char (cfile);

		/* Promote NUMBER -> NUMBER_OR_NAME -> NAME, never demote.
		 * Except in the case of '0x' syntax hex, which gets called
		 * a NAME at '0x', and returned to NUMBER_OR_NAME once it's
		 * verified to be at least 0xf or less.
		 */
		switch(isascii(c) ? token : BREAK) {
		    case NUMBER:
			if(isdigit(c))
				break;
			/* FALLTHROUGH */
		    case NUMBER_OR_NAME:
			if(isxdigit(c)) {
				token = NUMBER_OR_NAME;
				break;
			}
			/* FALLTHROUGH */
		    case NAME:
			if((i == 2) && isxdigit(c) &&
				(cfile->tokbuf[0] == '0') &&
				((cfile->tokbuf[1] == 'x') ||
				 (cfile->tokbuf[1] == 'X'))) {
				token = NUMBER_OR_NAME;
				break;
			} else if(((c == '-') || (c == '_') || isalnum(c))) {
				token = NAME;
				break;
			}
			/* FALLTHROUGH */
		    case BREAK:
			/* At this point c is either EOF or part of the next
			 * token.  If not EOF, rewind the file one byte so
			 * the next token is read from there.
			 */
			unget_char(cfile, c);
			goto end_read;

		    default:
			log_fatal("read_number():%s:%d: impossible case", MDL);
		}

		cfile -> tokbuf [i] = c;
	}

	if (i == sizeof cfile -> tokbuf) {
		parse_warn (cfile,
			    "numeric token larger than internal buffer");
		--i;
	}

  end_read:
	cfile -> tokbuf [i] = 0;
	cfile -> tlen = i;
	cfile -> tval = cfile -> tokbuf;

	/*
	 * If this entire token from start to finish was "-", such as
	 * the middle parameter in "42 - 7", return just the MINUS token.
	 */
	if ((i == 1) && (cfile->tokbuf[i] == '-'))
		return MINUS;
	else
		return token;
}

static enum dhcp_token read_num_or_name (c, cfile)
	int c;
	struct parse *cfile;
{
	int i = 0;
	enum dhcp_token rv = NUMBER_OR_NAME;
	cfile -> tokbuf [i++] = c;
	for (; i < sizeof cfile -> tokbuf; i++) {
		c = get_char (cfile);
		if (!isascii (c) ||
		    (c != '-' && c != '_' && !isalnum (c))) {
		    	unget_char(cfile, c);
			break;
		}
		if (!isxdigit (c))
			rv = NAME;
		cfile -> tokbuf [i] = c;
	}
	if (i == sizeof cfile -> tokbuf) {
		parse_warn (cfile, "token larger than internal buffer");
		--i;
	}
	cfile -> tokbuf [i] = 0;
	cfile -> tlen = i;
	cfile -> tval = cfile -> tokbuf;
	return intern(cfile->tval, rv);
}

static enum dhcp_token
intern(char *atom, enum dhcp_token dfv) {
	if (!isascii(atom[0]))
		return dfv;

	switch (tolower((unsigned char)atom[0])) {
	      case '-':
		if (atom [1] == 0)
			return MINUS;
		break;

	      case 'a':
		if (!strcasecmp(atom + 1, "bandoned"))
			return TOKEN_ABANDONED;
		if (!strcasecmp(atom + 1, "ctive"))
			return TOKEN_ACTIVE;
		if (!strncasecmp(atom + 1, "dd", 2)) {
			if (atom[3] == '\0')
				return TOKEN_ADD;
			else if (!strcasecmp(atom + 3, "ress"))
				return ADDRESS;
			break;
		}
		if (!strcasecmp(atom + 1, "fter"))
			return AFTER;
		if (isascii(atom[1]) &&
		    (tolower((unsigned char)atom[1]) == 'l')) {
			if (!strcasecmp(atom + 2, "gorithm"))
				return ALGORITHM;
			if (!strcasecmp(atom + 2, "ias"))
				return ALIAS;
			if (isascii(atom[2]) &&
			    (tolower((unsigned char)atom[2]) == 'l')) {
				if (atom[3] == '\0')
					return ALL;
				else if (!strcasecmp(atom + 3, "ow"))
					return ALLOW;
				break;
			}
			if (!strcasecmp(atom + 2, "so"))
				return TOKEN_ALSO;
			break;
		}
		if (isascii(atom[1]) &&
		    (tolower((unsigned char)atom[1]) == 'n')) {
			if (!strcasecmp(atom + 2, "d"))
				return AND;
			if (!strcasecmp(atom + 2, "ycast-mac"))
				return ANYCAST_MAC;
			break;
		}
		if (!strcasecmp(atom + 1, "ppend"))
			return APPEND;
		if (!strcasecmp(atom + 1, "rray"))
			return ARRAY;
		if (isascii(atom[1]) &&
		    (tolower((unsigned char)atom[1]) == 't')) {
			if (atom[2] == '\0')
				return AT;
			if (!strcasecmp(atom + 2, "sfp"))
				return ATSFP;
			break;
		}
		if (!strncasecmp(atom + 1, "ut", 2)) {
			if (isascii(atom[3]) &&
			    (tolower((unsigned char)atom[3]) == 'h')) {
				if (!strncasecmp(atom + 4, "enticat", 7)) {
					if (!strcasecmp(atom + 11, "ed"))
						return AUTHENTICATED;
					if (!strcasecmp(atom + 11, "ion"))
						return AUTHENTICATION;
					break;
				}
				if (!strcasecmp(atom + 4, "oritative"))
					return AUTHORITATIVE;
				break;
			}
			if (!strcasecmp(atom + 3, "o-partner-down"))
				return AUTO_PARTNER_DOWN;
			break;
		}
		break;
	      case 'b':
		if (!strcasecmp (atom + 1, "ackup"))
			return TOKEN_BACKUP;
		if (!strcasecmp (atom + 1, "ootp"))
			return TOKEN_BOOTP;
		if (!strcasecmp (atom + 1, "inding"))
			return BINDING;
		if (!strcasecmp (atom + 1, "inary-to-ascii"))
			return BINARY_TO_ASCII;
		if (!strcasecmp (atom + 1, "ackoff-cutoff"))
			return BACKOFF_CUTOFF;
		if (!strcasecmp (atom + 1, "ooting"))
			return BOOTING;
		if (!strcasecmp (atom + 1, "oot-unknown-clients"))
			return BOOT_UNKNOWN_CLIENTS;
		if (!strcasecmp (atom + 1, "reak"))
			return BREAK;
		if (!strcasecmp (atom + 1, "illing"))
			return BILLING;
		if (!strcasecmp (atom + 1, "oolean"))
			return BOOLEAN;
		if (!strcasecmp (atom + 1, "alance"))
			return BALANCE;
		if (!strcasecmp (atom + 1, "ound"))
			return BOUND;
		break;
	      case 'c':
		if (!strcasecmp(atom + 1, "ase"))
			return CASE;
		if (!strcasecmp(atom + 1, "heck"))
			return CHECK;
		if (!strcasecmp(atom + 1, "iaddr"))
			return CIADDR;
		if (isascii(atom[1]) &&
		    tolower((unsigned char)atom[1]) == 'l') {
			if (!strcasecmp(atom + 2, "ass"))
				return CLASS;
			if (!strncasecmp(atom + 2, "ient", 4)) {
				if (!strcasecmp(atom + 6, "s"))
					return CLIENTS;
				if (atom[6] == '-') {
					if (!strcasecmp(atom + 7, "hostname"))
						return CLIENT_HOSTNAME;
					if (!strcasecmp(atom + 7, "identifier"))
						return CLIENT_IDENTIFIER;
					if (!strcasecmp(atom + 7, "state"))
						return CLIENT_STATE;
					if (!strcasecmp(atom + 7, "updates"))
						return CLIENT_UPDATES;
					break;
				}
				break;
			}
			if (!strcasecmp(atom + 2, "ose"))
				return TOKEN_CLOSE;
			if (!strcasecmp(atom + 2, "tt"))
				return CLTT;
			break;
		}
		if (isascii(atom[1]) &&
		    tolower((unsigned char)atom[1]) == 'o') {
			if (!strcasecmp(atom + 2, "de"))
				return CODE;
			if (isascii(atom[2]) &&
			    tolower((unsigned char)atom[2]) == 'm') {
				if (!strcasecmp(atom + 3, "mit"))
					return COMMIT;
				if (!strcasecmp(atom + 3,
						"munications-interrupted"))
					return COMMUNICATIONS_INTERRUPTED;
				if (!strcasecmp(atom + 3, "pressed"))
					return COMPRESSED;
				break;
			}
			if (isascii(atom[2]) &&
			    tolower((unsigned char)atom[2]) == 'n') {
				if (!strcasecmp(atom + 3, "cat"))
					return CONCAT;
				if (!strcasecmp(atom + 3, "fig-option"))
					return CONFIG_OPTION;
				if (!strcasecmp(atom + 3, "flict-done"))
					return CONFLICT_DONE;
				if (!strcasecmp(atom + 3, "nect"))
					return CONNECT;
				break;
			}
			break;
		}
		if (!strcasecmp(atom + 1, "reate"))
			return TOKEN_CREATE;
		break;
	      case 'd':
		if (!strcasecmp(atom + 1, "b-time-format"))
			return DB_TIME_FORMAT;
		if (!strcasecmp (atom + 1, "omain"))
			return DOMAIN;
		if (!strncasecmp (atom + 1, "omain-", 6)) {
			if (!strcasecmp(atom + 7, "name"))
				return DOMAIN_NAME;
			if (!strcasecmp(atom + 7, "list"))
				return DOMAIN_LIST;
		}
		if (!strcasecmp (atom + 1, "o-forward-update"))
			return DO_FORWARD_UPDATE;
		if (!strcasecmp (atom + 1, "ebug"))
			return TOKEN_DEBUG;
		if (!strcasecmp (atom + 1, "eny"))
			return DENY;
		if (!strcasecmp (atom + 1, "eleted"))
			return TOKEN_DELETED;
		if (!strcasecmp (atom + 1, "elete"))
			return TOKEN_DELETE;
		if (!strncasecmp (atom + 1, "efault", 6)) {
			if (!atom [7])
				return DEFAULT;
			if (!strcasecmp(atom + 7, "-duid"))
				return DEFAULT_DUID;
			if (!strcasecmp (atom + 7, "-lease-time"))
				return DEFAULT_LEASE_TIME;
			break;
		}
		if (!strncasecmp (atom + 1, "ynamic", 6)) {
			if (!atom [7])
				return DYNAMIC;
			if (!strncasecmp (atom + 7, "-bootp", 6)) {
				if (!atom [13])
					return DYNAMIC_BOOTP;
				if (!strcasecmp (atom + 13, "-lease-cutoff"))
					return DYNAMIC_BOOTP_LEASE_CUTOFF;
				if (!strcasecmp (atom + 13, "-lease-length"))
					return DYNAMIC_BOOTP_LEASE_LENGTH;
				break;
			}
		}
		if (!strcasecmp (atom + 1, "uplicates"))
			return DUPLICATES;
		if (!strcasecmp (atom + 1, "eclines"))
			return DECLINES;
		if (!strncasecmp (atom + 1, "efine", 5)) {
			if (!strcasecmp (atom + 6, "d"))
				return DEFINED;
			if (!atom [6])
				return DEFINE;
		}
		break;
	      case 'e':
		if (isascii (atom [1]) && 
		    tolower((unsigned char)atom[1]) == 'x') {
			if (!strcasecmp (atom + 2, "tract-int"))
				return EXTRACT_INT;
			if (!strcasecmp (atom + 2, "ists"))
				return EXISTS;
			if (!strcasecmp (atom + 2, "piry"))
				return EXPIRY;
			if (!strcasecmp (atom + 2, "pire"))
				return EXPIRE;
			if (!strcasecmp (atom + 2, "pired"))
				return TOKEN_EXPIRED;
		}
		if (!strcasecmp (atom + 1, "ncode-int"))
			return ENCODE_INT;
		if (!strcasecmp(atom + 1, "poch"))
			return EPOCH;
		if (!strcasecmp (atom + 1, "thernet"))
			return ETHERNET;
		if (!strcasecmp (atom + 1, "nds"))
			return ENDS;
		if (!strncasecmp (atom + 1, "ls", 2)) {
			if (!strcasecmp (atom + 3, "e"))
				return ELSE;
			if (!strcasecmp (atom + 3, "if"))
				return ELSIF;
			break;
		}
		if (!strcasecmp (atom + 1, "rror"))
			return ERROR;
		if (!strcasecmp (atom + 1, "val"))
			return EVAL;
		if (!strcasecmp (atom + 1, "ncapsulate"))
			return ENCAPSULATE;
		if (!strcasecmp(atom + 1, "xecute"))
			return EXECUTE;
		if (!strcasecmp(atom+1, "n")) {
			return EN;
		}
		break;
	      case 'f':
		if (!strcasecmp (atom + 1, "atal"))
			return FATAL;
		if (!strcasecmp (atom + 1, "ilename"))
			return FILENAME;
		if (!strcasecmp (atom + 1, "ixed-address"))
			return FIXED_ADDR;
		if (!strcasecmp (atom + 1, "ixed-address6"))
			return FIXED_ADDR6;
		if (!strcasecmp (atom + 1, "ixed-prefix6"))
			return FIXED_PREFIX6;
		if (!strcasecmp (atom + 1, "ddi"))
			return TOKEN_FDDI;
		if (!strcasecmp (atom + 1, "ormerr"))
			return NS_FORMERR;
		if (!strcasecmp (atom + 1, "unction"))
			return FUNCTION;
		if (!strcasecmp (atom + 1, "ailover"))
			return FAILOVER;
		if (!strcasecmp (atom + 1, "ree"))
			return TOKEN_FREE;
		break;
	      case 'g':
		if (!strncasecmp(atom + 1, "et", 2)) {
			if (!strcasecmp(atom + 3, "-lease-hostnames"))
				return GET_LEASE_HOSTNAMES;
			if (!strcasecmp(atom + 3, "hostbyname"))
				return GETHOSTBYNAME;
			if (!strcasecmp(atom + 3, "hostname"))
				return GETHOSTNAME;
			break;
		}
		if (!strcasecmp (atom + 1, "iaddr"))
			return GIADDR;
		if (!strcasecmp (atom + 1, "roup"))
			return GROUP;
		break;
	      case 'h':
		if (!strcasecmp(atom + 1, "ash"))
			return HASH;
		if (!strcasecmp (atom + 1, "ba"))
			return HBA;
		if (!strcasecmp (atom + 1, "ost"))
			return HOST;
		if (!strcasecmp (atom + 1, "ost-decl-name"))
			return HOST_DECL_NAME;
		if (!strcasecmp(atom + 1, "ost-identifier"))
			return HOST_IDENTIFIER;
		if (!strcasecmp (atom + 1, "ardware"))
			return HARDWARE;
		if (!strcasecmp (atom + 1, "ostname"))
			return HOSTNAME;
		if (!strcasecmp (atom + 1, "elp"))
			return TOKEN_HELP;
		break;
	      case 'i':
	      	if (!strcasecmp(atom+1, "a-na")) 
			return IA_NA;
	      	if (!strcasecmp(atom+1, "a-ta")) 
			return IA_TA;
	      	if (!strcasecmp(atom+1, "a-pd")) 
			return IA_PD;
	      	if (!strcasecmp(atom+1, "aaddr")) 
			return IAADDR;
	      	if (!strcasecmp(atom+1, "aprefix")) 
			return IAPREFIX;
		if (!strcasecmp (atom + 1, "nclude"))
			return INCLUDE;
		if (!strcasecmp (atom + 1, "nteger"))
			return INTEGER;
		if (!strcasecmp (atom  + 1, "nfiniband"))
			return TOKEN_INFINIBAND;
		if (!strcasecmp (atom + 1, "nfinite"))
			return INFINITE;
		if (!strcasecmp (atom + 1, "nfo"))
			return INFO;
		if (!strcasecmp (atom + 1, "p-address"))
			return IP_ADDRESS;
		if (!strcasecmp (atom + 1, "p6-address"))
			return IP6_ADDRESS;
		if (!strcasecmp (atom + 1, "nitial-interval"))
			return INITIAL_INTERVAL;
		if (!strcasecmp (atom + 1, "nitial-delay"))
			return INITIAL_DELAY;
		if (!strcasecmp (atom + 1, "nterface"))
			return INTERFACE;
		if (!strcasecmp (atom + 1, "dentifier"))
			return IDENTIFIER;
		if (!strcasecmp (atom + 1, "f"))
			return IF;
		if (!strcasecmp (atom + 1, "s"))
			return IS;
		if (!strcasecmp (atom + 1, "gnore"))
			return IGNORE;
		break;
	      case 'k':
		if (!strncasecmp (atom + 1, "nown", 4)) {
			if (!strcasecmp (atom + 5, "-clients"))
				return KNOWN_CLIENTS;
			if (!atom[5])
				return KNOWN;
			break;
		}
		if (!strcasecmp (atom + 1, "ey"))
			return KEY;
		break;
	      case 'l':
		if (!strcasecmp (atom + 1, "case"))
			return LCASE;
		if (!strcasecmp (atom + 1, "ease"))
			return LEASE;
		if (!strcasecmp(atom + 1, "ease6"))
			return LEASE6;
		if (!strcasecmp (atom + 1, "eased-address"))
			return LEASED_ADDRESS;
		if (!strcasecmp (atom + 1, "ease-time"))
			return LEASE_TIME;
		if (!strcasecmp(atom + 1, "easequery"))
			return LEASEQUERY;
		if (!strcasecmp(atom + 1, "ength"))
			return LENGTH;
		if (!strcasecmp (atom + 1, "imit"))
			return LIMIT;
		if (!strcasecmp (atom + 1, "et"))
			return LET;
		if (!strcasecmp (atom + 1, "oad"))
			return LOAD;
		if (!strcasecmp(atom + 1, "ocal"))
			return LOCAL;
		if (!strcasecmp (atom + 1, "og"))
			return LOG;
		if (!strcasecmp(atom+1, "lt")) {
			return LLT;
		}
		if (!strcasecmp(atom+1, "l")) {
			return LL;
		}
		break;
	      case 'm':
		if (!strncasecmp (atom + 1, "ax", 2)) {
			if (!atom [3])
				return TOKEN_MAX;
			if (!strcasecmp (atom + 3, "-balance"))
				return MAX_BALANCE;
			if (!strncasecmp (atom + 3, "-lease-", 7)) {
				if (!strcasecmp(atom + 10, "misbalance"))
					return MAX_LEASE_MISBALANCE;
				if (!strcasecmp(atom + 10, "ownership"))
					return MAX_LEASE_OWNERSHIP;
				if (!strcasecmp(atom + 10, "time"))
					return MAX_LEASE_TIME;
			}
			if (!strcasecmp(atom + 3, "-life"))
				return MAX_LIFE;
			if (!strcasecmp (atom + 3, "-transmit-idle"))
				return MAX_TRANSMIT_IDLE;
			if (!strcasecmp (atom + 3, "-response-delay"))
				return MAX_RESPONSE_DELAY;
			if (!strcasecmp (atom + 3, "-unacked-updates"))
				return MAX_UNACKED_UPDATES;
		}
		if (!strncasecmp (atom + 1, "in-", 3)) {
			if (!strcasecmp (atom + 4, "balance"))
				return MIN_BALANCE;
			if (!strcasecmp (atom + 4, "lease-time"))
				return MIN_LEASE_TIME;
			if (!strcasecmp (atom + 4, "secs"))
				return MIN_SECS;
			break;
		}
		if (!strncasecmp (atom + 1, "edi", 3)) {
			if (!strcasecmp (atom + 4, "a"))
				return MEDIA;
			if (!strcasecmp (atom + 4, "um"))
				return MEDIUM;
			break;
		}
		if (!strcasecmp (atom + 1, "atch"))
			return MATCH;
		if (!strcasecmp (atom + 1, "embers"))
			return MEMBERS;
		if (!strcasecmp (atom + 1, "y"))
			return MY;
		if (!strcasecmp (atom + 1, "clt"))
			return MCLT;
		break;
	      case 'n':
		if (!strcasecmp (atom + 1, "ormal"))
			return NORMAL;
		if (!strcasecmp (atom + 1, "ameserver"))
			return NAMESERVER;
		if (!strcasecmp (atom + 1, "etmask"))
			return NETMASK;
		if (!strcasecmp (atom + 1, "ever"))
			return NEVER;
		if (!strcasecmp (atom + 1, "ext-server"))
			return NEXT_SERVER;
		if (!strcasecmp (atom + 1, "ot"))
			return TOKEN_NOT;
		if (!strcasecmp (atom + 1, "o"))
			return TOKEN_NO;
		if (!strcasecmp (atom + 1, "oerror"))
			return NS_NOERROR;
		if (!strcasecmp (atom + 1, "otauth"))
			return NS_NOTAUTH;
		if (!strcasecmp (atom + 1, "otimp"))
			return NS_NOTIMP;
		if (!strcasecmp (atom + 1, "otzone"))
			return NS_NOTZONE;
		if (!strcasecmp (atom + 1, "xdomain"))
			return NS_NXDOMAIN;
		if (!strcasecmp (atom + 1, "xrrset"))
			return NS_NXRRSET;
		if (!strcasecmp (atom + 1, "ull"))
			return TOKEN_NULL;
		if (!strcasecmp (atom + 1, "ext"))
			return TOKEN_NEXT;
		if (!strcasecmp (atom + 1, "ew"))
			return TOKEN_NEW;
		break;
	      case 'o':
		if (!strcasecmp (atom + 1, "mapi"))
			return OMAPI;
		if (!strcasecmp (atom + 1, "r"))
			return OR;
		if (!strcasecmp (atom + 1, "n"))
			return ON;
		if (!strcasecmp (atom + 1, "pen"))
			return TOKEN_OPEN;
		if (!strcasecmp (atom + 1, "ption"))
			return OPTION;
		if (!strcasecmp (atom + 1, "ne-lease-per-client"))
			return ONE_LEASE_PER_CLIENT;
		if (!strcasecmp (atom + 1, "f"))
			return OF;
		if (!strcasecmp (atom + 1, "wner"))
			return OWNER;
		break;
	      case 'p':
		if (!strcasecmp (atom + 1, "repend"))
			return PREPEND;
		if (!strcasecmp(atom + 1, "referred-life"))
			return PREFERRED_LIFE;
		if (!strcasecmp (atom + 1, "acket"))
			return PACKET;
		if (!strcasecmp (atom + 1, "ool"))
			return POOL;
		if (!strcasecmp (atom + 1, "ool6"))
			return POOL6;
		if (!strcasecmp (atom + 1, "refix6"))
			return PREFIX6;
		if (!strcasecmp (atom + 1, "seudo"))
			return PSEUDO;
		if (!strcasecmp (atom + 1, "eer"))
			return PEER;
		if (!strcasecmp (atom + 1, "rimary"))
			return PRIMARY;
		if (!strcasecmp (atom + 1, "rimary6"))
			return PRIMARY6;
		if (!strncasecmp (atom + 1, "artner", 6)) {
			if (!atom [7])
				return PARTNER;
			if (!strcasecmp (atom + 7, "-down"))
				return PARTNER_DOWN;
		}
		if (!strcasecmp (atom + 1, "ort"))
			return PORT;
		if (!strcasecmp (atom + 1, "otential-conflict"))
			return POTENTIAL_CONFLICT;
		if (!strcasecmp (atom + 1, "ick-first-value") ||
		    !strcasecmp (atom + 1, "ick"))
			return PICK;
		if (!strcasecmp (atom + 1, "aused"))
			return PAUSED;
		break;
	      case 'r':
		if (!strcasecmp(atom + 1, "ange"))
			return RANGE;
		if (!strcasecmp(atom + 1, "ange6"))
			return RANGE6;
		if (isascii(atom[1]) &&
		    (tolower((unsigned char)atom[1]) == 'e')) {
			if (!strcasecmp(atom + 2, "bind"))
				return REBIND;
			if (!strcasecmp(atom + 2, "boot"))
				return REBOOT;
			if (!strcasecmp(atom + 2, "contact-interval"))
				return RECONTACT_INTERVAL;
			if (!strncasecmp(atom + 2, "cover", 5)) {
				if (atom[7] == '\0')
					return RECOVER;
				if (!strcasecmp(atom + 7, "-done"))
					return RECOVER_DONE;
				if (!strcasecmp(atom + 7, "-wait"))
					return RECOVER_WAIT;
				break;
			}
			if (!strcasecmp(atom + 2, "fresh"))
				return REFRESH;
			if (!strcasecmp(atom + 2, "fused"))
				return NS_REFUSED;
			if (!strcasecmp(atom + 2, "ject"))
				return REJECT;
			if (!strcasecmp(atom + 2, "lease"))
				return RELEASE;
			if (!strcasecmp(atom + 2, "leased"))
				return TOKEN_RELEASED;
			if (!strcasecmp(atom + 2, "move"))
				return REMOVE;
			if (!strcasecmp(atom + 2, "new"))
				return RENEW;
			if (!strcasecmp(atom + 2, "quest"))
				return REQUEST;
			if (!strcasecmp(atom + 2, "quire"))
				return REQUIRE;
			if (isascii(atom[2]) &&
			    (tolower((unsigned char)atom[2]) == 's')) {
				if (!strcasecmp(atom + 3, "erved"))
					return TOKEN_RESERVED;
				if (!strcasecmp(atom + 3, "et"))
					return TOKEN_RESET;
				if (!strcasecmp(atom + 3,
						"olution-interrupted"))
					return RESOLUTION_INTERRUPTED;
				break;
			}
			if (!strcasecmp(atom + 2, "try"))
				return RETRY;
			if (!strcasecmp(atom + 2, "turn"))
				return RETURN;
			if (!strcasecmp(atom + 2, "verse"))
				return REVERSE;
			if (!strcasecmp(atom + 2, "wind"))
				return REWIND;
			break;
		}
		break;
	      case 's':
		if (!strcasecmp(atom + 1, "cript"))
			return SCRIPT;
		if (isascii(atom[1]) && 
		    tolower((unsigned char)atom[1]) == 'e') {
			if (!strcasecmp(atom + 2, "arch"))
				return SEARCH;
			if (isascii(atom[2]) && 
			    tolower((unsigned char)atom[2]) == 'c') {
				if (!strncasecmp(atom + 3, "ond", 3)) {
                                        if (!strcasecmp(atom + 6, "ary"))
						return SECONDARY;
                                        if (!strcasecmp(atom + 6, "ary6"))
						return SECONDARY6;
                                        if (!strcasecmp(atom + 6, "s"))
                                                return SECONDS;
					break;
				}
                                if (!strcasecmp(atom + 3, "ret"))
                                        return SECRET;
				break;
			}
			if (!strncasecmp(atom + 2, "lect", 4)) {
                                if (atom[6] == '\0')
                                        return SELECT;
                                if (!strcasecmp(atom + 6, "-timeout"))
                                        return SELECT_TIMEOUT;
				break;
			}
                        if (!strcasecmp(atom + 2, "nd"))
                                return SEND;
			if (!strncasecmp(atom + 2, "rv", 2)) {
				if (!strncasecmp(atom + 4, "er", 2)) {
                                        if (atom[6] == '\0')
                                                return TOKEN_SERVER;
					if (atom[6] == '-') {
						if (!strcasecmp(atom + 7,
								"duid")) 
							return SERVER_DUID;
                                                if (!strcasecmp(atom + 7,
								"name"))
                                                        return SERVER_NAME;
                                                if (!strcasecmp(atom + 7,
								"identifier"))
                                                      return SERVER_IDENTIFIER;
						break;
					}
					break;
				}
                                if (!strcasecmp(atom + 4, "fail"))
                                        return NS_SERVFAIL;
				break;
			}
                        if (!strcasecmp(atom + 2, "t"))
                                return TOKEN_SET;
			break;
		}
		if (isascii(atom[1]) && 
		    tolower((unsigned char)atom[1]) == 'h') {
                        if (!strcasecmp(atom + 2, "ared-network"))
                                return SHARED_NETWORK;
                        if (!strcasecmp(atom + 2, "utdown"))
                                return SHUTDOWN;
			break;
		}
		if (isascii(atom[1]) && 
		    tolower((unsigned char)atom[1]) == 'i') {
                        if (!strcasecmp(atom + 2, "addr"))
                                return SIADDR;
                        if (!strcasecmp(atom + 2, "gned"))
                                return SIGNED;
                        if (!strcasecmp(atom + 2, "ze"))
                                return SIZE;
			break;
		}
		if (isascii(atom[1]) && 
		    tolower((unsigned char)atom[1]) == 'p') {
			if (isascii(atom[2]) && 
			    tolower((unsigned char)atom[2]) == 'a') {
                                if (!strcasecmp(atom + 3, "ce"))
                                        return SPACE;
                                if (!strcasecmp(atom + 3, "wn"))
                                        return SPAWN;
				break;
			}
                        if (!strcasecmp(atom + 2, "lit"))
                                return SPLIT;
			break;
		}
		if (isascii(atom[1]) && 
		    tolower((unsigned char)atom[1]) == 't') {
			if (isascii(atom[2]) && 
			    tolower((unsigned char)atom[2]) == 'a') {
				if(!strncasecmp(atom + 3, "rt", 2)) {
                                         if (!strcasecmp(atom + 5, "s"))
                                                 return STARTS;
                                         if (!strcasecmp(atom + 5, "up"))
                                                 return STARTUP;
					break;
				}
				if (isascii(atom[3]) &&
				    tolower((unsigned char)atom[3]) == 't') {
                                        if (!strcasecmp(atom + 4, "e"))
                                                return STATE;
                                        if (!strcasecmp(atom + 4, "ic"))
                                                return STATIC;
					break;
				}
			}
                        if (!strcasecmp(atom + 2, "ring"))
                                return STRING_TOKEN;
			break;
		}
                if (!strncasecmp(atom + 1, "ub", 2)) {
                        if (!strcasecmp(atom + 3, "class"))
                                return SUBCLASS;
                        if (!strcasecmp(atom + 3, "net"))
                                return SUBNET;
                        if (!strcasecmp(atom + 3, "net6"))
                                return SUBNET6;
                        if (!strcasecmp(atom + 3, "string"))
                                return SUBSTRING;
                        break;
                }
		if (isascii(atom[1]) && 
		    tolower((unsigned char)atom[1]) == 'u') {
                        if (!strcasecmp(atom + 2, "ffix"))
                                return SUFFIX;
                        if (!strcasecmp(atom + 2, "persede"))
                                return SUPERSEDE;
		}
                if (!strcasecmp(atom + 1, "witch"))
                        return SWITCH;
		break;
	      case 't':
		if (!strcasecmp (atom + 1, "imestamp"))
			return TIMESTAMP;
		if (!strcasecmp (atom + 1, "imeout"))
			return TIMEOUT;
		if (!strcasecmp (atom + 1, "oken-ring"))
			return TOKEN_RING;
		if (!strcasecmp (atom + 1, "ext"))
			return TEXT;
		if (!strcasecmp (atom + 1, "stp"))
			return TSTP;
		if (!strcasecmp (atom + 1, "sfp"))
			return TSFP;
		if (!strcasecmp (atom + 1, "ransmission"))
			return TRANSMISSION;
		if (!strcasecmp(atom + 1, "emporary"))
			return TEMPORARY;
		break;
	      case 'u':
		if (!strcasecmp (atom + 1, "case"))
			return UCASE;
		if (!strcasecmp (atom + 1, "nset"))
			return UNSET;
		if (!strcasecmp (atom + 1, "nsigned"))
			return UNSIGNED;
		if (!strcasecmp (atom + 1, "id"))
			return UID;
		if (!strncasecmp (atom + 1, "se", 2)) {
			if (!strcasecmp (atom + 3, "r-class"))
				return USER_CLASS;
			if (!strcasecmp (atom + 3, "-host-decl-names"))
				return USE_HOST_DECL_NAMES;
			if (!strcasecmp (atom + 3,
					 "-lease-addr-for-default-route"))
				return USE_LEASE_ADDR_FOR_DEFAULT_ROUTE;
			break;
		}
		if (!strncasecmp (atom + 1, "nknown", 6)) {
			if (!strcasecmp (atom + 7, "-clients"))
				return UNKNOWN_CLIENTS;
			if (!strcasecmp (atom + 7, "-state"))
				return UNKNOWN_STATE;
			if (!atom [7])
				return UNKNOWN;
			break;
		}
		if (!strcasecmp (atom + 1, "nauthenticated"))
			return UNAUTHENTICATED;
		if (!strcasecmp (atom + 1, "pdate"))
			return UPDATE;
		break;
	      case 'v':
		if (!strcasecmp (atom + 1, "6relay"))
			return V6RELAY;
		if (!strcasecmp (atom + 1, "6relopt"))
			return V6RELOPT;
		if (!strcasecmp (atom + 1, "endor-class"))
			return VENDOR_CLASS;
		if (!strcasecmp (atom + 1, "endor"))
			return VENDOR;
		break;
	      case 'w':
		if (!strcasecmp (atom + 1, "ith"))
			return WITH;
		if (!strcasecmp(atom + 1, "idth"))
			return WIDTH;
		break;
	      case 'y':
		if (!strcasecmp (atom + 1, "iaddr"))
			return YIADDR;
		if (!strcasecmp (atom + 1, "xdomain"))
			return NS_YXDOMAIN;
		if (!strcasecmp (atom + 1, "xrrset"))
			return NS_YXRRSET;
		break;
	      case 'z':
		if (!strcasecmp (atom + 1, "erolen"))
			return ZEROLEN;
		if (!strcasecmp (atom + 1, "one"))
			return ZONE;
		break;
	}
	return dfv;
}

