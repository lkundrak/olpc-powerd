# Copyright (C) 2009 Paul G. Fox
# Licensed under the terms of the GNU GPL v2 or later; see COPYING for details.

VERSION=1
PACKAGE=olpc-switchd
MOCK=./mock-wrapper -r olpc-3-i386 --resultdir=$(MOCKDIR)
MOCKDIR=./rpms
PKGVER=$(PACKAGE)-$(VERSION)
CWD=$(shell pwd)

# --- simple rules for local make -------
CFLAGS = -Wall -O2 -g -DVERSION=$(VERSION)
PROG = olpc-switchd

all: $(PROG)

OBJS =  $(PROG).o

clean:
        rm -f *.o $(PROG)
