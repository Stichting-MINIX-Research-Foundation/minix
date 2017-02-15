#ifndef _IFCONFIG_ENV_H
#define _IFCONFIG_ENV_H

#include <prop/proplib.h>

const char *getifname(prop_dictionary_t);
ssize_t getargstr(prop_dictionary_t, const char *, char *, size_t);
ssize_t getargdata(prop_dictionary_t, const char *, uint8_t *, size_t);
int getaf(prop_dictionary_t);
int getifflags(prop_dictionary_t, prop_dictionary_t, unsigned short *);
const char *getifinfo(prop_dictionary_t, prop_dictionary_t, unsigned short *);
prop_dictionary_t prop_dictionary_augment(prop_dictionary_t, prop_dictionary_t);

/*
 * XXX: this really doesn't belong in here, but env.h is conveniently
 * included from all source modules *after* system headers, so it
 * allows us to be lazy.  See Makefile for more details.
 */
#ifdef RUMP_ACTION
#include <rump/rump.h>
#include <rump/rump_syscalls.h>
#include <rump/rumpclient.h>
#endif /* RUMP_ACTION */

#endif /* _IFCONFIG_ENV_H */
