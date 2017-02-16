%define prefix /usr
%define sysconfdir /etc
%define version 1.0

# official/beta release:
#define release 1
#define distrel %{version}

# release candidate:
%define release rc1
%define distrel %{version}-%{release}

%define serial 2002051501

%define name idnkit
%define distsrc %{name}-%{distrel}-src

Name: %{name}
Version: %{version}
Release: %{release}
Copyright: distributable
Group: System Environment
Source: %{distsrc}.tar.gz
BuildRoot: /var/tmp/%{name}-root
Serial: %{serial}
Summary: Internationalized Domain Name kit (idnkit/JPNIC)
Vendor: JPNIC
Packager: Japan Network Information Center

%description
idnkit is a kit for handling Internationalized Domain Name.

%package devel
Group: Development/Libraries
Summary: The development files for idnkit

%description devel
The header files and libraries (libidnkit.a and libidnkitlite.a)
to develop applications that use the libraries.

%prep
%setup -n %{distsrc}

%build
if [ -f /usr/lib/libiconv.a -o -f /usr/lib/libiconv.so ]
then
  if [ -f /lib/libc-2.0* ]
  then
    ICONV="--with-iconv=yes"
  fi
fi

CFLAGS="$RPM_OPT_FLAGS" ./configure \
	--prefix=%{prefix} --sysconfdir=%{sysconfdir} \
	--enable-runidn \
	$ICONV
make

%install
rm -fr $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
mv $RPM_BUILD_ROOT/etc/idn.conf.sample $RPM_BUILD_ROOT/etc/idn.conf
mv $RPM_BUILD_ROOT/etc/idnalias.conf.sample $RPM_BUILD_ROOT/etc/idnalias.conf

# devel kit
#install -c lib/libidnkit.a $RPM_BUILD_ROOT/usr/lib
#cp -r include/idn $RPM_BUILD_ROOT/usr/include

# docs
mkdir rpm_docs
(cp NEWS INSTALL INSTALL.ja DISTFILES README.ja README LICENSE.txt \
    ChangeLog rpm_docs)
cp -r patch rpm_docs

%clean
rm -fr $RPM_BUILD_ROOT

%files
%defattr(-, root, root)
%{prefix}/bin/idnconv
%{prefix}/bin/runidn
%{prefix}/lib/libidnkit.so.*
%{prefix}/lib/libidnkitlite.so.*
%{prefix}/lib/libidnkitres.so.*
%{prefix}/share/idnkit/*
%config %{sysconfdir}/idn.conf
%config %{sysconfdir}/idnalias.conf
%attr(0644, root, root) %config(noreplace) 
%attr(0644, root, man) %{prefix}/man/man1/*
%attr(0644, root, man) %{prefix}/man/man3/*
%attr(0644, root, man) %{prefix}/man/man5/*
%doc rpm_docs/*

%files devel
%defattr(-, root, root)
%{prefix}/lib/libidnkit.a
%{prefix}/lib/libidnkit.la
%{prefix}/lib/libidnkit.so
%{prefix}/lib/libidnkitlite.a
%{prefix}/lib/libidnkitlite.la
%{prefix}/lib/libidnkitlite.so
%{prefix}/lib/libidnkitres.a
%{prefix}/lib/libidnkitres.la
%{prefix}/lib/libidnkitres.so
%{prefix}/include/idn/*

%changelog
* Web May 15 2002 Motoyuki Kasahara <m-kasahr@sra.co.jp>
- 1.0beta2, experimental.

* Mon May 28 2001 MANABE Takashi <manabe@dsl.gr.jp>
- include runmdn, libmdnresolv

* Mon Apr  4 2001 Motoyuki Kasahara <m-kasahr@sra.co.jp>
- 2.1 release

* Mon Apr  4 2001 Motoyuki Kasahara <m-kasahr@sra.co.jp>
- 2.0.1 release

* Mon Apr  2 2001 MANABE Takashi <manabe@dsl.gr.jp>
- 2.0 release

* Fri Mar  3 2001 MANABE Takashi <manabe@dsl.gr.jp>
- 1.3 release

* Mon Dec  6 2000 MANABE Takashi <manabe@dsl.gr.jp>
- add brace/lace functions to libmdnresolv(mdnkit-1.2-runmdn.patch)
- include /var/dnsproxy
- change files section for compressed man pages

* Mon Nov 27 2000 Makoto Ishisone <ishisone@sra.co.jp>
- 1.2 release

* Thu Nov  2 2000 MANABE Takashi <manabe@dsl.gr.jp>
- 1.1 release

* Fri Oct 27 2000 MANABE Takashi <manabe@dsl.gr.jp>
- dnsproxy.patch1
- move libmdnresolv.{la,so} from mdnkit-devel to mdnkit package

* Wed Oct 18 2000 MANABE Takashi <manabe@dsl.gr.jp>
- 1.0 release
