#!/bin/sh

# Shell script used to test the Remote MIB (RMIB) functionality.

# We test a couple of things here, using the rmibtest service and test87:
# - some cases where remote MIB subtree registration should fail;
# - a new mount point (minix.rtest) with a small tree behind it, on which we
#   test some basic reads and writes on an integer pointer and a function;
# - shadowing of an existing subtree (minix.test) with a similarly looking
#   subtree, which we then subject to a subset of test87;
# - resource accounting, making sure everything is the same before and after.

bomb() {
  echo $*
  minix-service down rmibtest 2>/dev/null
  exit 1
}

PATH=/bin:/usr/bin:/sbin:/usr/sbin
export PATH

echo -n "Test RMIB "

cd rmibtest

sysctl -q minix.rtest && bomb "there should not be a minix.rtest"

old_nodes=`sysctl -n minix.mib.nodes 2>/dev/null` || bomb "no MIB stats?"
old_objects=`sysctl -n minix.mib.objects 2>/dev/null` || bomb "no MIB stats?"
old_remotes=`sysctl -n minix.mib.remotes 2>/dev/null` || bomb "no MIB stats?"

minix-service up `pwd`/rmibtest -label rmibtest -config rmibtest.conf || \
  bomb "unable to start test service"

cd ..

sleep 1

new_remotes=`sysctl -n minix.mib.remotes 2>/dev/null` || \
  bomb "unable to get mount stats"
[ $(($old_remotes + 2)) -eq $new_remotes ] || bomb "mounting subtree failed"

# Test the temporary minix.rtest subtree with its two mirroring nodes
sysctl -q minix.rtest || bomb "there should be a minix.rtest"

[ $(sysctl -n minix.rtest.int) -eq 5375123 ] || bomb "unexpected int value"
[ $(sysctl -n minix.rtest.func) -eq 5375123 ] || bomb "unexpected func value"
sysctl -wq minix.rtest.int=456 || bomb "unable to set int value"
[ $(sysctl -n minix.rtest.int) -eq 456 ] || bomb "unexpected int value"
[ $(sysctl -n minix.rtest.func) -eq 456 ] || bomb "unexpected func value"
sysctl -wq minix.rtest.func=7895375 || bomb "unable to set func value"
[ $(sysctl -n minix.rtest.int) -eq 7895375 ] || bomb "unexpected int value"
[ $(sysctl -n minix.rtest.func) -eq 7895375 ] || bomb "unexpected func value"

# Test the minix.test shadowing subtree using a subset of the regular MIB test
./test87 19 >/dev/null || bomb "test87 reported failure"

minix-service down rmibtest

sleep 1

# Is everything back to the old situation?
new_nodes=`sysctl -n minix.mib.nodes 2>/dev/null` || bomb "no MIB stats?"
new_objects=`sysctl -n minix.mib.objects 2>/dev/null` || bomb "no MIB stats?"
new_remotes=`sysctl -n minix.mib.remotes 2>/dev/null` || bomb "no MIB stats?"

[ $old_nodes -eq $new_nodes ] || bomb "stats not equal after unmount"
[ $old_objects -eq $new_objects ] || bomb "stats not equal after unmount"
[ $old_remotes -eq $new_remotes ] || bomb "stats not equal after unmount"

sysctl -q minix.rtest && bomb "there should not be a minix.rtest anymore"

echo "ok"
exit 0
