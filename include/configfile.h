/*	configfile.h - Generic configuration file format.
 *							Author: Kees J. Bot
 *								5 Jun 1999
 */
#ifndef _CONFIGFILE_H
#define _CONFIGFILE_H

/* Data can only be modified inside the library. */
#ifndef _c
#define _c	const
#endif

typedef _c struct config {	/* Contents of a generic configuration file. */
_c	struct config	*next;		/* Next configuration file thing. */
_c	struct config	*list;		/* For a { sublist }. */
	const char	*file;		/* File and line where this is found. */
	unsigned	line;
	int		flags;		/* Special flags. */
	char		word[1];	/* Payload. */
} config_t;

#define CFG_CLONG	0x0001		/* strtol(word, &end, 0) is valid. */
#define CFG_OLONG	0x0002		/* strtol(word, &end, 010). */
#define CFG_DLONG	0x0004		/* strtol(word, &end, 10). */
#define CFG_XLONG	0x0008		/* strtol(word, &end, 0x10). */
#define CFG_CULONG	0x0010		/* strtoul(word, &end, 0). */
#define CFG_OULONG	0x0020		/* strtoul(word, &end, 010). */
#define CFG_DULONG	0x0040		/* strtoul(word, &end, 10). */
#define CFG_XULONG	0x0080		/* strtoul(word, &end, 0x10). */
#define CFG_STRING	0x0100		/* The word is enclosed in quotes. */
#define CFG_SUBLIST	0x0200		/* This is a sublist, so no word. */
#define CFG_ESCAPED	0x0400		/* Escapes are still marked with \. */

config_t *config_read(const char *_file, int flags, config_t *_cfg);
void config_delete(config_t *_cfg);
int config_renewed(config_t *_cfg);
size_t config_length(config_t *_cfg);
#define config_issub(cfg)	(!!((cfg)->flags & CFG_SUBLIST))
#define config_isatom(cfg)	(!config_issub(cfg))
#define config_isstring(cfg)	(!!((cfg)->flags & CFG_STRING))

#undef _c

#endif /* _CONFIGFILE_H */
