#-
# Copyright (C) 2001-2003 by NBMK Encryption Technologies.
# All rights reserved.
# 
# NBMK Encryption Technologies provides no support of any kind for
# this software.  Questions or concerns about it may be addressed to
# the members of the relevant open-source community at
# <tech-crypto@netbsd.org>.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
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

#*****************************************************************************
# @(#) Makefile 1.20@(#)
#*****************************************************************************
# Makefile for NetOctave's Queue Manager
#***************************************************************************
# Revision history:
# 03/21/02 brr Reorganize 2.1 directory structure.
# 03/06/02 brr Removed SAPI from the include path.
# 02/20/02 brr Reinstated QMgrInit.
# 01/16/02 brr Removed obsolete QMgrInit & NSPInit.
# 01/13/02 brr Removed obsolete QMMemory.
# 01/04/02 brr Modified to build in kernel space.
# 11/27/01 brr Removed obsolete Queue source files.
# 10/22/01 brr Original version. 
#***************************************************************************

BUILD=Kernel

BASE_DIR = ../..
include $(BASE_DIR)/Makefile.defaults

.PHONY: clean


SOURCES = QMgrInit.c QMQueue.c QMUtil.c RNQueue.c
OBJECTS = $(SOURCES:.c=.o)

INCLUDES +=	-I$(BASE_DIR)/driver/common \
                -I$(BASE_DIR)/driver/$(TARGET_DIR)

ifeq ($(TARGET_OS), VxWorks)
 PROGRAM	= QMgr.out
else
  ifeq ($(TARGET_OS), Linux)
    PROGRAM	= QMgr.o
  endif
endif

all: $(PROGRAM)

QMgr.out: $(OBJECTS) Makefile $(BASE_DIR)/Makefile.defaults
	$(LD) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

QMgr.o: $(OBJECTS)
	ld -r -o $(PROGRAM) $(OBJECTS)

clean:
	$(RM) $(RMFLAGS) *.d *.o $(PROGRAM)


ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCES:.c=.d)
endif



