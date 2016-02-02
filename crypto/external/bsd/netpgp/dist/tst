#! /bin/sh

# function to mark a test as good or bad
marktest() {
	local lnum=$1
	local lgood=$2
	if [ $lgood -eq $lnum ]; then
		passed=$(expr $passed + 1)
		echo "$lnum	yes" >> passed
	else
		echo "$lnum	no" >> passed
	fi
}

while [ $# -gt 0 ]; do
	case "$1" in
	-v)
		set -x
		;;
	*)
		break
		;;
	esac
	shift
done

env USETOOLS=no MAKEOBJDIRPREFIX=/usr/build/amd64 sh -c 'cd ../libmj && \
	make cleandir ; \
	su root -c "make includes"; \
	make ; \
	su root -c "make install"'
env USETOOLS=no MAKEOBJDIRPREFIX=/usr/build/amd64 sh -c 'cd ../lib && \
	make cleandir ; \
	su root -c "make includes"; \
	make ; \
	su root -c "make install"'
env USETOOLS=no MAKEOBJDIRPREFIX=/usr/build/amd64 sh -c 'cd ../netpgp && \
	make cleandir ; \
	make ; \
	su root -c "make install"'
env USETOOLS=no MAKEOBJDIRPREFIX=/usr/build/amd64 sh -c 'cd ../netpgpkeys && \
	make cleandir ; \
	make ; \
	su root -c "make install"'

