#
# Copyright (c) 2008 Kungliga Tekniska Högskolan
# (Royal Institute of Technology, Stockholm, Sweden). 
# All rights reserved. 
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions 
# are met: 
#
# 1. Redistributions of source code must retain the above copyright 
#    notice, this list of conditions and the following disclaimer. 
#
# 2. Redistributions in binary form must reproduce the above copyright 
#    notice, this list of conditions and the following disclaimer in the 
#    documentation and/or other materials provided with the distribution. 
#
# 3. Neither the name of the Institute nor the names of its contributors 
#    may be used to endorse or promote products derived from this software 
#    without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
# ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
# SUCH DAMAGE. 


import sys

tokens = [ 'SYMBOL' ]
literals = ['{','}',';', ':']

t_SYMBOL    = r'[a-zA-Z_][a-zA-Z0-9_\.]*'
t_ignore = " \t\n"

def t_error(t):
    print "Illegal character '%s'" % t.value[0]
    t.lexer.skip(1)
    
import ply.lex as lex
lex.lex()

namespace = "global"
symbols = []

def p_syms(p):
    'syms : SYMBOL "{" elements "}"'
    print "# %s" % p[1]

def p_elements(p):
    '''elements : element
             | element elements'''

def p_element(p):
    '''element : SYMBOL ":"
               | SYMBOL ";"'''
    global namespace
    if p[2] == ':':
        namespace = p[1]
    else:
        symbols.append([namespace, p[1]])
    
def p_error(p):
    if p:
        print "Syntax error at '%s'" % p.value
    else:
        print "Syntax error at EOF"

import ply.yacc as yacc
yacc.yacc()

lines = sys.stdin.readlines()

for line in lines:
    yacc.parse(line)

for symbol in symbols:
    if symbol[0] == "global":
        print "%s" % symbol[1]
