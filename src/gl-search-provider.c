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
#include "gl-search-provider-generated.h"
#include "gl-search-provider.h"

#define MAX_NUMBER_OF_RESULTS 5

typedef struct
{
    LogsShellSearchProvider2 *skeleton;
    GlJournalModel *model;
    GDBusMethodInvocation *invocation;

    GHashTable *metas_cache; /* remember to clear the HashTable everytime a new search is started */

} GlSearchProviderPrivate;

struct _GlSearchProvider
{
    GObject parent;

    /*< private >*/
    GlSearchProviderPrivate *priv;
};

struct _GlSearchProviderClass
{
    GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_PRIVATE (GlSearchProvider, gl_search_provider, G_TYPE_OBJECT);

static void
results_added (GListModel *list,
               guint       position,
               guint       removed,
               guint       added,
               gpointer    user_data)
{
    GlSearchProvider *search_provider = user_data;
    GlSearchProviderPrivate *priv;
    priv = gl_search_provider_get_instance_private (search_provider);

    /* check if model has fetched the minimum required results to display in search provider */
    if ((g_list_model_get_n_items (list) == MAX_NUMBER_OF_RESULTS))
    {
        GVariantBuilder builder;
        guint i;

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

        i = 0;
        while (g_list_model_get_item (list, i))
        {
            GlJournalEntry *entry;
            const gchar *entry_cursor;

            entry = g_list_model_get_item (list, i);

            entry_cursor = gl_journal_entry_get_cursor (entry);

            g_hash_table_replace (priv->metas_cache, g_strdup (entry_cursor), g_object_ref (entry));

            g_variant_builder_add (&builder, "s", entry_cursor);

            i++;
        }

        g_print("model has fetched 3 entries\n");

        g_dbus_method_invocation_return_value (priv->invocation, g_variant_new ("(as)", &builder));

        g_clear_object (&priv->invocation);

        g_application_release (g_application_get_default ());
    }
}


static void
execute_search (GlSearchProvider *search_provider,
                GDBusMethodInvocation *invocation,
                gchar **terms)
{
    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (search_provider);
    gchar *search_text;
    GlQuery *query;

    /* don't attempt searches for a single character */
    if (g_strv_length (terms) == 1 && g_utf8_strlen (terms[0], -1) == 1)
    {
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
        return;
    }

    /* Clear the results of previous searches */
    g_hash_table_remove_all (priv->metas_cache);

    /* join terms separated by space */
    search_text = g_strjoinv (" ", terms);

    /* Create the query to be given to journal model */
    query = gl_query_new ();

    /* Add all available journal fields */
    /* Does it make sense to search the terms by all the fields ? */
    /* Also, shoud entire journal be searched or just the current boot ? */
    gl_query_add_match (query, "_PID", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_UID", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_GID", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "MESSAGE", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_COMM", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_SYSTEMD_UNIT", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_KERNEL_DEVICE", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_AUDIT_SESSION", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_EXE", search_text, GL_QUERY_SEARCH_TYPE_SUBSTRING);

    priv->invocation = g_object_ref (invocation);

    gl_journal_model_take_query (priv->model, query);

    g_application_hold (g_application_get_default ());

    g_print ("execute_search\n");
}


static gboolean
handle_get_initial_result_set (LogsShellSearchProvider2 *skeleton,
                               GDBusMethodInvocation *invocation,
                               gchar **terms,
                               gpointer user_data)
{
  GlSearchProvider *search_provider = user_data;

  execute_search (search_provider, invocation, terms);

  return TRUE;
}

/* Here you also get the previous results through which we can
   refine our search */
static gboolean
handle_get_subsearch_result_set (LogsShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation *invocation,
                                 gchar **previous_results,
                                 gchar **terms,
                                 gpointer user_data)
{
  GlSearchProvider *search_provider = user_data;

  execute_search (search_provider, invocation, terms);

  return TRUE;
}

static gboolean
handle_get_result_metas (LogsShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation *invocation,
                         gchar **results,
                         gpointer user_data)
{
    GlSearchProvider *search_provider = user_data;
    GIcon *result_icon;
    GVariantBuilder builder;
    gint idx;

    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (search_provider);

    /* Load the icon */
    result_icon = g_themed_icon_new_with_default_fallbacks ("text-x-generic");

    /* Build the array of result's metadata */
    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    /* Get the shell search provider metadata for each journal entry returned by GetInitialResultSet */
    for (idx = 0; results[idx] != NULL; idx++)
    {
        GlJournalEntry *entry;
        const gchar *message;
        const gchar *process_name;
        gchar *entry_cursor;
        GVariantBuilder meta;
        GVariant *meta_data;

        /* Get the journal entry cursor returned by GetInitialResultSet */
        entry_cursor = results[idx];

        /* Get the journal entry corresponding to the unique journal entry cursor */
        entry = g_hash_table_lookup (priv->metas_cache, entry_cursor);

        /* Get the metadata for every journal entry */
        g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

        message = gl_journal_entry_get_message (entry);
        process_name = gl_journal_entry_get_command_line (entry);

        g_variant_builder_add (&meta, "{sv}",
                               "id", g_variant_new_string (entry_cursor));

        g_variant_builder_add (&meta, "{sv}",
                               "name", g_variant_new_string (message));

        if(process_name == NULL)
        {
            g_variant_builder_add (&meta, "{sv}",
                               "description", g_variant_new_string ("null")); // change this to " " afterwards
        }
        else
        {
            g_variant_builder_add (&meta, "{sv}",
                                   "description", g_variant_new_string (process_name));
        }

        g_variant_builder_add (&meta, "{sv}",
                               "icon", g_icon_serialize (result_icon));

        meta_data = g_variant_builder_end (&meta);

        g_variant_builder_add_value (&builder, meta_data);

    }

    g_dbus_method_invocation_return_value (invocation,
                                           g_variant_new ("(aa{sv})", &builder));


    return TRUE;
}

static gboolean
handle_activate_result (LogsShellSearchProvider2 *skeleton,
                        GDBusMethodInvocation *invocation,
                        gchar *result,
                        gchar **terms,
                        guint32 timestamp,
                        gpointer user_data)
{
    GlSearchProvider *search_provider = user_data;
    GApplication *app;
    GlJournalEntry *entry;

    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (search_provider);

    app = g_application_get_default ();

    entry = g_hash_table_lookup (priv->metas_cache, result);

    gl_application_open_detail_entry (app, entry);

    logs_shell_search_provider2_complete_activate_result (skeleton, invocation);

    return TRUE;
}

static gboolean
handle_launch_search (LogsShellSearchProvider2 *skeleton,
                      GDBusMethodInvocation *invocation,
                      gchar **terms,
                      guint32 timestamp,
                      gpointer user_data)
{
  GApplication *app;
  gchar *string;

  string = g_strjoinv (" ", terms);

  app = g_application_get_default ();

  gl_application_search (app, string);

  g_free (string);

  logs_shell_search_provider2_complete_launch_search (skeleton, invocation);

  return TRUE;
}

static void
search_provider_dispose (GObject *obj)
{
    GlSearchProvider *provider = GL_SEARCH_PROVIDER (obj);

    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (provider);

    g_clear_object (&priv->skeleton);
    g_clear_object (&priv->model);
    G_OBJECT_CLASS (gl_search_provider_parent_class)->dispose (obj);
}

static void
gl_search_provider_class_init (GlSearchProviderClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = search_provider_dispose;
}

static void
gl_search_provider_init (GlSearchProvider *self)
{
    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (self);

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
    g_signal_connect (priv->skeleton,
                      "handle-activate-result",
                      G_CALLBACK (handle_activate_result),
                      self);
    g_signal_connect (priv->skeleton,
                      "handle-launch-search",
                      G_CALLBACK (handle_launch_search),
                      self);

    g_signal_connect (priv->model, "items-changed",
                      G_CALLBACK (results_added), self);
}

gboolean
gl_search_provider_register (GlSearchProvider *self,
                             GDBusConnection *connection,
                             GError **error)
{
    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (self);

    return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (priv->skeleton),
                                             connection,
                                             "/org/gnome/Logs/SearchProvider", error);
}

void
gl_search_provider_unregister (GlSearchProvider *self)
{
    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (self);

    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (priv->skeleton));
}

GlSearchProvider *
gl_search_provider_new (void)
{
    return g_object_new (GL_TYPE_SEARCH_PROVIDER, NULL);
}

void
gl_search_provider_setup (GlSearchProvider *self, GlJournalModel *model)
{
    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (self);

    priv->model = g_object_ref (model);
}
