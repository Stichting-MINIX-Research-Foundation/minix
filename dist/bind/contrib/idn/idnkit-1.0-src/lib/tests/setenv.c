#ifndef lint
static char *rcsid = "$Id: setenv.c,v 1.1.1.1 2003-06-04 00:27:01 marka Exp $";
#endif

/*
 * Copyright (c) 2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <stddef.h>
#include <string.h>

/*
 * We don't include <stdlib.h> here.
 * Also <stdlib.h> may declare `environ' and its type might be different
 * from ours.
 */
extern char **environ;

typedef struct myenv myenv_t;

struct myenv {
	char *pointer;
	myenv_t *next;
	myenv_t *prev;
};

static myenv_t *myenvs = NULL;

void
myunsetenv(const char *name) {
	char **e;
	myenv_t *mye;
	size_t namelen;
	extern void free(void *);

	namelen = strlen(name);
	for (e = environ; *e != NULL; e++) {
		if (strncmp(*e, name, namelen) == 0 && (*e)[namelen] == '=')
			break;
	}
	if (*e == NULL)
		return;

	for (mye = myenvs; mye != NULL; mye = mye->next) {
		if (mye->pointer == *e) {
			if (mye->next != NULL)
				mye->next->prev = mye->prev;
			if (mye->prev != NULL)
				mye->prev->next = mye->next;
			if (mye->next == NULL && mye->prev == NULL)
				myenvs = NULL;
			free(mye);
			free(*e);
			break;
		}
	}

	for ( ; *e != NULL; e++)
		*e = *(e + 1);
}

#include <stdlib.h>

int
mysetenv(const char *name, const char *value, int overwrite) {
	myenv_t *mye;
	char *buffer;
	int result;

	if (getenv(name) != NULL && !overwrite)
		return 0;

	buffer = (char *) malloc(strlen(name) + strlen(value) + 2);
	if (buffer == NULL)
		return -1;
	strcpy(buffer, name);
	strcat(buffer, "=");
	strcat(buffer, value);

	myunsetenv(name);

	mye = (myenv_t *) malloc(sizeof(myenv_t));
	if (mye == NULL)
		return -1;
	mye->pointer = buffer;
	mye->next = myenvs;
	mye->prev = NULL;
	if (myenvs != NULL)
		myenvs->prev = mye;
	myenvs = mye;

	result = putenv(buffer);

	return result;	
}
