#include <glib.h>
#include "powerd-dbus.h"

/*
 * "suspend OK" state flags.
 * TRUE means suspend OK, FALSE means suspend inhibited.
 */
static gboolean nm_state = TRUE;
static gboolean wpas_state = TRUE;
static gboolean last_state = TRUE;

static guint timeout_id = 0;

static gboolean send_suspend_ok(void)
{
	last_state = TRUE;
	powerd_send_event("allow-suspend", "network_suspend_ok");
	timeout_id = 0;
	return FALSE;
}

static void communicate_state()
{
	gboolean new_state = nm_state && wpas_state;

	/*
	 * If suspend-not-OK but we had a timer about to send suspend-OK,
	 * abort the timer.
	 */
	if (!new_state && timeout_id) {
		g_message("abort suspend-OK timer, suspend not OK.");
		g_source_remove(timeout_id);
		timeout_id = 0;
	}

	if (new_state == last_state)
		return;

	/* communicate suspend-not-OK events immediately */
	if (!new_state) {
		last_state = new_state;
		powerd_send_event("inhibit-suspend", "network_suspend_not_ok");
		return;
	}

	/* nothing to do if we already have scheduled the suspend-OK message */
	if (timeout_id)
		return;

	/* defer suspend-OK events for 8 seconds, to ensure things have settled */
	timeout_id = g_timeout_add_seconds(8, (GSourceFunc) send_suspend_ok,
		NULL);
	g_message("sending suspend-OK message after settle delay");
}

void nm_suspend_ok(gboolean suspend_ok)
{
	nm_state = suspend_ok;
	communicate_state();
}

void wpas_suspend_ok(gboolean suspend_ok)
{
	wpas_state = suspend_ok;
	communicate_state();
}
