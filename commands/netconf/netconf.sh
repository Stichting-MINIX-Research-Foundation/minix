#!/bin/sh
#
#	netconf 0.1 - Configure network	
#
# Changes:
#						

LOCALRC=/usr/etc/rc.local
INETCONF=/etc/inet.conf
RCNET=/etc/rc.net
HOSTS=/etc/hosts
HOSTNAME=/etc/hostname.file
USRKBFILE=/.usrkb

step1=""
step2=""
step3=""
v=1 # verbosity
manual_opts=0
prefix=""
cd="no" # running from cd?

eth=""
driver=""
driverargs=""

config=""
manual=""
dhcp="no"

hostname=""
hostname_prev=""
ip=""
ip_prev=""
netmask=""
netmask_prev=""
gateway=""
dns1=""
dns2=""

# Provide some sane defaults
hostname_default=`uname -n`
test -z "$hostname_default" && hostname_default="Minix"
ip_default="10.0.0.1"
netmask_default="255.255.255.0"
gateway_default=""

usage()
{
    cat >&2 <<'EOF'
Usage:

  netconf [-q] [-p <prefix>] [-e <num>] [-a]
  netconf [-H <name> -i <ip> -n <mask> -g <gw> -d <prim dns> [-s <sec dns>]]

  flags:
     -q Limit generated output
     -p Path prefix for configuration files (e.g., during install -p mnt is used as files are mounted on /mnt).
     -e Ethernet card
     -a Use DHCP (-H, -i, -n, -g, -d, and -s flags are discarded)
     -H Hostname
     -i IP address
     -n Netmask
     -g Default gateway
     -d Primary DNS
     -s Secondary DNS
     -h Shows this help file
     -c Shows a list of ethernet cards supported

  By default netconf starts in Interactive mode. By providing parameters on the
  command line, some questions can be omitted.
EOF
    exit 1
}

card()
{
	card_number=$1
	card_name=$2
	card_avail=0
	shift 2
	while [ $# -gt 0 ]
	do 
		lspci | grep > /dev/null "^$1" && card_avail=1
		shift
	done
	if [ $card_avail -gt 0 ]
	then 
		card_mark="*"
		eth_default=$card_number
	else
		card_mark=" "
	fi
	printf "%2d. %s %s\n" "$card_number" "$card_mark" "$card_name"
}

cards()
{
    card 0 "No Ethernet card (no networking)"
    card 1 "Intel Pro/100" "8086:103D" "8086:1064" "8086:1229" "8086:2449"
    card 2 "3Com 501 or 3Com 509 based card"
    card 3 "Realtek 8139 based card (also emulated by KVM)"            \
           "10EC:8139" "02AC:1012" "1065:8139" "1113:1211" "1186:1300" \
           "1186:1340" "11DB:1234" "1259:A117" "1259:A11E" "126C:1211" \
           "13D1:AB06" "1432:9130" "14EA:AB06" "14EA:AB07" "1500:1360" \
           "1743:8139" "4033:1360"
    card 4 "Realtek 8169 based card"	\
           "10EC:8129" "10EC:8167" "10EC:8169" "1186:4300" "1259:C107" \
           "1385:8169" "16EC:0116" "1737:1032" "10EC:8168"
    card 5 "Realtek 8029 based card (also emulated by Qemu)" "10EC:8029"
    card 6 "NE2000, 3com 503 or WD based card (also emulated by Bochs)"
    card 7 "AMD LANCE (also emulated by VMWare and VirtualBox)" "1022:2000"
    card 8 "Intel PRO/1000 Gigabit" "8086:100E" "8086:107C" "8086:10CD"
    card 9 "Attansic/Atheros L2 FastEthernet" "1969:2048"
    card 10 "DEC Tulip 21140A in VirtualPC" "1011:0009"
    card 11 "Different Ethernet card (no networking)"
}

warn()
{
    echo -e "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b ! $1"
}

do_step1()
{
    eth_default=0
    
    # Ask user about networking
    echo "MINIX 3 currently supports the following Ethernet cards. PCI cards detected"
    echo "by MINIX are marked with *. Please choose: "
    echo ""
    cards
    echo ""

    while [ "$step1" != ok ]; do
      echo -n "Ethernet card? [$eth_default] "; read eth
      test -z $eth && eth=$eth_default

      drv_params $eth
      test -n "$driver" && step1="ok"
    done
}

drv_params()
{
      case "$1" in
        0) driver=psip0;    ;;    
	1) driver=fxp;      ;;
	2) driver=dpeth;    driverargs="#dpeth_arg='DPETH0=port:irq:memory'";
	   test "$v" = 1 && echo ""
           test "$v" = 1 && echo "Note: After installing, edit $LOCALRC to the right configuration."
		;;
	4) driver=rtl8169;  ;;
	3) driver=rtl8139;  ;;
	5) driver=dp8390;   driverargs="dp8390_arg='DPETH0=pci'";	;;
	6) driver=dp8390;   driverargs="dp8390_arg='DPETH0=240:9'"; 
	   test "$v" = 1 && echo ""
           test "$v" = 1 && echo "Note: After installing, edit $LOCALRC to the right configuration."
           test "$v" = 1 && echo " chose option 4, the defaults for emulation by Bochs have been set."
		;;
        7) driver="lance"; ;;    
	8) driver="e1000"; ;;
        9) driver="atl2";   ;;
        10) driver="dec21140A"; ;;    
        11) driver="psip0"; ;;    
        *) warn "choose a number"
      esac
}

