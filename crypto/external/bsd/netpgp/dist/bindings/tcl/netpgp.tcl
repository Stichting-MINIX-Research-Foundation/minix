#! /usr/bin/env tclsh

# netpgp bindings for tcl

load libnetpgptcl.so

# initialisations
set n [new_netpgp_t]
netpgp_setvar $n "homedir" "/home/agc/.gnupg"
netpgp_setvar $n "hash" "SHA256"
netpgp_init $n

set userid [netpgp_getvar $n "userid"]
netpgp_sign_file $n $userid "a" "a.gpg" 0 0 0
netpgp_verify_file $n "a.gpg" "/dev/null" 0
