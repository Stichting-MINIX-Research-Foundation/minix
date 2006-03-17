#!/bin/sh
echo -n "Would you like to install binary packages from the CD? (y/N) "
read y
if [ "$y" = y -o "$y" = Y ]
then	echo "Ok, showing you a list of packages, please type y"
	echo "for every package you want installed."
	/usr/bin/packme
else	echo "Ok, not installing binary packages."
fi
echo "Use the 'packme' command after rebooting MINIX to get another chance"
echo "to install binary packages."
