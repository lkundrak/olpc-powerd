Summary: OLPC XO experimental power management
Name: olpc-powerd
Version: 1
Release: 1
License: GPL
Group: System Environment/Base
URL: http://dev.laptop.org/git?p=users/pgf/olpc-powerd
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gcc, kernel-headers
BuildArch: i386
Provides: olpc-powerd

%description
The olpc-powerd can function as an easily customizable replacement
for ohmd, which is independent of X, dbus, and hald.  This package
provides the powerd and olpc-switchd daemons (and related
utilities).  This package

%prep
%setup

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/etc/event.d
mkdir -p $RPM_BUILD_ROOT/etc/powerd

%{__install} -m 755 olpc-switchd $RPM_BUILD_ROOT/usr/bin/olpc-switchd
%{__install} -m 755 powerd $RPM_BUILD_ROOT/usr/bin/powerd
%{__install} -m 755 pnmto565fb $RPM_BUILD_ROOT/usr/bin/pnmto565fb
%{__install} -m 644 olpc-switchd.upstart $RPM_BUILD_ROOT/etc/event.d/olpc-switchd
%{__install} -m 644 powerd.upstart $RPM_BUILD_ROOT/etc/event.d/powerd
%{__install} -m 644 pleaseconfirm.pgm $RPM_BUILD_ROOT/etc/powerd/pleaseconfirm.pgm
%{__install} -m 644 shuttingdown.pgm $RPM_BUILD_ROOT/etc/powerd/shuttingdown.pgm
%{__install} -m 644 powerd.conf $RPM_BUILD_ROOT/etc/powerd/powerd.conf


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc COPYING

/usr/bin/olpc-switchd
/usr/bin/powerd
/usr/bin/pnmto565fb
/etc/event.d/olpc-switchd
/etc/event.d/powerd
/etc/powerd/pleaseconfirm.pgm
/etc/powerd/shuttingdown.pgm
/etc/powerd/powerd.conf

%post
chkconfig ohmd off
/etc/init.d/ohmd stop
initctl start powerd
initctl start olpc-switchd

%changelog
