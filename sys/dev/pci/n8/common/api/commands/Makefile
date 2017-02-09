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

#***************************************************************************
# Makefile for API.
#
# Note:  This makefile is here only to allow building from within the
# common directory.  The make simply changes directory to the
# user-space build directories and does a build there.  This is here
# only as a convenience to developers working on files here.  It is
# not part of the actual build system.
#***************************************************************************
# Revision history:
#
# 05/07/03 bac Original version. 
#***************************************************************************
BASE_DIR= ../../..

include $(BASE_DIR)/Makefile.defaults

.PHONY: all clean

all:
	$(MAKE) -C $(BASE_DIR)/user/api
