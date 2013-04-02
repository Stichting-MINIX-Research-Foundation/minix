Summary: NTP Time Synchronization Client 
Name: openntpd
Version: 3.7p1
Release: 1
Copyright: BSD License
Group: Applications/System
Source: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: openssl-devel
Requires: /usr/sbin/useradd, /usr/sbin/usermod
Requires: /sbin/chkconfig

#Patch1: openntpd-3.6p1-linux-adjtimex3.patch

%description
NTP Time Synchronization Client - http://www.openntpd.org

%prep
%setup -q -n %{name}-%{version}
#%patch1 -p0

%build
./configure \
  --sbindir=%{_sbindir} \
  --mandir=%{_mandir} \
  --sysconfdir=%{_sysconfdir} \
  --with-privsep-user=ntp
%{__make}

%clean
%{__rm} -rf $RPM_BUILD_ROOT

%install
%{__rm} -rf $RPM_BUILD_ROOT
%{__make} install DESTDIR=$RPM_BUILD_ROOT INSTALL="%{__install} -p"
%{__mkdir_p} $RPM_BUILD_ROOT/%{_initrddir}
%{__cp} contrib/redhat/ntpd $RPM_BUILD_ROOT/%{_initrddir}

%pre
%{__mkdir_p} /var/empty/ntpd
%{__chown} 0 /var/empty/ntpd
%{__chgrp} 0 /var/empty/ntpd
%{__chmod} 0755 /var/empty/ntpd

if ! /usr/bin/id ntp &>/dev/null; then
  useradd -c "OpenNTPD Unprivileged User" -u 38 -r -d /var/empty/ntpd ntp &>/dev/null || \
		%logmsg "Unexpected error adding user \"ntp\". Aborting installation."
fi
/usr/sbin/usermod -d /var/empty/ntpd -s /sbin/nologin ntp &>/dev/null || :



%post
/sbin/chkconfig --add ntpd

%preun
if [ $1 -eq 0 ]; then
  /sbin/service ntpd stop &>/dev/null || :
	/sbin/chkconfig --del ntpd
  /usr/sbin/userdel ntp
  exit 0
fi

%postun
/sbin/service ntp condrestart &>/dev/null || :


%files
%defattr(-,root,root,-)
%doc ChangeLog CREDITS README LICENCE
%config(noreplace) %attr(644, root, root) %{_sysconfdir}/ntpd.conf
%config %attr(755, root, root) %{_initrddir}/ntpd
%attr(755, root, root) %{_sbindir}/*
%attr(644, root, root) %{_mandir}/*

%changelog

