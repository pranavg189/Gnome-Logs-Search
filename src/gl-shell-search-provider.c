/*
 *  GNOME Logs - View and search logs
 *  Copyright (C) 2016  Pranav Ganorkar <pranavg189@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "gl-application.h"
#include "gl-journal-model.h"
#include "gl-journal.h"
#include "gl-util.h"
#include "gl-shell-search-provider-generated.h"
#include "gl-shell-search-provider.h"

#define MAX_NUMBER_OF_RESULTS 5

typedef struct
{
    LogsShellSearchProvider2 *skeleton;

    GlJournalModel *model;
    GDBusMethodInvocation *invocation;
    GHashTable *metas_cache;
    GlUtilClockFormat clock_format;

    guint64 result_id_counter;

} GlShellSearchProviderPrivate;

struct _GlShellSearchProvider
{
    GObject parent;

    /*< private >*/
    GlShellSearchProviderPrivate *priv;
};

struct _GlShellSearchProviderClass
{
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GlShellSearchProvider, gl_shell_search_provider, G_TYPE_OBJECT);

static const gchar DESKTOP_SCHEMA[] = "org.gnome.desktop.interface";
static const gchar CLOCK_FORMAT[] = "clock-format";

static void
model_items_changed (GListModel *list,
                     guint       position,
                     guint       removed,
                     guint       added,
                     gpointer    user_data)
{
    GlShellSearchProvider *search_provider = user_data;
    GlShellSearchProviderPrivate *priv;

    priv = gl_shell_search_provider_get_instance_private (search_provider);

    if (added && !removed)
    {
        /* Check if model has fetched required number of entries */
        if ((g_list_model_get_n_items (list) == MAX_NUMBER_OF_RESULTS) ||
             gl_journal_model_fetched_all (GL_JOURNAL_MODEL (list)))
        {
            GVariantBuilder builder;
            guint index;

            gl_journal_model_stop_idle (GL_JOURNAL_MODEL (list));

            g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

            index = g_list_model_get_n_items (list) - 1;
            while (index >= 0)
            {
                GlRowEntry *row_entry;
                GlJournalEntry *journal_entry;
                gchar *result_id;

                row_entry = g_list_model_get_item (list, index);

                journal_entry = gl_row_entry_get_journal_entry (row_entry);

                result_id = g_strdup_printf ("%" G_GUINT64_FORMAT "", priv->result_id_counter);

                g_hash_table_replace (priv->metas_cache, g_strdup (result_id), g_object_ref (journal_entry));

                g_variant_builder_add (&builder, "s", result_id);

                g_free (result_id);

                priv->result_id_counter++;

                /* Prevent unsigned integer underflow */
                if (index)
                {
                    index--;
                }
                else
                {
                    break;
                }
            }

            /* Finish the search */
            g_dbus_method_invocation_return_value (priv->invocation, g_variant_new ("(as)", &builder));

            g_clear_object (&priv->invocation);

            g_application_release (g_application_get_default ());
        }
    }
}


