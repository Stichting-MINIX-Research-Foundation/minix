#
#  Copyright (c) International Business Machines  Corp., 2001
#
#  This program is free software;  you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY;  without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
#  the GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program;  if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#

SRCS   = libipc.c rmobj.c parse_opts.c tst_res.c tst_sig.c tst_tmpdir.c
OBJS   = $(SRCS:.c=.o)
LIBIPC = ../libipc.a

CFLAGS += -I../../../../../include -Wall -D_NETBSD_SOURCE=1

all: $(LIBIPC)

$(LIBIPC): $(OBJS)
	$(AR) -rc $@ $(OBJS)

install:

clean:
	rm -f $(OBJS) $(LIBIPC)
