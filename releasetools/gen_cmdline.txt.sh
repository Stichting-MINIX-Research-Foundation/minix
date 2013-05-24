#!/bin/sh
#default for the beagleboard-xM
CONSOLE=tty02 
#verbosity
VERBOSE=3 
while getopts "c:v:?" c
do
        case "$c" in
        \?)
                echo "Usage: $0 [-c consoletty] [-v level]" >&2
                exit 1
                ;;
        c)
                # genrate netbooting uEnv.txt
                CONSOLE=$OPTARG
		;;
        v)
                # genrate netbooting uEnv.txt
                VERBOSE=$OPTARG
		;;
        esac
done


echo console=$CONSOLE rootdevname=c0d0p1s0 verbose=$VERBOSE
