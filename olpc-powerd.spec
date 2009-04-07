Summary: OLPC XO experimental power management
Name: olpc-powerd
Version: 3
Release: 1
License: GPLv2
Group: System Environment/Base
URL: http://dev.laptop.org/git?p=users/pgf/olpc-powerd
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gcc, kernel-headers
Requires: olpc-kbdshim >= 2-2
BuildArch: i386
Provides: olpc-powerd = 3-1

%description
The olpc-powerd can function as an easily customizable replacement
for ohmd, which is independent of X, dbus, and hald.  This package
provides the powerd and olpc-switchd daemons (and related
utilities).  This package

%prep
%setup -q

%build
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/sbin
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/etc/event.d
mkdir -p $RPM_BUILD_ROOT/etc/powerd
mkdir -p $RPM_BUILD_ROOT/etc/powerd/postresume.d

%{__install} -m 755 olpc-switchd $RPM_BUILD_ROOT/usr/sbin/olpc-switchd
%{__install} -m 755 powerd $RPM_BUILD_ROOT/usr/sbin/powerd
%{__install} -m 755 pnmto565fb $RPM_BUILD_ROOT/usr/bin/pnmto565fb
%{__install} -m 755 powerd-config $RPM_BUILD_ROOT/usr/bin/powerd-config
%{__install} -m 755 olpc-brightness $RPM_BUILD_ROOT/usr/bin/olpc-brightness
%{__install} -m 644 olpc-switchd.upstart $RPM_BUILD_ROOT/etc/event.d/olpc-switchd
%{__install} -m 644 powerd.upstart $RPM_BUILD_ROOT/etc/event.d/powerd
%{__install} -m 644 pleaseconfirm.pgm $RPM_BUILD_ROOT/etc/powerd/pleaseconfirm.pgm
%{__install} -m 644 shuttingdown.pgm $RPM_BUILD_ROOT/etc/powerd/shuttingdown.pgm
%{__install} -m 644 powerd.conf.dist $RPM_BUILD_ROOT/etc/powerd/powerd.conf


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc COPYING

/usr/sbin/olpc-switchd
/usr/sbin/powerd
/usr/bin/pnmto565fb
/usr/bin/powerd-config
/usr/bin/olpc-brightness
/etc/event.d/olpc-switchd
/etc/event.d/powerd
/etc/powerd/pleaseconfirm.pgm
/etc/powerd/shuttingdown.pgm
/etc/powerd/powerd.conf

%post
if test -e /etc/init.d/ohmd
then
    /etc/init.d/ohmd stop
    chkconfig ohmd off
fi
initctl start powerd
initctl start olpc-switchd
if test -e /etc/event.d/olpc-kbdshim
then
    sed -i -e 's/#\+ *-A/ -A/' etc/event.d/olpc-kbdshim
    initctl stop olpc-kbdshim
    initctl start olpc-kbdshim
fi

KEYHANDLER=/usr/share/sugar/shell/view/keyhandler.py
if test -e $KEYHANDLER && ! grep -q 'patch v1 by olpc-powerd' $KEYHANDLER
then
    sed -i \
        -e '/    def handle_brightness_max(self):$/a\
\
        # patch v1 by olpc-powerd\
        if os.path.exists("/usr/bin/olpc-brightness"):\
           os.system("/usr/bin/olpc-brightness high")\
           return\
'\
        -e '/    def handle_brightness_min(self):$/a\
\
        # patch v1 by olpc-powerd\
        if os.path.exists("/usr/bin/olpc-brightness"):\
           os.system("/usr/bin/olpc-brightness low")\
           return\
'\
        -e '/    def handle_brightness_up(self):$/a\
\
        # patch v1 by olpc-powerd\
        if os.path.exists("/usr/bin/olpc-brightness"):\
           os.system("/usr/bin/olpc-brightness up")\
           return\
'\
        -e '/    def handle_brightness_down(self):$/a\
\
        # patch v1 by olpc-powerd\
        if os.path.exists("/usr/bin/olpc-brightness"):\
           os.system("/usr/bin/olpc-brightness down")\
           return\
\
' $KEYHANDLER
fi

%preun
if test -e /etc/event.d/olpc-kbdshim
then
    sed -i -e 's/ \+-A \/var/ #&/' /etc/event.d/olpc-kbdshim
    initctl stop olpc-kbdshim
    initctl start olpc-kbdshim
fi
initctl stop olpc-switchd
initctl stop powerd
rm -f /var/run/powerevents
if test -e /etc/init.d/ohmd
then
    /etc/init.d/ohmd start
    chkconfig ohmd on
fi

%changelog
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

