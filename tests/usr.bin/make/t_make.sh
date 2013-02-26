# $NetBSD: t_make.sh,v 1.1 2012/03/17 16:33:14 jruoho Exp $
#
# Copyright (c) 2008, 2010 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

# Executes make and compares the output to a golden file.
run_and_check()
{
	local name="${1}"; shift

	local srcdir="$(atf_get_srcdir)"
	atf_check -o file:"${srcdir}/d_${name}.out" -x \
	    "make -k -f ${srcdir}/d_${name}.mk 2>&1 | sed -e 's,${srcdir}/d_,,'"
}

# Defines a test case for make(1), parsing a given file and comparing the
# output to prerecorded results.
test_case()
{
	local name="${1}"; shift
	local descr="${1}"; shift

	atf_test_case "${name}"
	eval "${name}_head() { \
		atf_set descr '${descr}'; \
	}"
	eval "${name}_body() { \
		run_and_check '${name}'; \
	}"
}

test_case comment "Checks comments (PR/17732, PR/30536)"
test_case cond1 "Checks conditionals (PR/24420)"
test_case dotwait "Checks .WAIT"
test_case export "Checks .export of provided variables"
test_case export_all "Checks .export of all global variables"
test_case moderrs "Checks correct handling of errors in modifiers usage"
test_case modmatch "Checks modifier :M"
test_case modmisc "Checks modifiers specified in variables"
test_case modorder "Checks modifier :Ox"
test_case modts "Checks modifier :ts"
test_case modword "Checks modifier :[]"
test_case posix "Checks conformance to POSIX"
test_case qequals "Checks operator ?="
test_case ternary "Checks ternary modifier"
test_case varcmd "Checks behavior of command line variable assignments"
test_case unmatchedvarparen "Checks $ ( ) matches"

atf_init_test_cases()
{
	atf_add_test_case comment
	atf_add_test_case cond1
	atf_add_test_case dotwait
	atf_add_test_case export
	atf_add_test_case export_all
	atf_add_test_case moderrs
	atf_add_test_case modmatch
	atf_add_test_case modmisc
	atf_add_test_case modorder
	atf_add_test_case modts
	atf_add_test_case modword
	atf_add_test_case posix
	atf_add_test_case qequals
	atf_add_test_case ternary
	atf_add_test_case varcmd
	atf_add_test_case unmatchedvarparen
}
