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

int nm_monitor_init(void);
int wpas_monitor_init(void);
void nm_suspend_ok(gboolean suspend_ok);
void wpas_suspend_ok(gboolean suspend_ok);

void powerd_send_event(const char *event, const char *arg);

#endif

