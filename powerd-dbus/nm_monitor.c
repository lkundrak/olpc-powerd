#include <errno.h>
#include <glib.h>
#include <NetworkManager.h>
#include "powerd-dbus.h"

static NMClient *client;
static GHashTable *monitored_devices;

typedef struct DeviceHandle {
	gulong signal_handler; // FIXME use disconnect_by_func no need to store
	NMDeviceState state;
} DeviceHandle;

static void check_device_state(NMDevice *device, DeviceHandle *handle, gboolean *suspend_ok)
{
	if (handle->state >= NM_DEVICE_STATE_PREPARE &&
		handle->state < NM_DEVICE_STATE_ACTIVATED) {
		g_message("%d: device %p state %d inhibits suspend", (int)time(0),
			device, handle->state);
		*suspend_ok = FALSE;
	}
}

static void evaluate_devices(void)
{
	gboolean suspend_ok = TRUE;
	g_hash_table_foreach(monitored_devices, (GHFunc) check_device_state,
		&suspend_ok);
	nm_suspend_ok(suspend_ok);
}

static void device_state_changed(NMDevice *device, GParamSpec *pspec, DeviceHandle *handle)
{
	handle->state = nm_device_get_state(device);
	g_message("%d: Device %p state changed: %d", (int)time(0), device, handle->state);
	evaluate_devices();
}

static void monitor_device(NMDevice *device)
{
	DeviceHandle *handle;

	if (g_hash_table_lookup(monitored_devices, device))
		return; /* paranoia */

	g_message("%d: Monitoring wifi device %p", (int)time(0), device);
	handle = g_new(DeviceHandle, 1);
	handle->signal_handler = g_signal_connect(device, "notify::state",
		(GCallback) device_state_changed, handle);
	handle->state = nm_device_get_state(device);

	g_hash_table_insert(monitored_devices, device, handle);
	evaluate_devices();
}

static void device_added_cb(NMClient *client, NMDevice *device, gpointer user_data)
{
	g_message("%d: NM device-added: %s", (int)time(0), nm_device_get_iface(device));
	if (NM_IS_DEVICE_WIFI(device))
		monitor_device(device);
}

static void device_removed_cb(NMClient *client, NMDevice *device, gpointer user_data)
{
	DeviceHandle *handle;

	g_message("%d: NM device-removed: %s", (int)time(0), nm_device_get_iface(device));
	if (!NM_IS_DEVICE_WIFI(device))
		return;

	handle = g_hash_table_lookup(monitored_devices, device);
	if (!handle)
		return;

	g_message("%d: Disconnecting from wifi device %p", (int)time(0), device);
	g_signal_handler_disconnect(device, handle->signal_handler);
	g_hash_table_remove(monitored_devices, device);
	g_free(handle);

	evaluate_devices();
}

static void examine_device(NMDevice *device, gpointer unused)
{
	g_message("%d: Examine device: %s", (int)time(0), nm_device_get_iface(device));
	if (NM_IS_DEVICE_WIFI(device))
		monitor_device(device);
}

int nm_monitor_init(void)
{
	const GPtrArray *devices;
	monitored_devices = g_hash_table_new(NULL, NULL);

	client = nm_client_new(NULL, NULL);
	if (!client)
		return -EINVAL;

	g_signal_connect(client, "device-added", (GCallback) device_added_cb, NULL);
	g_signal_connect(client, "device-removed", (GCallback) device_removed_cb,
		NULL);
	devices = nm_client_get_devices(client);
	if (devices)
		g_ptr_array_foreach((GPtrArray *) devices, (GFunc) examine_device,
			NULL);
	return 0;
}
