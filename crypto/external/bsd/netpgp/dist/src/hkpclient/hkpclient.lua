#! /usr/bin/env lua

--
-- Copyright (c) 2010 The NetBSD Foundation, Inc.
-- All rights reserved.
--
-- This code is derived from software contributed to The NetBSD Foundation
-- by Alistair Crooks (agc@netbsd.org)
--
-- Redistribution and use in source and binary forms, with or without
-- modification, are permitted provided that the following conditions
-- are met:
-- 1. Redistributions of source code must retain the above copyright
--    notice, this list of conditions and the following disclaimer.
-- 2. Redistributions in binary form must reproduce the above copyright
--    notice, this list of conditions and the following disclaimer in the
--    documentation and/or other materials provided with the distribution.
--
-- THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
-- ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
-- TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
-- PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
-- BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
-- POSSIBILITY OF SUCH DAMAGE.
--

-- a short HKP client

require("cURL")

-- command line args
dofile "optparse.lua"

opt = OptionParser{usage="%prog [options] file", version="20100226"}

opt.add_option{"-V", "--version", action="store_true", dest="version",
			help="--version"}
opt.add_option{"-m", "--mr", action="store_true", dest="mr", help="-m"}
opt.add_option{"-o", "--op", action="store", dest="op", help="-o op"}
opt.add_option{"-p", "--port", action="store", dest="port", help="-p port"}
opt.add_option{"-s", "--server", action="store", dest="server", help="-s server"}

-- parse command line args
options,args = opt.parse_args()

-- set defaults
local server = options.server or "pgp.mit.edu"
local port = options.port or 11371
local op = options.op or "get"
local mr = ""
if options.mr then mr = "&options=mr" end

-- get output stream
f = io.output()

c = cURL.easy_init()

-- setup url
c:setopt_url("http://" .. server .. ":" .. port ..
	"/pks/lookup?op=" .. op .. "&search=" .. args[1] .. mr)

-- perform, invokes callbacks
c:perform({writefunction = function(str) 
				f:write(str)
			     end})

-- close output file
f:close()

