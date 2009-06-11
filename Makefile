# Copyright (C) 2009 Paul G. Fox
# Licensed under the terms of the GNU GPL v2 or later; see COPYING for details.

ifeq ($(do_rel),)
    DATETAG=$(shell date +%Y%m%d)
    GITHEAD=git$(shell test -d .git && \
    	git-show --pretty=format:%h HEAD 2>/dev/null | sed 1q )
    SNAP=.$(DATETAG)$(GITHEAD)
endif

VERSION=6
RELEASE=2
SRELEASE=$(RELEASE)$(SNAP)

PACKAGE=olpc-powerd
MOCK=./mock-wrapper -r olpc-3-i386 --resultdir=$(MOCKDIR)
MOCKDIR=./rpms
PKGVER=$(PACKAGE)-$(VERSION)
CWD=$(shell pwd)
PKGVER=$(PACKAGE)-$(VERSION)

OPT_FLAGS ?= -O2 -g

# --- rules for local make -------

CFLAGS = -Wall $(OPT_FLAGS) -DVERSION=$(VERSION)
PROG1 = olpc-switchd
PROG2 = pnmto565fb

all: $(PROG1) $(PROG2)

clean:
	rm -f *.o $(PROG1) $(PROG2)

distribute: $(PKGVER).tar.gz $(PKGVER)-$(SRELEASE).src.rpm rpms/$(PKGVER)-$(SRELEASE).*.i386.rpm
	scp $(PKGVER).tar.gz $(PKGVER)-$(SRELEASE).src.rpm  \
		crank:public_html/rpms/srpms
	scp $(PACKAGE).spec \
		crank:public_html/rpms/srpms/$(PACKAGE).spec-$(VERSION)-$(SRELEASE)
	scp rpms/$(PKGVER)-$(SRELEASE).*.i386.rpm \
		crank:public_html/rpms

privdist:
	scp rpms/$(PKGVER)-$(SRELEASE).*.i386.rpm \
		crank:public_html/private_rpms
	ssh crank ln -sf \
		$(PKGVER)-$(SRELEASE).fc9.i386.rpm \
		public_html/private_rpms/$(PKGVER)-$(RELEASE).latest.rpm

# mock-ish rules for building an rpm
# (leveraged from cscott's olpc-update makefile)

update-version:
	sed -i \
	-e 's/^Version: [[:digit:]]\+/Version: $(VERSION)/' \
	-e 's/^Release:[^%]*/Release: $(SRELEASE)/' \
	$(PACKAGE).spec

tarball:  update-version $(PKGVER).tar.gz

$(PKGVER).tar.gz:
	-git diff --exit-code # working copy is clean?
	-git diff --cached --exit-code # uncommitted changes?
	git archive --format=tar --prefix=$(PKGVER)/ HEAD | gzip -c > $@

.PHONY: $(PKGVER).tar.gz # force refresh

# builds SRPM and RPMs in an appropriate chroot, leaving the results in
# the $(MOCKDIR) subdirectory.

# make the SRPM.
srpm: update-version $(PKGVER)-$(SRELEASE).src.rpm

$(PKGVER)-$(SRELEASE).src.rpm: $(PKGVER).tar.gz
	rpmbuild --define "_specdir $(CWD)" \
		 --define "_sourcedir $(CWD)" \
		 --define "_builddir $(CWD)"  \
		 --define "_srcrpmdir $(CWD)"  \
		 --define "_rpmdir $(CWD)"  \
		 --define "dist %nil"  \
		 --nodeps -bs $(PACKAGE).spec

# build RPMs from the SRPM
mock: update-version $(PKGVER)-$(SRELEASE).src.rpm
	@mkdir -p $(MOCKDIR)
	$(MOCK) -q --init
	$(MOCK) --installdeps $(PKGVER)-$(SRELEASE).src.rpm
	$(MOCK) -v --no-clean --rebuild $(PKGVER)-$(SRELEASE).src.rpm

rpmclean:
	-$(RM) $(PKGVER)-[12345].src.rpm $(PKGVER).tar.gz
	-$(RM) -rf $(MOCKDIR)
