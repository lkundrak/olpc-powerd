# Copyright (C) 2009 Paul G. Fox
# Licensed under the terms of the GNU GPL v2 or later; see COPYING for details.

PACKAGE=olpc-powerd
PACKAGEDBUS=olpc-powerd-dbus
VERSION=37


# don't edit .spec -- edit .spec.tmpl
SPEC=$(PACKAGE).spec

DATETAG=$(shell date +%Y%m%d)
GITHEAD=git$(shell test -d .git && git rev-parse --short HEAD )

ifeq ($(do_rel),)
    SNAP=.$(DATETAG)$(GITHEAD)
endif


RELEASE=$(shell cat .spec_release 2>/dev/null || echo error)
SRELEASE=$(RELEASE)$(SNAP)

TARBALL=$(PKGVER)-$(GITHEAD).tar.gz
SRPM=$(PKGVER)-$(SRELEASE).src.rpm

MOCK=./mock-wrapper -r fedora-14-i386 --resultdir=$(MOCKDIR)
MOCKDIR=./rpms
CWD=$(shell pwd)

PKGVER=$(PACKAGE)-$(VERSION)
PKGVERDBUS=$(PACKAGEDBUS)-$(VERSION)

# may be set externally to RPM_OPT_FLAGS
OPT_FLAGS ?= -O2 -g

#####

PROGS = olpc-switchd pnmto565fb

export CFLAGS = -Wall $(OPT_FLAGS) -DVERSION=$(VERSION)

#####

all: version $(PROGS) powerd-dbus

powerd-dbus:
	$(MAKE) -C $@
.PHONY: powerd-dbus

olpc-switchd: olpc-switchd.c
	$(CC) $(CFLAGS) -fwhole-program $^ -o $@

pnmto565fb: pnmto565fb.c
	$(CC) $(CFLAGS) -fwhole-program $^ -o $@

# testing targets
tarball:  $(TARBALL)
srpm: $(SRPM)

version: Makefile
	@echo "powerd_version='version $(VERSION)'" >version

src_distribute: $(TARBALL) $(SRPM)
	scp $(TARBALL) $(SRPM)  \
		crank:public_html/rpms/srpms
#	scp $(SPEC) \
#		crank:public_html/rpms/srpms/$(SPEC)-$(VERSION)-$(SRELEASE)
distribute: src_distribute rpms/$(PKGVER)-$(SRELEASE).fc11.i586.rpm
	scp rpms/$(PKGVER)-$(SRELEASE).fc11.i586.rpm \
		rpms/$(PKGVERDBUS)-$(SRELEASE).fc11.i586.rpm \
		crank:public_html/rpms

privdist:
	scp rpms/$(PKGVER)-$(SRELEASE).fc11.i586.rpm \
		rpms/$(PKGVERDBUS)-$(SRELEASE).fc11.i586.rpm \
		crank:public_html/private_rpms
	ssh crank ln -sf \
		$(PKGVER)-$(SRELEASE).fc11.i586.rpm \
		public_html/private_rpms/$(PKGVER)-$(RELEASE).latest.rpm
	ssh crank ln -sf \
		$(PKGVERDBUS)-$(SRELEASE).fc11.i586.rpm \
		public_html/private_rpms/$(PKGVERDBUS)-$(RELEASE).latest.rpm

# create the real spec (carefully!) so it refers to a) our tarball, and
# b) our prerelease string.
$(SPEC): ALWAYS
	sed \
	-e 's/__VERSION__/$(VERSION)/' \
	-e 's/__RELEASE__/$(SRELEASE)/' \
	-e 's/__TARBALL__/$(TARBALL)/' \
	$(SPEC).tmpl > $(SPEC)


# build the tarball directly from git.
# THIS MEANS NO UNCOMMITED CHANGES WILL BE INCLUDED!!!

$(TARBALL):
	-git diff --exit-code # working copy is clean?
	-git diff --cached --exit-code # uncommitted changes?
	git archive --format=tar --prefix=$(PKGVER)/ HEAD | gzip -c > $@


# build the SRPM from the spec and the tarball

$(SRPM): $(SPEC) $(TARBALL)
	rpmbuild --define "_specdir $(CWD)" \
		 --define "_sourcedir $(CWD)" \
		 --define "_builddir $(CWD)"  \
		 --define "_srcrpmdir $(CWD)"  \
		 --define "_rpmdir $(CWD)"  \
		 --define "dist %nil"  \
		 --nodeps -bs $(SPEC)

# build rpm from the srpm
mock: version $(SRPM)
	@mkdir -p $(MOCKDIR)
	$(MOCK) -q --init
	$(MOCK) --installdeps $(SRPM)
	$(MOCK) -v --no-clean --rebuild $(SRPM)

clean:
	rm -f *.o $(PROGS)
	-$(RM) $(SRPM) $(TARBALL)
	-$(RM) -rf $(MOCKDIR)
	make -C powerd-dbus clean

.PHONY: tarball srpm mock ALWAYS
