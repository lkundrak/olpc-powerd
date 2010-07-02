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

void powerd_send_event(const char *event)
{
	int fd, n;
	char evtbuf[128];

	fd = open("/var/run/powerevents", O_RDWR | O_NONBLOCK);
	if (fd == -1)
		return;

	n = snprintf(evtbuf, sizeof(evtbuf), "%s %d\n", event, (int)time(0));
	write(fd, evtbuf, n);
	close(fd);
}

/* maximum runtime of 30 seconds, unless we're busy doing something at
 * that point. dbus will launch us again if needed. */
static gboolean exit_timeout(gpointer data)
{
	GError *error = NULL;
	guint32 ret;

	if (g_main_context_pending(NULL))
		return TRUE;

	org_freedesktop_DBus_release_name(driver_proxy, OHM_DBUS_SERVICE, &ret,
		&error);
	g_message("exit after timeout");
	g_main_loop_quit(mainloop);
	return FALSE;
}

int main(void)
{
	GError *error = NULL;
	guint32 request_name_ret;

	g_type_init();
	mainloop = g_main_loop_new(NULL, FALSE);

	g_timeout_add_seconds(30, exit_timeout, NULL);

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

	g_message("entering main loop");
	g_main_loop_run(mainloop);
	g_message("main loop completed");
	return 0;
}