passed=0
total=36
rm -f passed
date > passed
echo "======> sign/verify 180938 file"
cp configure a
/usr/bin/netpgp --sign a
/usr/bin/netpgp --verify a.gpg && good=1
marktest 1 $good
echo "======> attempt to verify an unsigned file"
/usr/bin/netpgp --verify a || good=2
marktest 2 $good
echo "======> encrypt/decrypt 10809 file"
cp src/netpgp/netpgp.1 b
/usr/bin/netpgp --encrypt b
/usr/bin/netpgp --decrypt b.gpg
diff src/netpgp/netpgp.1 b && good=3
marktest 3 $good
echo "======> encrypt/decrypt 180938 file"
cp configure c
/usr/bin/netpgp --encrypt c
/usr/bin/netpgp --decrypt c.gpg
diff configure c && good=4
marktest 4 $good
echo "======> encrypt/decrypt bigass file"
cat configure configure configure configure configure configure > d
ls -l d
cp d e
/usr/bin/netpgp --encrypt d
/usr/bin/netpgp --decrypt d.gpg
diff e d && good=5
marktest 5 $good
echo "======> sign/verify detached signature file"
cat configure configure configure configure configure configure > f
/usr/bin/netpgp --sign --detached f
ls -l f f.sig
/usr/bin/netpgp --verify f.sig && good=6
marktest 6 $good
echo "======> cat signature - verified cat command"
/usr/bin/netpgp --cat a.gpg > a2
diff a a2 && good=7
marktest 7 $good
echo "======> another cat signature - verified cat command"
/usr/bin/netpgp --cat --output=a3 a.gpg
diff a a3 && good=8
marktest 8 $good
echo "======> netpgp list-packets test"
/usr/bin/netpgp --list-packets || good=9
marktest 9 $good
echo "======> version information"
/usr/bin/netpgp --version && good=10
marktest 10 $good
echo "======> netpgpverify file"
/usr/bin/netpgp -v < a.gpg && good=11
marktest 11 $good
echo "======> attempt to verify an unsigned file"
/usr/bin/netpgp -v < a || good=12
marktest 12 $good
echo "======> sign/verify detached signature file"
ls -l f f.sig
/usr/bin/netpgp -v f.sig && good=13
marktest 13 $good
echo "======> another verify signature - verified cat command"
/usr/bin/netpgp -v --output=a3 < a.gpg
diff a a3 && good=14
marktest 14 $good
echo "======> list keys"
/usr/bin/netpgpkeys --list-keys && good=15
marktest 15 $good
echo "======> version information"
/usr/bin/netpgp -v --version && good=16
marktest 16 $good
echo "======> find specific key information"
/usr/bin/netpgpkeys --get-key c0596823 agc@netbsd.org && good=17
marktest 17 $good
echo "======> ascii armoured signature"
cp Makefile.am g
/usr/bin/netpgp --sign --armor g && good=18
marktest 18 $good
echo "======> ascii armoured sig detection and verification"
/usr/bin/netpgp --verify g.asc && good=19
marktest 19 $good
echo "======> ascii armoured signature of large file"
cp Makefile.in g
/usr/bin/netpgp --sign --armor g && good=20
marktest 20 $good
echo "======> ascii armoured sig detection and verification of large file"
/usr/bin/netpgp --verify g.asc && good=21
marktest 21 $good
echo "======> verify memory by recognising ascii armour"
/usr/bin/netpgp --cat < g.asc > g2
diff g g2 && good=22
marktest 22 $good
echo "======> list ssh host RSA public key"
/usr/bin/netpgpkeys --ssh --sshkeyfile=/etc/ssh/ssh_host_rsa_key.pub --list-keys && good=23
marktest 23 $good
echo "======> sign/verify file with ssh host keys"
cp configure a
sudo /usr/bin/netpgp --ssh --sshkeyfile=/etc/ssh/ssh_host_rsa_key.pub --sign a
sudo chmod 644 a.gpg
/usr/bin/netpgp --verify --ssh --sshkeyfile=/etc/ssh/ssh_host_rsa_key.pub a.gpg && good=24
marktest 24 $good
echo "======> pipeline and memory encrypt/decrypt"
/usr/bin/netpgp --encrypt < a | /usr/bin/netpgp --decrypt > a4
diff a a4 && good=25
marktest 25 $good
echo "======> pipeline and memory sign/verify"
/usr/bin/netpgp --sign < a | /usr/bin/netpgp --cat > a5
diff a a5 && good=26
marktest 26 $good
echo "======> verify within a duration"
cp Makefile.am h
/usr/bin/netpgp --sign --duration 6m --detached h
/usr/bin/netpgp --verify h.sig && good=27
marktest 27 $good
echo "======> invalid signature - expired"
rm -f h.sig
/usr/bin/netpgp --sign --duration 2 --detached h
sleep 3
/usr/bin/netpgp --verify h.sig || good=28
marktest 28 $good
echo "======> list signatures and subkey signatures"
/usr/bin/netpgpkeys --list-sigs && good=29
marktest 29 $good
echo "======> generate a new RSA key"
/usr/bin/netpgpkeys --generate-key && good=30
marktest 30 $good
echo "======> ascii detached armoured signature"
cp Makefile.am i
/usr/bin/netpgp --sign --armor --detached i && good=31
marktest 31 $good
echo "======> ascii detached armoured sig detection and verification"
/usr/bin/netpgp --verify i.asc && good=32
marktest 32 $good
echo "======> host ssh fingerprint and netpgp fingerprint"
netpgpkey=$(/usr/bin/netpgpkeys --ssh --sshkeyfile=/etc/ssh/ssh_host_rsa_key.pub --list-keys --hash=md5 | awk 'NR == 3 { print $3 $4 $5 $6 $7 $8 $9 $10 }')
sshkey=$(/usr/bin/ssh-keygen -l -f /etc/ssh/ssh_host_rsa_key.pub | awk '{ gsub(":", "", $2); print $2 }')
echo "host sshkey \"$sshkey\" = netpgpkey \"$netpgpkey\""
[ $sshkey = $netpgpkey ] && good=33
marktest 33 $good
echo "======> user ssh fingerprint and netpgp fingerprint"
netpgpkey=$(/usr/bin/netpgpkeys --ssh --list-keys --hash=md5 | awk 'NR == 3 { print $3 $4 $5 $6 $7 $8 $9 $10 }')
sshkey=$(/usr/bin/ssh-keygen -l -f /home/agc/.ssh/id_rsa.pub | awk '{ gsub(":", "", $2); print $2 }')
echo "user sshkey \"$sshkey\" = netpgpkey \"$netpgpkey\""
[ $sshkey = $netpgpkey ] && good=34
marktest 34 $good
echo "======> single key listing"
/usr/bin/netpgpkeys -l agc && good=35
marktest 35 $good
echo "======> pipeline and memory encrypt/decrypt with specified cipher"
/usr/bin/netpgp -e --cipher camellia128 < a | /usr/bin/netpgp -d > a6
diff a a6 && good=36
marktest 36 $good
rm -f a a.gpg b b.gpg c c.gpg d d.gpg e f f.sig g g.asc g2 a2 a3 a4 a5 a6 h h.sig i i.asc
echo "Passed ${passed}/${total} tests"
