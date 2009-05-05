Summary: OLPC XO power management
Name: olpc-powerd
Version: 5
Release: 1
License: GPLv2+
Group: System Environment/Base
URL:  http://dev.laptop.org/git/users/pgf/powerd/tree/powerd
# Source0: the source tarball is created by "make tarball" from within
# a clone of this git tree: git://dev.laptop.org/users/pgf/powerd
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: kernel-headers
Requires: olpc-kbdshim >= 2-2
BuildArch: i386

%description
The powerd daemon can function as an easily customizable
replacement for ohmd, which is independent of X, dbus, and hald. 
This package provides the powerd and olpc-switchd daemons, and
related utilities.

%prep
%setup -q

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_sbindir}
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/event.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/powerd
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/powerd/postresume.d

%{__install} -m 755 olpc-switchd $RPM_BUILD_ROOT/%{_sbindir}/olpc-switchd
%{__install} -m 755 powerd $RPM_BUILD_ROOT/%{_sbindir}/powerd
%{__install} -m 755 pnmto565fb $RPM_BUILD_ROOT/%{_bindir}/pnmto565fb
%{__install} -m 755 powerd-config $RPM_BUILD_ROOT/%{_bindir}/powerd-config
%{__install} -m 755 olpc-brightness $RPM_BUILD_ROOT/%{_bindir}/olpc-brightness
%{__install} -m 644 olpc-switchd.upstart $RPM_BUILD_ROOT%{_sysconfdir}/event.d/olpc-switchd
%{__install} -m 644 powerd.upstart $RPM_BUILD_ROOT%{_sysconfdir}/event.d/powerd
%{__install} -m 644 pleaseconfirm.pgm $RPM_BUILD_ROOT%{_sysconfdir}/powerd/pleaseconfirm.pgm
%{__install} -m 644 shuttingdown.pgm $RPM_BUILD_ROOT%{_sysconfdir}/powerd/shuttingdown.pgm
%{__install} -m 644 powerd.conf.dist $RPM_BUILD_ROOT%{_sysconfdir}/powerd/powerd.conf


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc COPYING

%{_sbindir}/olpc-switchd
%{_sbindir}/powerd
%{_bindir}/pnmto565fb
%{_bindir}/powerd-config
%{_bindir}/olpc-brightness
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

