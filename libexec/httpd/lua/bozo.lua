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

opt = OptionParser{usage="%prog [options] root [vhost]", version="20091105"}                           

opt.add_option{"-C", "--cgimap", action="store", dest="cgimap", help="--cgimap 's t'"}
opt.add_option{"-H", "--hide-dots", action="store_true", dest="hidedots", help="--hide-dots"}
opt.add_option{"-I", "--portnum", action="store", dest="portnum", help="--portnum number"}
opt.add_option{"-M", "--dynamicmime", action="store", dest="dynmime", help="--dynamicmime 'suffix type a b'"}
opt.add_option{"-S", "--server-software", action="store", dest="serversw", help="--server-software name"}
opt.add_option{"-U", "--username", action="store", dest="username", help="--username name"}
opt.add_option{"-V", "--unknown-slash", action="store_true", dest="unknown", help="--unknown-slash"}
opt.add_option{"-X", "--dir-index", action="store_true", dest="dirindex", help="--dir-index"}
opt.add_option{"-Z", "--ssl", action="store", dest="ssl", help="--ssl 'cert priv'"}
opt.add_option{"-b", "--background", action="store", dest="background", help="--background count"}
opt.add_option{"-c", "--cgibin", action="store", dest="cgibin", help="--cgibin bin"}
opt.add_option{"-e", "--dirtyenv", action="store_true", dest="dirtyenv", help="--dirtyenv"}
opt.add_option{"-f", "--foreground", action="store_true", dest="foreground", help="--foreground"}
opt.add_option{"-i", "--bindaddr", action="store", dest="bindaddress", help="--bindaddr address"}
opt.add_option{"-n", "--numeric", action="store_true", dest="numeric", help="--numeric"}
opt.add_option{"-p", "--public-html", action="store", dest="public_html", help="--public-html dir"}
opt.add_option{"-r", "--trusted-referal", action="store_true", dest="trustedref", help="trusted referal"}
opt.add_option{"-s", "--logtostderr", action="store_true", dest="logstderr", help="log to stderr"}
opt.add_option{"-t", "--chroot", action="store", dest="chroot", help="--chroot dir"}
opt.add_option{"-u", "--enable-users", action="store_true", dest="enableusers", help="--enable-users"}
opt.add_option{"-v", "--virtbase", action="store", dest="virtbase", help="virtual base location"}
opt.add_option{"-x", "--index-html", action="store", dest="indexhtml", help="index.html name"}

-- caller lua script
local extension = ".so"
f = io.open("libluabozohttpd.dylib", "r")
if f then
	extension = ".dylib"
	io.close(f)
end
glupkg = package.loadlib("./" .. "libluabozohttpd" .. extension, "luaopen_bozohttpd")
bozohttpd = glupkg()

-- initialise
httpd = bozohttpd.new()
bozohttpd.init_httpd(httpd)
prefs = bozohttpd.init_prefs()

-- parse command line args
options,args = opt.parse_args()
if options.portnum then
        bozohttpd.set_pref(prefs, "port number", options.portnum)
end
if options.background then
        bozohttpd.set_pref(prefs, "background", options.background)
end
if options.numeric then
        bozohttpd.set_pref(prefs, "numeric", "true")
end
if options.logstderr then
        bozohttpd.set_pref(prefs, "log to stderr", "true")
end
if options.foreground then
        bozohttpd.set_pref(prefs, "foreground", "true")
end
if options.trustedref then
        bozohttpd.set_pref(prefs, "trusted referal", "true")
end
if options.dynmime then
	suffix, type, s1, s2 = string.find(options.dynmime,
					"(%S+)%s+(%S+)%s+(%S+)%s+(%S+)")
        bozohttpd.dynamic_mime(httpd, suffix, type, s1, s2)
end
if options.serversw then
        bozohttpd.set_pref(prefs, "server software", options.serversw)
end
if options.ssl then
	cert, priv = string.find(options.ssl, "(%S+)%s+(%S+)")
        bozohttpd.dynamic_mime(httpd, cert, priv)
end
if options.username then
        bozohttpd.set_pref(prefs, "username", options.username)
end
if options.unknownslash then
        bozohttpd.set_pref(prefs, "unknown slash", "true")
end
if options.virtbase then
        bozohttpd.set_pref(prefs, "virtual base", options.virtbase)
end
if options.indexhtml then
        bozohttpd.set_pref(prefs, "index.html", options.indexhtml)
end
if options.dirtyenv then
        bozohttpd.set_pref(prefs, "dirty environment", "true")
end
if options.bindaddr then
        bozohttpd.set_pref(prefs, "bind address", options.bindaddr)
end
if options.cgibin then
        bozohttpd.cgi_setbin(httpd, options.cgibin)
end
if options.cgimap then
	name, handler = string.find(options.cgimap, "(%S+)%s+(%S+)")
        bozohttpd.cgi_map(httpd, name, handler)
end
if options.public_html then
        bozohttpd.set_pref(prefs, "public_html", options.public_html)
end
if options.chroot then
        bozohttpd.set_pref(prefs, "chroot dir", options.chroot)
end
if options.enableusers then
        bozohttpd.set_pref(prefs, "enable users", "true")
end
if options.hidedots then
        bozohttpd.set_pref(prefs, "hide dots", "true")
end
if options.dirindex then
        bozohttpd.set_pref(prefs, "directory indexing", "true")
end

if #args < 1 then
	print("At least one arg needed for root directory")
else
	-- set up connections
	local vhost = args[2] or ""
	bozohttpd.setup(httpd, prefs, vhost, args[1])

	-- loop, serving requests
	local numreps = options.background or 0
	repeat
		req = bozohttpd.read_request(httpd)
		bozohttpd.process_request(httpd, req)
		bozohttpd.clean_request(req)
	until numreps == 0
end
