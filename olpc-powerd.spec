Summary: OLPC XO power management
Name: olpc-powerd
Version: 6
Release: 2.20090611gitb48a011%{?dist}
License: GPLv2+
Group: System Environment/Base
URL:  http://dev.laptop.org/git/users/pgf/powerd/tree/powerd
# Source0: the source tarball is created by "make tarball" from within
# a clone of this git tree: git://dev.laptop.org/users/pgf/powerd
Source0: %{name}-%{version}-gitb48a011.tar.gz
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires: kernel-headers
Requires: olpc-kbdshim >= 2-2
ExclusiveArch: i386

%description
The powerd daemon can function as an easily customizable replacement for ohmd,
which is independent of X, dbus, and hald.  This package provides the powerd
and olpc-switchd daemons, and related utilities.

%prep
%setup -q

%build
export OPT_FLAGS="$RPM_OPT_FLAGS"
make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_sbindir}
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/event.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/powerd
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/powerd/postresume.d

%{__install} -p -m 755 olpc-switchd $RPM_BUILD_ROOT/%{_sbindir}/olpc-switchd
%{__install} -p -m 755 powerd $RPM_BUILD_ROOT/%{_sbindir}/powerd
%{__install} -p -m 755 pnmto565fb $RPM_BUILD_ROOT/%{_bindir}/pnmto565fb
%{__install} -p -m 755 powerd-config $RPM_BUILD_ROOT/%{_bindir}/powerd-config
%{__install} -p -m 644 olpc-switchd.upstart $RPM_BUILD_ROOT%{_sysconfdir}/event.d/olpc-switchd
%{__install} -p -m 644 powerd.upstart $RPM_BUILD_ROOT%{_sysconfdir}/event.d/powerd
%{__install} -p -m 644 pleaseconfirm.pgm $RPM_BUILD_ROOT%{_sysconfdir}/powerd/pleaseconfirm.pgm
%{__install} -p -m 644 shuttingdown.pgm $RPM_BUILD_ROOT%{_sysconfdir}/powerd/shuttingdown.pgm
%{__install} -p -m 644 powerd.conf.dist $RPM_BUILD_ROOT%{_sysconfdir}/powerd/powerd.conf


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc COPYING
%config(noreplace) %{_sysconfdir}/powerd/powerd.conf
%config(noreplace) %{_sysconfdir}/event.d/powerd
%config(noreplace) %{_sysconfdir}/event.d/olpc-switchd
%config(noreplace) %{_sysconfdir}/powerd/pleaseconfirm.pgm
%config(noreplace) %{_sysconfdir}/powerd/shuttingdown.pgm

%{_sbindir}/olpc-switchd
%{_sbindir}/powerd
%{_bindir}/pnmto565fb
%{_bindir}/powerd-config
%{_sysconfdir}/event.d/olpc-switchd
%{_sysconfdir}/event.d/powerd
%{_sysconfdir}/powerd/pleaseconfirm.pgm
%{_sysconfdir}/powerd/shuttingdown.pgm
%{_sysconfdir}/powerd/powerd.conf

%post
if test -e /etc/init.d/ohmd
then
    /etc/init.d/ohmd stop
    chkconfig ohmd off
fi
initctl start powerd
initctl start olpc-switchd

%preun

initctl stop olpc-switchd
initctl stop powerd
rm -f /var/run/powerevents
if test -e /etc/init.d/ohmd
then
    /etc/init.d/ohmd start
    chkconfig ohmd on
fi

%changelog
* Thu Jun 11 2009 Paul Fox <pg@laptop.org>
- 6-2
- utility targets in makefile

* Sat Jun 6 2009 Paul Fox <pg@laptop.org>
- 6-1
- various fixes
- incorporate lessons from kbdshim review

* Tue May 5 2009 Paul Fox <pgf@laptop.org>
- 5-1
- fixed ability to shut down with backlight off.  oops.
- various utility bug fixes (powerd-config, olpc-brightness)

* Sun Apr 12 2009 Paul Fox <pgf@laptop.org>
- 4-1
- add control over sleep on lid-close
- resync version numbers

* Sat Apr 11 2009 Paul Fox <pgf@laptop.org>
- 3-3
- fixed powerd-config behavior wrt symlinked configs

* Fri Apr 10 2009 Paul Fox <pgf@laptop.org>
- 3-2
- fix bugs, implement cpu idleness check, and add suspend inhibit mechanism

* Tue Apr 7 2009 Paul Fox <pgf@laptop.org>
- 3-1
- convert to HAL-based operation

* Thu Mar 19 2009 Paul Fox <pgf@laptop.org
- 2-3
- removed extra dcon calls.

* Thu Mar 19 2009 Paul Fox <pgf@laptop.org
- 2-2
- bug fixing

* Tue Mar 17 2009 Paul Fox <pgf@laptop.org
- 2-1 
- added powerd-config, and added blank-or-shutdown after sleep
  capability

* Fri Mar 13 2009 Paul Fox <pgf@laptop.org>
- 1-2
- fix rpmlint errors, move daemons to /usr/sbin

