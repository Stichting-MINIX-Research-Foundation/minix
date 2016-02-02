#! /usr/bin/env lua

--
-- Copyright (c) 2009 The NetBSD Foundation, Inc.
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

-- command line args
dofile "optparse.lua"

opt = OptionParser{usage="%prog [options] file", version="20090711"}                           

opt.add_option{"-s", "--sign", action="store_true", dest="sign", help="--sign [--detached] [--armour] file"}
opt.add_option{"-v", "--verify", action="store_true", dest="verify", help="--verify [--armour] file"}
opt.add_option{"-e", "--encrypt", action="store_true", dest="encrypt", help="--encrypt [--armour] file"}
opt.add_option{"-d", "--decrypt", action="store_true", dest="decrypt", help="--decrypt [--armour] file"}
opt.add_option{"-h", "--homedir", action="store", dest="homedir", help="--homedir directory"}
opt.add_option{"-o", "--output", action="store", dest="output", help="--output file"}
opt.add_option{"-a", "--armour", action="store_true", dest="armour", help="--armour"}
opt.add_option{"-D", "--detached", action="store_true", dest="detached", help="--detached"}

-- caller lua script
local extension = ".so"
f = io.open("libluanetpgp.dylib", "r")
if f then
	extension = ".dylib"
	io.close(f)
end
glupkg = package.loadlib("libluanetpgp" .. extension, "luaopen_netpgp")
netpgp = glupkg()

-- initialise
pgp = netpgp.new()

-- parse command line args
options,args = opt.parse_args()

-- set defaults
local output = options.output or ""
local armour = "binary"
if options.armour then
	armour = "armour"
end
local detached = "attached"
if options.detached then
	detached = "detached"
end
if options.homedir then
	netpgp.homedir(pgp, options.homedir)
end

-- initialise everything
netpgp.init(pgp)

local i
for i = 1, #args do
	if options.encrypt then
		-- encrypt a file
		netpgp.encrypt_file(pgp, args[1], output, armour)
		os.execute("ls -l " .. args[1] .. ".gpg")
	end
	if options.decrypt then
		-- decrypt file
		netpgp.decrypt_file(pgp, args[1], output, armour)
	end
	if options.sign then
		-- detached signature
		netpgp.sign_file(pgp, args[1], output, armour, detached)
		os.execute("ls -l " .. args[1] .. ".sig")
	end
	if options.verify then
		-- verification of detached signature
		netpgp.verify_file(pgp, args[1], armour)
	end
end
