#ifndef __POWERD_DBUS_H
#define __POWERD_DBUS_H

#include <dbus/dbus-glib-bindings.h>

extern DBusGConnection *dbus_conn;

#define	OHM_DBUS_SERVICE		"org.freedesktop.ohm"
#define	OHM_DBUS_INTERFACE_KEYSTORE	"org.freedesktop.ohm.Keystore"
#define	OHM_DBUS_PATH_KEYSTORE		"/org/freedesktop/ohm/Keystore"
struct OhmKeystore;
typedef struct OhmKeystore OhmKeystore;
OhmKeystore *ohm_keystore_new(void);

void powerd_send_event(const char *event);

#endif

