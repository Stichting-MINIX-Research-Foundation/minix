#!/bin/sh
#
#	adduser 1.0 - add a new user to the system	Author: Kees J. Bot
#								16 Jan 1996

# Check arguments.
case "$#" in
3)	user="$1"; group="$2"; home="$3"
	;;
*)	echo "Usage: adduser user group home-dir" >&2; exit 1
esac

# We need to be root.
case "`id`" in
'uid=0('*)
	;;
*)	echo "adduser: you must be root to add users" >&2; exit 1
esac

# User and group names must be alphanumeric and no longer than 8 characters.
len=`expr "$user" : '[a-z][a-z0-9]*$'`
if [ "$len" -eq 0 -o "$len" -gt 8 ]
then
	echo >&2 \
"adduser: the user name must be alphanumeric and no longer than 8 characters"
	exit 1
fi

len=`expr "$group" : '[a-z][a-z0-9]*$'`
if [ "$len" -eq 0 -o "$len" -gt 8 ]
then
	echo >&2 \
"adduser: the group name must be alphanumeric and no longer than 8 characters"
	exit 1
fi

# The new user name must not exist, but the group must exist.
if grep "^$user:" /etc/passwd >/dev/null
then
	echo "adduser: user $user already exists" >&2
	exit 1
fi

gid=`sed -e "/^$group:/!d" -e 's/^[^:]*:[^:]*:\\([^:]*\\):.*/\\1/' /etc/group`
if [ `expr "$gid" : '[0-9]*$'` -eq 0 ]
then
	echo "adduser: group $group does not exist" >&2
	exit 1
fi

# Find the first free user-id of 10 or higher.
uid=10
while grep "^[^:]*:[^:]*:$uid:.*" /etc/passwd >/dev/null
do
	uid=`expr $uid + 1`
done

# No interruptions.
trap '' 1 2 3 15

# Lock the password file.
ln /etc/passwd /etc/ptmp || {
	echo "adduser: password file busy, try again later"
	exit 1
}

# Make the new home directory, it should not exist already.
mkdir "$home" || {
	rm -rf /etc/ptmp
	exit 1
}

# Make the new home directory by copying the honorary home directory of our
# fearless leader.
echo cpdir /usr/ast "$home"
cpdir /usr/ast "$home" || {
	rm -rf /etc/ptmp "$home"
	exit 1
}

# Change the ownership to the new user.
echo chown -R $uid:$gid "$home"
chown -R $uid:$group "$home" || {
	rm -rf /etc/ptmp "$home"
	exit 1
}

# Is there a shadow password file?  If so add an entry.
if [ -f /etc/shadow ]
then
	echo "echo $user::0:0::: >>/etc/shadow"
	echo "$user::0:0:::" >>/etc/shadow || {
		rm -rf /etc/ptmp "$home"
		exit 1
	}
	pwd="##$user"
else
	pwd=
fi

# Finish up by adding a password file entry.
echo "echo $user:$pwd:$uid:$gid:$user:$home: >>/etc/passwd"
echo "$user:$pwd:$uid:$gid:$user:$home:" >>/etc/passwd || {
	rm -rf /etc/ptmp "$home"
	exit 1
}

# Remove the lock.
rm /etc/ptmp || exit

echo "
The new user $user has been added to the system.  Note that the password,
full name, and shell may be changed with the commands passwd(1), chfn(1),
and chsh(1).  The password is now empty, so only console logins are possible."
if [ $gid = 0 ]
then
	echo "\
Also note that a new operator needs an executable search path (\$PATH) that
does not contain the current directory (an empty field or "." in \$PATH)."
fi
exit 0
