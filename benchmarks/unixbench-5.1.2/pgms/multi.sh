#! /bin/sh
###############################################################################
#  The BYTE UNIX Benchmarks - Release 3
#          Module: multi.sh   SID: 3.4 5/15/91 19:30:24
#          
###############################################################################
# Bug reports, patches, comments, suggestions should be sent to:
#
#	Ben Smith or Rick Grehan at BYTE Magazine
#	ben@bytepb.UUCP    rick_g@bytepb.UUCP
#
###############################################################################
#  Modification Log:
#
###############################################################################
ID="@(#)multi.sh:3.4 -- 5/15/91 19:30:24";
instance=1
while [ $instance -le $1 ]; do
	/bin/sh "$UB_BINDIR/tst.sh" &
	instance=`expr $instance + 1`
done
wait