do_step2()
{
    echo ""
    echo "Configure network using DHCP or manually?"
    echo ""
    echo "1. Automatically using DHCP"
    echo "2. Manually"
    echo ""

    while [ "$step2" != ok ]
      do
        echo -n "Configure method? [1] "; read config
	test -z $config && config=1
      
	case "$config" in
	    1) step2="ok"; dhcp="yes" ; ;;
            2) step2="ok"; manual="do"; ;;
	    *) warn "choose a number"
	esac
    done
    
    # Use manual parameters?
    if [ -n "$manual" ]; then
        # Query user for settings
        # Hostname
	if [ -z $hostname_prev ]; then
	    hostname_prev=$hostname_default
	fi
	echo -n "Hostname [$hostname_prev]: "
	read hostname
	if [ ! -z $hostname ]; then
	    hostname_prev=$hostname
	else
	    hostname=$hostname_prev
	fi
	
        # IP address
	if [ -z $ip_prev ]; then
	    ip_prev=$ip_default
	fi
	echo -n "IP address [$ip_prev]: "
	read ip
	if [ ! -z $ip ]; then
	    ip_prev=$ip
	else
	    ip=$ip_prev
	fi
	
        # Netmask
	if [ -z $netmask_prev ]; then
	    netmask_prev=$netmask_default
	fi
	echo -n "Netmask [$netmask_prev]: "
	read netmask
	if [ ! -z $netmask ]; then
	    netmask_prev=$netmask
	else
	    netmask=$netmask_prev
	fi
	
        # Gateway (no gateway is fine for local networking)
	echo -n "Gateway: "
	read gateway
	    
        # DNS Servers
	echo -n "Primary DNS Server [$dns1_prev]: "
	read dns1
	test -z "$dns1" && test -n "$dns1_prev" && dns1=$dns1_prev
	if [ ! -z "$dns1" ]; then
	    dns1_prev=$dns1
	    
	    echo -n "Secondary DNS Server [$dns2_prev]: "
	    read dns2
	    if [ ! -z $dns2 ]; then
		dns2_prev=$dns2
	    fi
	else
	    # If no primary DNS, then also no secondary DNS
	    dns2=""
	fi
    fi
}

# Parse options
while getopts ":qe:p:aH:i:n:g:d:s:hc" arg; do
    case "$arg" in
	q) v=0; ;;
	e) ethernet=$OPTARG; 
	   test "$ethernet" -ge 0 -a "$ethernet" -le 7 2>/dev/null || usage
	   drv_params $ethernet
	   ;;
	p) prefix=$OPTARG; ;;
	a) dhcp="yes"; ;;
	H) hostname=$OPTARG; manual_opts=`expr $manual_opts '+' 1`;;
	i) ip=$OPTARG;       manual_opts=`expr $manual_opts '+' 1`;;
	n) netmask=$OPTARG;  manual_opts=`expr $manual_opts '+' 1`;;
	g) gateway=$OPTARG;  manual_opts=`expr $manual_opts '+' 1`;;
	d) dns1=$OPTARG;     ;;
	s) dns2=$OPTARG;     ;;
	h) usage ;;
	c) echo -e "The following cards are supported by Minix:\n";
	   cards; exit 0
	   ;;
	\?) echo "Unknown option -$OPTARG"; usage ;;
	:) echo "Missing required argument for -$OPTARG"; usage ;;
	*)  usage ;;
    esac
