/*
 * Copyright (c) 2009, Erik van der Kouwe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution. 
 * 3. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO 
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <minix/com.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	struct pciinfo_entry *entry;
	struct pciinfo pciinfo;

	/* obtain a list of PCI devices from PM */
	if (getsysinfo(PM_PROC_NR, SI_PCI_INFO, &pciinfo) < 0)
	{
		perror("getsysinfo failed");
		return -1;
	}

	/* print the list of devices */
	entry = pciinfo.pi_entries;
	while (pciinfo.pi_count-- > 0)
	{
		printf("%.4X:%.4X %s\n", entry->pie_vid, entry->pie_did, entry->pie_name);
		entry++;
	}

	return 0;
}
