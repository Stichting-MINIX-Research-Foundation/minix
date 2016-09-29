#!/bin/sh
#
# DESCRIBE 2.2 - Describe the given devices.		Author: Kees J. Bot
#
# BUGS
# - Arguments may not contain shell metacharacters.

case $# in
0)	flag=; set -$- /dev ;;
*)	flag=d ;;
esac

ls -l$flag $* | \
sed	-e '/^total/d' \
	-e '/^l/d' \
	-e '/^[^bc]/s/.* /BAD BAD /' \
	-e '/^[bc]/s/.* \([0-9][0-9]*\), *\([0-9][0-9]*\).* /\1 \2 /' \
| {
ex=0	# exit code

while read major minor path
do
    case $path in
    /*)	name=`expr $path : '.*/\\(.*\\)$'`
	;;
    *)	name=$path
    esac
    dev= des=

    case $major in	# One of the controllers?  What is its controller nr?
    3)	ctrlr=0	;;
    8)	ctrlr=1	;;
    10)	ctrlr=2	;;
    12)	ctrlr=3	;;
    esac

    case $major,$minor in
    1,0)	des="RAM disk" dev=ram
	;;
    1,1)	des="memory" dev=mem
	;;
    1,2)	des="kernel memory" dev=kmem
	;;
    1,3)	des="null device, data sink" dev=null
	;;
    1,4)	des="boot device loaded from boot image" dev=boot
	;;
    1,5)	des="null byte stream generator" dev=zero
	;;
    1,6)	des="boot image RAM disk" dev=imgrd
	;;
    1,[789]|1,1[012])
		ramdisk=`expr $minor - 7`
		des="RAM disk $ramdisk" dev=ram$ramdisk
	;;
    2,*)	drive=`expr $minor % 4`
	case `expr $minor - $drive` in
	0)	des='auto density' dev="fd$drive"
	    ;;
	4)	des='360k, 5.25"' dev="pc$drive"
	    ;;
	8)	des='1.2M, 5.25"' dev="at$drive"
	    ;;
	12)	des='360k in 720k, 5.25"' dev="qd$drive"
	    ;;
	16)	des='720k, 3.5"' dev="ps$drive"
	    ;;
	20)	des='360k in 1.2M, 5.25"' dev="pat$drive"
	    ;;
	24)	des='720k in 1.2M, 5.25"' dev="qh$drive"
	    ;;
	28)	des='1.44M, 3.5"' dev="PS$drive"
	    ;;
	112)	des='auto partition 0' dev="fd${drive}p0"
	    ;;
	116)	des='auto partition 1' dev="fd${drive}p1"
	    ;;
	120)	des='auto partition 2' dev="fd${drive}p2"
	    ;;
	124)	des='auto partition 3' dev="fd${drive}p3"
	    ;;
	*)	dev=BAD
	esac
	des="floppy drive $drive ($des)"
	;;
    [38],[05]|[38],[123][05]|1[02],[05]|1[02],[123][05])
	drive=`expr $minor / 5`
	des="controller $ctrlr disk $drive" dev=c${ctrlr}d${drive}
	;;
    [38],?|[38],[123]?|1[02],?|1[02],[123]?)
	drive=`expr $minor / 5`
	par=`expr $minor % 5 - 1`
	des="controller $ctrlr disk $drive partition $par"
	dev=c${ctrlr}d${drive}p${par}
	;;
    [38],12[89]|[38],1[3-9]?|[38],2??|1[02],12[89]|1[02],1[3-9]?|1[02],2??)
	drive=`expr \\( $minor - 128 \\) / 16`
	par=`expr \\( \\( $minor - 128 \\) / 4 \\) % 4`
	sub=`expr \\( $minor - 128 \\) % 4`
	des="hard disk $drive, partition $par, subpartition $sub"
	des="controller $ctrlr disk $drive partition $par slice $sub"
	#par=`expr $drive '*' 5 + $par`
	dev=c${ctrlr}d${drive}p${par}s${sub}
	;;
    [38],6[4-9]|[38],7?|1[02],6[4-9]|1[02],7?)
	tape=`expr \\( $minor - 64 \\) / 2`
	case $minor in
	*[02468])
	    des="controller $ctrlr tape $tape (non-rewinding)"
	    dev=c${ctrlr}t${tape}n
	    ;;
	*[13579])
	    des="controller $ctrlr tape $tape (rewinding)"
	    dev=c${ctrlr}t${tape}
	esac
	;;
    4,0)	des="console device" dev=console
	;;
    4,[1-7])des="virtual console $minor" dev=ttyc$minor
	;;
    4,15)	des="diagnostics device" dev=log
	;;
    4,1[6-9])
	line=`expr $minor - 16`
	des="serial line $line" dev=tty0$line
	;;
    4,125)	des="video output" dev=video
	;;
    5,0)	des="anonymous tty" dev=tty
	;;
    6,0)	des="line printer, parallel port" dev=lp
	;;
    7,0)
	des="Berkeley Packet Filter device" dev=bpf
	;;
    9,0)
	des="unix98 pseudoterminal master" dev=ptmx
	;;
    9,12[89]|9,1[3-8]?|9,19[01])
	p=`expr \\( $minor - 128 \\) / 16 | tr '0123' 'pqrs'`
	n=`expr $minor % 16`
	test $n -ge 10 && n=`expr $n - 10 | tr '012345' 'abcdef'`
	des="pseudo tty `expr $minor - 128`" dev=tty$p$n
	;;
    9,???)
	p=`expr \\( $minor - 192 \\) / 16 | tr '0123' 'pqrs'`
	n=`expr $minor % 16`
	test $n -ge 10 && n=`expr $n - 10 | tr '012345' 'abcdef'`
	des="controller of tty$p$n" dev=pty$p$n
	;;
    11,0)
	des="block filter" dev=filter
	;;
    13,0)
	des="audio" dev=audio
	;;
    14,0)
	des="faulty block device driver" dev=fbd
	;;
    15,0)
	des="kernel log" dev=klog
	;;
    16,0)
	des="pseudo random number generator" dev=urandom
	;;
    17,0)
	des="hello" dev=hello
	;;
    5[6-9],0|6[0-3],0)
	drive=`expr $major - 56`
	des="vnode disk $drive" dev=vnd$drive
	;;
    5[6-9],[1-4]|6[0-3],[1-4])
	drive=`expr $major - 56`
	par=`expr $minor - 1`
	des="vnode disk $drive partition $par" dev=vnd${drive}p${par}
	;;
    5[6-9],12[89]|5[6-9],13[0-9]|5[6-9],14[0-3]|6[0-3],12[89]|5[6-9],13[0-9]|5[6-9],14[0-3])
	drive=`expr $major - 56`
	par=`expr \\( \\( $minor - 128 \\) / 4 \\) % 4`
	sub=`expr \\( $minor - 128 \\) % 4`
	des="vnode disk $drive partition $par slice $sub"
	dev=vnd${drive}p${par}s${sub}
	;;
    64,0)
	des="keyboard input multiplexer"
	dev=kbdmux
	;;
    64,[1-4])
	n=`expr $minor - 1`
	des="keyboard input $n"
	dev=kbd$n
	;;
    64,64)
	des="mouse input multiplexer"
	dev=mousemux
	;;
    64,6[5-8])
	n=`expr $minor - 65`
	des="mouse input $n"
	dev=mouse$n
	;;
    BAD,BAD)
	des= dev=
	;;
    *)	dev=BAD
    esac

    case $name:$dev in
    *:)
	echo "$path: not a device" >&2
	ex=1
	;;
    *:*BAD*)
	echo "$path: cannot describe: major=$major, minor=$minor" >&2
	ex=1
	;;
    $dev:*)
	echo "$path: $des"
	;;
    *:*)	echo "$path: nonstandard name for $dev: $des"
    esac
done

exit $ex
}
