# Copyright (C) 2009 Paul G. Fox
# Licensed under the terms of the GNU GPL v2 or later; see COPYING for details.

VERSION=4
RELEASE=1
PACKAGE=olpc-powerd
MOCK=./mock-wrapper -r olpc-3-i386 --resultdir=$(MOCKDIR)
MOCKDIR=./rpms
PKGVER=$(PACKAGE)-$(VERSION)
CWD=$(shell pwd)

# --- simple rules for local make -------
CFLAGS = -Wall -O2 -g -DVERSION=$(VERSION)
PROG1 = olpc-switchd
PROG2 = pnmto565fb

all: $(PROG1) $(PROG2)

clean:
	rm -f *.o $(PROG1) $(PROG2)

release: tarball srpm rpms/$(PKGVER)-$(RELEASE).src.rpm
	scp $(PKGVER).tar.gz $(PKGVER)-$(RELEASE).src.rpm $(PACKAGE).spec \
		crank:public_html/rpms/srpms
	scp rpms/$(PKGVER)-$(RELEASE).src.rpm \
		crank:public_html/rpms

# mock-ish rules for building an rpm
# (leveraged from cscott's olpc-update makefile)

update-version:
	sed -i \
	-e 's/^Version: .*/Version: $(VERSION)/' \
	-e 's/^Release: .*/Release: $(RELEASE)/' \
	-e 's/^Provides: .*/Provides: $(PACKAGE) = $(VERSION)-$(RELEASE)/' \
	$(PACKAGE).spec

tarball: $(PKGVER).tar.gz

$(PKGVER).tar.gz:  update-version
	git diff --shortstat --exit-code # working copy is clean?
	git diff --cached --shortstat --exit-code # uncommitted changes?
	git archive --format=tar --prefix=$(PKGVER)/ HEAD | gzip -c > $@

.PHONY: $(PKGVER).tar.gz # force refresh

# builds SRPM and RPMs in an appropriate chroot, leaving the results in
# the $(MOCKDIR) subdirectory.

# make the SRPM.
srpm: $(PKGVER)-$(RELEASE).src.rpm

$(PKGVER)-$(RELEASE).src.rpm: $(PKGVER).tar.gz
	rpmbuild --define "_specdir $(CWD)" \
		 --define "_sourcedir $(CWD)" \
		 --define "_builddir $(CWD)"  \
		 --define "_srcrpmdir $(CWD)"  \
		 --define "_rpmdir $(CWD)"  \
		 --define "dist %nil"  \
		 --nodeps -bs $(PACKAGE).spec

# build RPMs from the SRPM
mock:	$(PKGVER)-$(RELEASE).src.rpm
	@mkdir -p $(MOCKDIR)
	$(MOCK) -q --init
	#$(MOCK) init
	$(MOCK) --installdeps $(PKGVER)-$(RELEASE).src.rpm
	$(MOCK) -v --no-clean --rebuild $(PKGVER)-$(RELEASE).src.rpm

rpmclean:
	-$(RM) $(PKGVER)-[12345].src.rpm $(PKGVER).tar.gz
	-$(RM) -rf $(MOCKDIR)
