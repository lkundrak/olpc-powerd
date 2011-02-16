DBUS_GLIB_CFLAGS=$(shell pkg-config --cflags --libs dbus-glib-1)

all: powerd-dbus

powerd-dbus: ohm-keystore-glue.h ohm-keystore.c powerd-dbus.c powerd-dbus.h
	opt=-combine; ln -sf /dev/null null.c; \
	gcc -flto -c null.c 2>/dev/null && opt=-flto ; \
	gcc $(CFLAGS) -fwhole-program $$opt \
	    $(DBUS_GLIB_CFLAGS) $(filter %.c,$^) -o $@

ohm-keystore-glue.h: ohm-keystore.xml
	dbus-binding-tool --prefix=ohm_keystore --mode=glib-server $< --output=$@

clean:
	rm -f powerd-dbus ohm-keystore-glue.h

