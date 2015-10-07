#! /usr/bin/env python2.5

# netpgp bindings for python
import _netpgppython

# initialisations
n = _netpgppython.new_netpgp_t()
_netpgppython.netpgp_setvar(n, "homedir", "/home/agc/.gnupg")
_netpgppython.netpgp_setvar(n, "hash", "SHA256")
_netpgppython.netpgp_init(n)

userid = _netpgppython.netpgp_getvar(n, "userid")
_netpgppython.netpgp_sign_file(n, userid, "a", "a.gpg", 0, 0, 0)
_netpgppython.netpgp_verify_file(n, "a.gpg", "/dev/null", 0)

