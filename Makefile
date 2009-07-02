# Copyright (C) 2009 Paul G. Fox
# Licensed under the terms of the GNU GPL v2 or later; see COPYING for details.

PACKAGE=olpc-powerd
SPEC=$(PACKAGE).spec

DATETAG=$(shell date +%Y%m%d)
GITHEAD=git$(shell test -d .git && \
    git-show --pretty=format:%h HEAD 2>/dev/null | sed 1q )

ifeq ($(do_rel),)
    SNAP=.$(DATETAG)$(GITHEAD)
endif

VERSION=$(shell sed -n 's/^Version: \([[:digit:]]\+\).*/\1/p' $(SPEC))
RELEASE=$(shell sed -n 's/^Release: \([[:digit:]]\+\).*/\1/p' $(SPEC))
SRELEASE=$(RELEASE)$(SNAP)

TARBALL=$(PKGVER)-$(GITHEAD).tar.gz
SRPM=$(PKGVER)-$(SRELEASE).src.rpm

MOCK=./mock-wrapper -r olpc-3-i386 --resultdir=$(MOCKDIR)
MOCKDIR=./rpms
CWD=$(shell pwd)

PKGVER=$(PACKAGE)-$(VERSION)

# may be set externally to RPM_OPT_FLAGS
OPT_FLAGS ?= -O2 -g

CFLAGS = -Wall $(OPT_FLAGS) -DVERSION=$(VERSION)
PROG1 = olpc-switchd
PROG2 = pnmto565fb

PROGS = $(PROG1) $(PROG2)

all: $(PROGS)

# testing targets
tarball:  update-version $(TARBALL)
srpm: update-version $(SRPM)


distribute: $(TARBALL) $(SRPM) rpms/$(PKGVER)-$(SRELEASE).fc9.i386.rpm
	scp $(TARBALL) $(SRPM)  \
		crank:public_html/rpms/srpms
	scp $(SPEC) \
		crank:public_html/rpms/srpms/$(SPEC)-$(VERSION)-$(SRELEASE)
	scp rpms/$(PKGVER)-$(SRELEASE).fc9.i386.rpm \
		crank:public_html/rpms

privdist:
	scp rpms/$(PKGVER)-$(SRELEASE).fc9.i386.rpm \
		crank:public_html/private_rpms
	ssh crank ln -sf \
		$(PKGVER)-$(SRELEASE).fc9.i386.rpm \
		public_html/private_rpms/$(PKGVER)-$(RELEASE).latest.rpm

# edit the spec (carefully!) so it refers to a) our tarball, and
# b) our prerelease string.
update-version:
	sed -i \
	-e 's/^Release:[^%]*%{?dist}/Release: $(SRELEASE)%{?dist}/' \
	-e 's/\(.*\)git[0-9a-f]\+\(.*\)/\1$(GITHEAD)\2/' \
	$(SPEC)


# build the tarball directly from git.
# THIS MEANS UNCOMMITED CHANGES TO THE MAKEFILE WILL NOT BE INCLUDED!!!

$(TARBALL):
	-git diff --exit-code # working copy is clean?
	-git diff --cached --exit-code # uncommitted changes?
	git archive --format=tar --prefix=$(PKGVER)/ HEAD | gzip -c > $@


# build the SRPM from the spec and the tarball

$(SRPM): $(TARBALL)
	rpmbuild --define "_specdir $(CWD)" \
		 --define "_sourcedir $(CWD)" \
		 --define "_builddir $(CWD)"  \
		 --define "_srcrpmdir $(CWD)"  \
		 --define "_rpmdir $(CWD)"  \
		 --define "dist %nil"  \
		 --nodeps -bs $(SPEC)

# build rpm from the srpm
mock: update-version $(SRPM)
	@mkdir -p $(MOCKDIR)
	$(MOCK) -q --init
	$(MOCK) --installdeps $(SRPM)
	$(MOCK) -v --no-clean --rebuild $(SRPM)

clean:
	rm -f *.o $(PROGS)
	-$(RM) $(SRPM) $(TARBALL)
	-$(RM) -rf $(MOCKDIR)

.PHONY: tarball srpm mock
