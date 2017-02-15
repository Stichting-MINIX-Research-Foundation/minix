#ifndef _IFCONFIG_UTIL_H
#define _IFCONFIG_UTIL_H

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include "parse.h"

struct afswtch {
	const char *af_name;
	short af_af;
	void (*af_status)(prop_dictionary_t, prop_dictionary_t, bool);
	void (*af_addr_commit)(prop_dictionary_t, prop_dictionary_t);
	bool (*af_addr_tentative)(struct ifaddrs *);
	SIMPLEQ_ENTRY(afswtch)	af_next;
};

void print_link_addresses(prop_dictionary_t, bool);
const char *get_string(const char *, const char *, u_int8_t *, int *, bool);
const struct afswtch *lookup_af_byname(const char *);
const struct afswtch *lookup_af_bynum(int);
void	print_string(const u_int8_t *, int);
int    getsock(int);
struct paddr_prefix *prefixlen_to_mask(int, int);
int direct_ioctl(prop_dictionary_t, unsigned long, void *);
int indirect_ioctl(prop_dictionary_t, unsigned long, void *);
bool ifa_any_preferences(const char *, struct ifaddrs *, int);
void ifa_print_preference(const char *, const struct sockaddr *);
int16_t ifa_get_preference(const char *, const struct sockaddr *);

#endif /* _IFCONFIG_UTIL_H */
