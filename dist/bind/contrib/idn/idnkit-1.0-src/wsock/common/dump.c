/*
 * dump.c - dump data
 */

/*
 * Copyright (c) 2000 Japan Network Information Center.  All rights reserved.
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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wrapcommon.h"

char *
dumpAddr(const char FAR *addr, int len, char *buff, size_t size) {
	int i;
	char *p;
    
	buff[0] = '\0';
	for (i = 0, p = buff; i < len; i++) {
		char digits[8];

		sprintf(digits, "%d", (addr[i] & 0xff));
		if (i + 1 < len) {
			strcat(digits, ".");
		}
		if (strlen(digits) >= size) {
			break;
		}
		strcpy(p, digits);
		p += strlen(digits);
		size -= strlen(digits);
	}
	return (buff);
}

char *
dumpHost(const struct hostent FAR *hp, char *buff, size_t size) {
	char *p = buff;

	p[0] = '\0';
	if (strlen(hp->h_name) + 1 < size) {
		sprintf(p, "%s ", hp->h_name);
		p += strlen(p);
		size -= strlen(p);
	}
	dumpAddr(hp->h_addr_list[0], hp->h_length, p, size);
	return (buff);
}

char *
dumpName(const char *name, char *buff, size_t size) {
	const char *sp;
	char *dp;
    
	for (sp = name, dp = buff; *sp != '\0'; sp++) {
		if (*sp >= 0x21 && *sp <= 0x7e) {
			if (size < 2) {
				break;
			}
			*dp++ = *sp;
			size--;
		} else {
			if (size < 5) {
				break;
			}
			dp[0] = '\\';
			dp[1] = 'x';
			sprintf(dp + 2, "%02x", *sp & 0xff);
			dp += 4;
			size -= 4;
		}
	}
	*dp = '\0';

	return (buff);
}

