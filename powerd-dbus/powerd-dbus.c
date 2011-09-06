#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dbus/dbus-glib-bindings.h>
#include <glib.h>

#include "powerd-dbus.h"

DBusGConnection *dbus_conn;
static DBusGProxy *driver_proxy;
static GMainLoop *mainloop;

void powerd_send_event(const char *event, const char *arg)
{
	int fd, n;
	char evtbuf[128];
	int ignore;

	g_message("powerd_send_event %s", event);
	fd = open("/var/run/powerevents", O_RDWR | O_NONBLOCK);
	if (fd == -1)
		return;

	if (arg)
		n = snprintf(evtbuf, sizeof(evtbuf), "%s %d %s\n", event, (int)time(0), arg);
	else
		n = snprintf(evtbuf, sizeof(evtbuf), "%s %d\n", event, (int)time(0));

	ignore = write(fd, evtbuf, n);
	close(fd);
}

int main(void)
{
	GError *error = NULL;
	guint32 request_name_ret;

	g_type_init();
	mainloop = g_main_loop_new(NULL, FALSE);

	dbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
	if (dbus_conn == NULL)
		g_error("Failed to open connection to bus: %s", error->message);

	driver_proxy = dbus_g_proxy_new_for_name(dbus_conn,
		DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	if (!org_freedesktop_DBus_request_name(driver_proxy, OHM_DBUS_SERVICE,
			0, &request_name_ret, &error))
		g_error("Failed to get name: %s", error->message);

	if (request_name_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		g_error ("Got result code %u from requesting name", request_name_ret);
		exit(1);
	}

	ohm_keystore_new();
	wpas_monitor_init();
	nm_monitor_init();

	g_message("entering main loop");
	g_main_loop_run(mainloop);
	g_message("main loop completed");
	return 0;
}

