# Copyright (c) 1984 through 2008, William LeFebvre
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
# 
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
# 
#    * Neither the name of William LeFebvre nor the names of other
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Awk script converts an include file with definitions for signal names
# in to a predefined array that associates the signal numbers to the names.
#

BEGIN		{
		    nsig = 0;
		    j = 0;
		    print "/* This file was automatically generated */"
		    print "/* by the awk script \"sigconv.awk\".      */\n"
		    print "struct sigdesc {"
		    print "    const char *name;"
		    print "    int  number;"
		    print "};\n"
		    print "struct sigdesc sigdesc[] = {"
		}

/^#define[ \t][ \t]*SIG[A-Z]/	{

				    j = sprintf("%d", $3);
				    if (siglist[j] != "") next;
				    str = $2;

				    if (nsig < j) 
					nsig = j;

				    siglist[j] = sprintf("\"%s\",\t%2d", \
						substr(str, 4), j);
				}
/^#[ \t]*define[ \t][ \t]*SIG[A-Z]/	{

				    j = sprintf("%d", $4);
				    if (siglist[j] != "") next;
				    str = $3;

				    if (nsig < j)
					nsig = j;

				    siglist[j] = sprintf("\"%s\",\t%2d", \
						substr(str, 4), j);
				}
/^#[ \t]*define[ \t][ \t]*_SIG[A-Z]/	{

				    j = sprintf("%d", $4);
				    if (siglist[j] != "") next;
				    str = $3;

				    if (nsig < j)
					nsig = j;

				    siglist[j] = sprintf("\"%s\",\t%2d", \
					    substr(str, 5), j);
				}

END				{
				    for (n = 1; n <= nsig; n++) 
					if (siglist[n] != "")
					    printf("    { %s },\n", siglist[n]);

				    printf("    { NULL,\t 0 }\n};\n");
				}