static void
execute_search (GlShellSearchProvider *search_provider,
                GDBusMethodInvocation *invocation,
                gchar **terms)
{
    GlShellSearchProviderPrivate *priv;
    gchar *search_text;
    GlQuery *query;
    GArray *boot_ids;
    GlJournalBootID *boot_id;
    GSettings *settings;

    priv = gl_shell_search_provider_get_instance_private (search_provider);

    /* don't attempt searches for a single character */
    if (g_strv_length (terms) == 1 && g_utf8_strlen (terms[0], -1) == 1)
    {
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
        return;
    }

    if (priv->invocation)
    {
        g_clear_object (&priv->invocation);
    }

    /* Clear the results of previous search */
    g_hash_table_remove_all (priv->metas_cache);

    /* join terms separated by space */
    search_text = g_strjoinv (" ", terms);

    /* Create the query to be given to journal model */
    query = gl_query_new ();

    /* Add all available journal fields */
    gl_query_add_match (query, "_PID", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_UID", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_GID", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "MESSAGE", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_COMM", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_SYSTEMD_UNIT", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_KERNEL_DEVICE", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_AUDIT_SESSION", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_EXE", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);

    boot_ids = gl_journal_model_get_boot_ids (priv->model);

    boot_id = &g_array_index (boot_ids, GlJournalBootID, boot_ids->len - 1);

    /* Fetch logs until the starting timestamp of current boot */
    gl_query_set_journal_timestamp_range (query, g_get_real_time(),
                                          boot_id->realtime_first);

    settings = g_settings_new (DESKTOP_SCHEMA);
    priv->clock_format = g_settings_get_enum (settings, CLOCK_FORMAT);

    g_object_unref (settings);

    priv->invocation = g_object_ref (invocation);

    /* Start the search */
    gl_journal_model_take_query (priv->model, query);

    g_application_hold (g_application_get_default ());
}


static gboolean
handle_get_initial_result_set (LogsShellSearchProvider2 *skeleton,
                               GDBusMethodInvocation *invocation,
                               gchar **terms,
                               gpointer user_data)
{
    GlShellSearchProvider *search_provider = user_data;

    execute_search (search_provider, invocation, terms);

    return TRUE;
}

static gboolean
handle_get_subsearch_result_set (LogsShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation *invocation,
                                 gchar **previous_results,
                                 gchar **terms,
                                 gpointer user_data)
{
    GlShellSearchProvider *search_provider = user_data;

    execute_search (search_provider, invocation, terms);

    return TRUE;
}

static gboolean
handle_get_result_metas (LogsShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation *invocation,
                         gchar **results,
                         gpointer user_data)
{
    GlShellSearchProvider *search_provider = user_data;
    GIcon *result_icon;
    GVariantBuilder builder;
    gint i;

    GlShellSearchProviderPrivate *priv;

    priv = gl_shell_search_provider_get_instance_private (search_provider);

    /* Load the icon */
    result_icon = g_themed_icon_new_with_default_fallbacks ("text-x-generic");

    /* Build the array of result's metadata */
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    /* Get the shell search provider metadata for each journal entry returned by GetInitialResultSet */
    for (i = 0; results[i] != NULL; i++)
    {
        GlJournalEntry *entry;
        const gchar *message;
        const gchar *process_name;
        guint64 timestamp;
        GDateTime *now;
        gchar *time;
        gchar *result_id;
        GVariantBuilder meta;
        GVariant *meta_data;
        gchar *newline_index;

        /* Get the result id returned by GetInitialResultSet */
        result_id = results[i];

        /* Get the journal entry corresponding to the unique result id */
        entry = g_hash_table_lookup (priv->metas_cache, result_id);

        /* Get the metadata for every journal entry */
        g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

        message = gl_journal_entry_get_message (entry);
        process_name = gl_journal_entry_get_command_line (entry);
        timestamp = gl_journal_entry_get_timestamp (entry);

        now = g_date_time_new_now_local ();
        time = gl_util_timestamp_to_display (timestamp, now,
                                             priv->clock_format, FALSE);

        g_variant_builder_add (&meta, "{sv}",
                               "id", g_variant_new_string (result_id));

        /* Handle message containing multiple lines */
        newline_index = strchr (message, '\n');

        if (newline_index)
        {
            gchar *new_message;

            new_message = gl_util_message_replace_newline (message);

            g_variant_builder_add (&meta, "{sv}",
                               "name", g_variant_new_string (new_message));

            g_free (new_message);
        }
        else
        {
            g_variant_builder_add (&meta, "{sv}",
                               "name", g_variant_new_string (message));
        }


        if (process_name == NULL)
        {
            g_variant_builder_add (&meta, "{sv}",
                               "description", g_variant_new_printf ("%s", time));
        }
        else
        {
            g_variant_builder_add (&meta, "{sv}",
                                   "description",
                                   g_variant_new_printf("Process: %s    Time: %s",
                                                        process_name,
                                                        time));
        }

        g_variant_builder_add (&meta, "{sv}",
                               "icon", g_icon_serialize (result_icon));

        meta_data = g_variant_builder_end (&meta);

        g_variant_builder_add_value (&builder, meta_data);

        g_free (time);
        g_date_time_unref (now);
    }

    g_dbus_method_invocation_return_value (invocation,
                                           g_variant_new ("(aa{sv})", &builder));

    return TRUE;
}

static void
gl_shell_search_provider_dispose (GObject *obj)
{
    GlShellSearchProvider *provider = GL_SHELL_SEARCH_PROVIDER (obj);

    GlShellSearchProviderPrivate *priv;

    priv = gl_shell_search_provider_get_instance_private (provider);

    g_clear_object (&priv->skeleton);
    g_clear_object (&priv->model);
    G_OBJECT_CLASS (gl_shell_search_provider_parent_class)->dispose (obj);
}

static void
gl_shell_search_provider_class_init (GlShellSearchProviderClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = gl_shell_search_provider_dispose;
}

static void
gl_shell_search_provider_init (GlShellSearchProvider *self)
{
    GlShellSearchProviderPrivate *priv;

    priv = gl_shell_search_provider_get_instance_private (self);

    priv->model = gl_journal_model_new ();

    priv->metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

    priv->skeleton = logs_shell_search_provider2_skeleton_new ();

    g_signal_connect (priv->skeleton,
                      "handle-get-initial-result-set",
                      G_CALLBACK (handle_get_initial_result_set),
                      self);
    g_signal_connect (priv->skeleton,
                      "handle-get-subsearch-result-set",
                      G_CALLBACK (handle_get_subsearch_result_set),
                      self);
    g_signal_connect (priv->skeleton,
                      "handle-get-result-metas",
                      G_CALLBACK (handle_get_result_metas),
                      self);

    g_signal_connect (priv->model, "items-changed",
                      G_CALLBACK (model_items_changed), self);

    priv->result_id_counter = 0;
}

gboolean
gl_shell_search_provider_register (GlShellSearchProvider *self,
                                   GDBusConnection *connection,
                                   GError **error)
{
    GlShellSearchProviderPrivate *priv;

    priv = gl_shell_search_provider_get_instance_private (self);

    return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->skeleton),
                                             connection,
                                             "/org/gnome/Logs/SearchProvider", error);
}

void
gl_shell_search_provider_unregister (GlShellSearchProvider *self)
{
    GlShellSearchProviderPrivate *priv;

    priv = gl_shell_search_provider_get_instance_private (self);

    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (priv->skeleton));
}

GlShellSearchProvider *
gl_shell_search_provider_new (void)
{
    return g_object_new (GL_TYPE_SHELL_SEARCH_PROVIDER, NULL);
}
