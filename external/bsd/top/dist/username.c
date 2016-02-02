/*
 * Copyright (c) 1984 through 2008, William LeFebvre
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 * 
 *     * Neither the name of William LeFebvre nor the names of other
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 */

/*
 *  Username translation code for top.
 *
 *  These routines handle uid to username mapping.  They use a hash table to
 *  reduce reading overhead.  Entries are refreshed every EXPIRETIME seconds.
 *
 *  The old ad-hoc hash functions have been replaced with something a little
 *  more formal and (hopefully) more robust (found in hash.c)
 */

#include "os.h"

#include <pwd.h>

#include "top.h"
#include "utils.h"
#include "hash.h"
#include "username.h"

#define EXPIRETIME (60 * 5)

/* we need some sort of idea how long usernames can be */
#ifndef MAXLOGNAME
#ifdef _POSIX_LOGIN_NAME_MAX 
#define MAXLOGNAME _POSIX_LOGIN_NAME_MAX 
#else
#define MAXLOGNAME 9
#endif
#endif

struct hash_data {
    int    uid;
    char   name[MAXLOGNAME];  /* big enough? */
    time_t expire;
};

hash_table *userhash;


void
init_username(void)

{
    userhash = hash_create(211);
}

char *
username(int xuid)

{
    struct hash_data *data;
    struct passwd *pw;
    time_t now;

    /* what time is it? */
    now = time(NULL);

    /* get whatever is in the cache */
    data = hash_lookup_uint(userhash, (unsigned int)xuid);

    /* if we had a cache miss, then create space for a new entry */
    if (data == NULL)
    {
	/* make space */
	data = emalloc(sizeof(struct hash_data));

	/* fill in some data, including an already expired time */
	data->uid = xuid;
	data->expire = (time_t)0;

	/* add it to the hash: the rest gets filled in later */
	hash_add_uint(userhash, xuid, data);
    }

    /* Now data points to the correct hash entry for "xuid".  If this is
       a new entry, then expire is 0 and the next test will be true. */
    if (data->expire <= now)
    {
	if ((pw = getpwuid(xuid)) != NULL)
	{
	    strncpy(data->name, pw->pw_name, MAXLOGNAME-1);
	    data->expire = now + EXPIRETIME;
	    dprintf("username: updating %d with %s, expires %d\n",
		    data->uid, data->name, data->expire);
	}
	else
	{
	    /* username doesnt exist ... so invent one */
	    snprintf(data->name, sizeof(data->name), "%d", xuid);
	    data->expire = now + EXPIRETIME;
	    dprintf("username: updating %d with %s, expires %d\n",
		    data->uid, data->name, data->expire);
	}	    
    }

    /* return what we have */
    return data->name;
}

int
userid(char *xusername)

{
    struct passwd *pwd;

    if ((pwd = getpwnam(xusername)) == NULL)
    {
	return(-1);
    }

    /* return our result */
    return(pwd->pw_uid);
}

