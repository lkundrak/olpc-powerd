#include <string.h>
#include <glib.h>

#include "powerd-dbus.h"

struct OhmKeystore {
	GObject parent;
};

typedef struct OhmKeystoreClass {
	GObjectClass parent_class;
} OhmKeystoreClass;

typedef enum
{
	 OHM_KEYSTORE_ERROR_INVALID,
	 OHM_KEYSTORE_ERROR_KEY_MISSING,
	 OHM_KEYSTORE_ERROR_KEY_OVERRIDE,
	 OHM_KEYSTORE_ERROR_KEY_LAST
} OhmKeystoreError;

#define OHM_TYPE_KEYSTORE			(ohm_keystore_get_type())
#define OHM_KEYSTORE(o)				(G_TYPE_CHECK_INSTANCE_CAST((o), OHM_TYPE_KEYSTORE, OhmKeystore))
#define OHM_KEYSTORE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), OHM_TYPE_KEYSTORE, OhmKeystoreClass))
#define OHM_IS_KEYSTORE(o)			(G_TYPE_CHECK_INSTANCE_TYPE((o), OHM_TYPE_KEYSTORE))
#define OHM_IS_KEYSTORE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE((k), OHM_TYPE_KEYSTORE))
#define OHM_KEYSTORE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS((o), OHM_TYPE_KEYSTORE, OhmKeystoreClass))


static GType ohm_keystore_get_type(void);
static gboolean ohm_keystore_set_key(OhmKeystore *keystore, const gchar *key,
									 gint value, GError **error);
#include "ohm-keystore-glue.h"

static void ohm_keystore_class_init(OhmKeystoreClass *klass)
{
	dbus_g_object_type_install_info(OHM_TYPE_KEYSTORE, &dbus_glib_ohm_keystore_object_info);
}

static void ohm_keystore_init(OhmKeystore *keystore)
{
	dbus_g_connection_register_g_object(dbus_conn, OHM_DBUS_PATH_KEYSTORE, G_OBJECT(keystore));
}

G_DEFINE_TYPE (OhmKeystore, ohm_keystore, G_TYPE_OBJECT)

static GQuark ohm_keystore_error_quark(void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string("ohm_keystore_error");
	return quark;
}

static gboolean ohm_keystore_set_key(OhmKeystore *keystore, const gchar *key,
									 gint value, GError **error)
{
	if (strcmp(key, "display.dcon_freeze") == 0) {
		powerd_send_event(value == 0 ? "unfreeze_dcon" : "freeze_dcon", NULL);
	} else {
		*error = g_error_new(ohm_keystore_error_quark(),
			OHM_KEYSTORE_ERROR_KEY_MISSING, "Key %s missing", key);
		return FALSE;
	}
	return TRUE;
}

OhmKeystore *ohm_keystore_new(void)
{
	return OHM_KEYSTORE(g_object_new(OHM_TYPE_KEYSTORE, NULL));
}

