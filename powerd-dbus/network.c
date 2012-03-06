#include <glib.h>
#include "powerd-dbus.h"

/*
 * "suspend OK" state flags.
 * TRUE means suspend OK, FALSE means suspend inhibited.
 */
static gboolean nm_susp_ok = TRUE;
static gboolean wpas_susp_ok = TRUE;
static gboolean last_susp_ok = TRUE;

static guint timeout_id = 0;

static gboolean send_susp_ok(void)
{
	last_susp_ok = TRUE;
	powerd_send_event("allow-suspend", "network_suspend_ok");
	timeout_id = 0;
	return FALSE;
}

static void communicate_state()
{
	gboolean new_susp_ok = nm_susp_ok && wpas_susp_ok;

	/*
	 * If suspend-not-OK but we had a timer about to send suspend-OK,
	 * abort the timer.
	 */
	if (!new_susp_ok && timeout_id) {
		g_message("%d: abort suspend-OK timer, suspend not OK.", (int)time(0));
		g_source_remove(timeout_id);
		timeout_id = 0;
	}

	if (new_susp_ok == last_susp_ok)
		return;

	/* communicate suspend-not-OK events immediately */
	if (!new_susp_ok) {
		last_susp_ok = new_susp_ok;
		powerd_send_event("inhibit-suspend", "network_suspend_not_ok");
		return;
	}

	/* nothing to do if we already have scheduled the suspend-OK message */
	if (timeout_id)
		return;

	/* defer suspend-OK events for 8 seconds, to ensure things have settled */
	timeout_id = g_timeout_add_seconds(8, (GSourceFunc) send_susp_ok,
		NULL);
	g_message("%d: sending suspend-OK message after settle delay", (int)time(0));
}

void nm_suspend_ok(gboolean susp_ok)
{
	nm_susp_ok = susp_ok;
	communicate_state();
}

void wpas_suspend_ok(gboolean susp_ok)
{
	wpas_susp_ok = susp_ok;
	communicate_state();
}