done

# Verify parameter count
if [ "$dhcp" != "yes" ] ; then
    if [ $manual_opts -gt 0 ] ; then
        test $manual_opts -eq 4 -a -n "$dns1" || usage
        manual="do"
    fi
fi

if [ -n "$prefix" ] ; then
    LOCALRC=$prefix$LOCALRC
    INETCONF=$prefix$INETCONF
    RCNET=$prefix$RCNET
    HOSTS=$prefix$HOSTS
    HOSTNAME=$prefix$HOSTNAME
    if [ ! -f  $INETCONF ]; then
    	echo -e "It seems the supplied prefix (\`$prefix') is invalid."
    	exit 1
    fi
fi

if [ "$USER" != root ] ; then
    test "$v" = 1 && echo "Please run netconf as root."
    exit 1
fi

# Are we running from CD?
if [ -f "$USRKBFILE" ] ; then
    cd="yes" # We are running from CD
fi

# Do we know what ethernet card to use?
test -z "$ethernet" && do_step1

# If no parameters are supplied and we're not using DHCP, query for settings
test $manual_opts -eq 0 -a "$dhcp" = "no" && do_step2

# Store settings.
# Do not make backups if we're running from CD
test "$cd" != "yes" && test -f $INETCONF && mv $INETCONF "$INETCONF~" && 
                     test "$v" = 1 && echo "Backed up $INETCONF to $INETCONF~"
test "$cd" != "yes" && test -f $LOCALRC && mv $LOCALRC "$LOCALRC~" &&
                     test "$v" = 1 && echo "Backed up $LOCALRC to $LOCALRC~"

if [ "$driver" = "psip0" ]; then
    echo "psip0 { default; } ;" > $INETCONF
else
    echo "eth0 $driver 0 { default; } ;" > $INETCONF
fi
echo "$driverargs" > $LOCALRC

if [ -n "$manual" ]
    then
    # Backup config file if it exists and we're not running from CD
    test "$cd" != "yes" && test -f $RCNET && mv $RCNET "$RCNET~" && 
                      test "$v" = 1 && echo "Backed up $RCNET to $RCNET~"
    test "$cd" != "yes" && test -f $HOSTS && mv $HOSTS "$HOSTS~" && 
                      test "$v" = 1 && echo "Backed up $HOSTS to $HOSTS~"

    # Store manual config
    echo "ifconfig -I /dev/ip0 -n $netmask -h $ip" > $RCNET
    test ! -z $gateway && echo "add_route -g $gateway" >> $RCNET
    echo "daemonize nonamed -L" >> $RCNET
    test ! -z $dns1 && echo -e "$ip\t%nameserver\t#$hostname" > $HOSTS
    test ! -z $dns1 && echo -e "$dns1\t%nameserver\t#DNS 1" >> $HOSTS
    test ! -z $dns2 && echo -e "$dns2\t%nameserver\t#DNS 2" >> $HOSTS
    echo -e "\n$ip\t$hostname" >> $HOSTS
    echo $hostname > $HOSTNAME
else
    test "$cd" != "yes" && test -f "$RCNET" && mv "$RCNET" "$RCNET~" && 
        test "$v" = 1 && echo "Moved $RCNET to $RCNET~ to use default settings"
    test "$cd" != "yes" && test -f $HOSTS && mv $HOSTS "$HOSTS~" && 
        test "$v" = 1 && echo "Backed up $HOSTS to $HOSTS~"
    test -f "$HOSTS~" && grep -v "%nameserver" "$HOSTS~" > $HOSTS
fi

test "$cd" != "yes" && test "$v" = 1 && echo "
You might have to reboot for the changes to take effect."
exit 0
