DBUS_GLIB_CFLAGS=$(shell pkg-config --cflags --libs dbus-glib-1)
GIO_CFLAGS=$(shell pkg-config --cflags --libs gio-2.0)
LIBNM_CFLAGS=$(shell pkg-config --cflags --libs libnm)

ifdef WPA_SUPPLICANT_0_6
CFLAGS += -DWPA_SUPPLICANT_0_6
endif

CFLAGS += -DG_LOG_DOMAIN=\"powerd-dbus\"

all: powerd-dbus

powerd-dbus: ohm-keystore-glue.h ohm-keystore.c network.c nm_monitor.c wpas_monitor.c powerd-dbus.c powerd-dbus.h
	opt=-combine; ln -sf /dev/null null.c; \
	gcc -flto -c null.c 2>/dev/null && opt=-flto ; \
	gcc $(CFLAGS) -fwhole-program $$opt \
	    $(LIBNM_CFLAGS) $(GIO_CFLAGS) $(DBUS_GLIB_CFLAGS) $(filter %.c,$^) -o $@

ohm-keystore-glue.h: ohm-keystore.xml
	dbus-binding-tool --prefix=ohm_keystore --mode=glib-server $< --output=$@

clean:
	rm -f powerd-dbus ohm-keystore-glue.h null.c null.o

