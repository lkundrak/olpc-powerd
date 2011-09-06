#include <glib.h>
#include <gio/gio.h>
#include <errno.h>
#include <stdio.h>

#include "powerd-dbus.h"

/*
 * Not implemented for wpa_supplicant-0.6.
 * Although "Scanning" signals are emitted to indicate state, there are no
 * device-added/device-removed signals, making accurate tracking difficult.
 * For v0.6, rely on the nm_monitor code to do a good-enough job, and wait
 * for the v0.7 upgrade to improve this situation.
 */
int wpas_monitor_init(void)
{
	return 0;
}
#if 0
/*
 * FIXME: the following will work for wpa_supplicant-0.7 when
 * http://lists.shmoo.com/pipermail/hostap/2011-August/023733.html gets applied
 * Disable for now.
 */

#define WPAS_SERVICE "fi.w1.wpa_supplicant1"
#define WPAS_IFACE "fi.w1.wpa_supplicant1"
#define WPAS_PATH "/fi/w1/wpa_supplicant1"

#define WPAS_INTERFACE_IFACE "fi.w1.wpa_supplicant1.Interface"

static GDBusProxy *wpas_obj = NULL;
static GSList *monitored_ifaces = NULL;

static GDBusProxy *find_monitored_iface_by_path(const gchar *path)
{
	GSList *list;
	for (list = monitored_ifaces; list; list = g_slist_next(list)) {
		GDBusProxy *proxy = (GDBusProxy *) list->data;
		const char *obj_path = g_dbus_proxy_get_object_path(proxy);
		if (strcmp(obj_path, path) == 0)
			return proxy;
	}
	return NULL;
}

static void evaluate_interfaces(void)
{
	gboolean suspend_ok = TRUE;
	GSList *list;

	for (list = monitored_ifaces; list; list = g_slist_next(list)) {
		GDBusProxy *proxy = (GDBusProxy *) list->data;
		GVariant *val = g_dbus_proxy_get_cached_property(proxy, "Scanning");
		gboolean scanning;

		if (!val)
			continue; /* should never happen */

		scanning = g_variant_get_boolean(val);
		g_variant_unref(val);

		if (scanning) {
			g_message("%s is scanning; inhibit suspend",
				g_dbus_proxy_get_object_path(proxy));
			suspend_ok = FALSE;
			break;
		}
	}

	wpas_suspend_ok(suspend_ok);
}

static void iface_props_changed(GDBusProxy *proxy, GVariant *changes,
	GStrv *invalidated, gpointer data)
{
	GVariantIter *iter;
	GVariant *value;
	const gchar *key;
	gboolean scanning_changed = FALSE;

	g_message("iface props changed %s", g_dbus_proxy_get_object_path(proxy));

	if (!g_variant_n_children(changes))
		return;

	g_variant_get(changes, "a{sv}", &iter);
	while (g_variant_iter_loop(iter, "{&sv}", &key, &value)) {
		if (strcasecmp(key, "Scanning") == 0)
			scanning_changed = TRUE;
		/*
		 * loop must run to completion so that memory is freed, see
		 * g_variant_iter_loop docs
		 */
	}

	if (scanning_changed)
		evaluate_interfaces();
}

static void monitor_interface(const gchar *path)
{
	GError *error = NULL;
	GDBusProxy *proxy;

	if (find_monitored_iface_by_path(path))
		return; /* already monitoring? should never happen */

	proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
		0, NULL,
		WPAS_SERVICE, path, WPAS_INTERFACE_IFACE,
		NULL, &error);
	if (!wpas_obj) {
		g_warning("Error creating wpas interface proxy: %s\n", error->message);
		g_error_free(error);
		return;
	}

	g_signal_connect(proxy, "g-properties-changed",
		(GCallback) iface_props_changed, NULL);

	g_message("Monitoring wpas interface: %s", path);
	monitored_ifaces = g_slist_prepend(monitored_ifaces, proxy);
	evaluate_interfaces();
}

static void remove_interface(const gchar *path)
{
	GDBusProxy *proxy = find_monitored_iface_by_path(path);
	if (!proxy)
		return;

	g_message("Disconnecting from %s", path);
	g_signal_handlers_disconnect_by_func(proxy, iface_props_changed, NULL);
	monitored_ifaces = g_slist_remove(monitored_ifaces, proxy);
	g_object_unref(proxy);
	evaluate_interfaces();
}

static void signal_handler(GDBusProxy *proxy, gchar *sender, gchar *signal,
	GVariant *parameters, gpointer data)
{
	GVariant *child;
	const gchar *path;

	if (strcmp(signal, "InterfaceAdded") != 0
			&& strcmp(signal, "InterfaceRemoved") != 0)
		return;

	if (!g_variant_n_children(parameters))
		return;

	child = g_variant_get_child_value(parameters, 0);
	path = g_variant_get_string(child, NULL);

	g_message("wpa_supplicant %s %s", signal, path);

	if (strcmp(signal, "InterfaceAdded") == 0)
		monitor_interface(path);
	else
		remove_interface(path);
}

static void examine_interfaces(GVariant *interfaces_v)
{
	const gchar **interfaces;
	GVariant *child;
	gsize i;
	gsize len = g_variant_n_children(interfaces_v);

	for (i = 0; i < len; i++) {
		GVariant *child = g_variant_get_child_value(interfaces_v, i);
		const gchar *interface = g_variant_get_string(child, NULL);
		g_message("Found wpas interface: %s", interface);
		monitor_interface(interface);
	}
}

int wpas_monitor_init(void)
{
	GError *error = NULL;
	GVariant *interfaces;
	int i;

	wpas_obj = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, NULL,
		WPAS_SERVICE, WPAS_PATH, WPAS_IFACE,
		NULL, &error);
	if (!wpas_obj) {
		g_printerr("Error creating wpas proxy: %s\n", error->message);
		g_error_free(error);
		return -EIO;
	}

	g_signal_connect(wpas_obj, "g-signal", (GCallback) signal_handler, NULL);

	interfaces = g_dbus_proxy_get_cached_property(wpas_obj, "Interfaces");
	if (!interfaces)
		g_error("Failed to list wpa_supplicant interfaces");
	examine_interfaces(interfaces);
	g_variant_unref(interfaces);
}
#endif
