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

#include "gl-journal-model.h"
#include "gl-journal.h"
#include "gl-search-provider-generated.h"
#include "gl-search-provider.h"

struct _GlSearchProvider
{
    GObject parent_instance;
    LogsShellSearchProvider2 *skeleton;

    GlJournalModel *model;
    GPtrArray *hits;
    GDBusMethodInvocation *invocation;
};

struct _GlSearchProviderClass
{
    GObjectClass parent_class;
};

G_DEFINE_TYPE (GlSearchProvider, gl_search_provider, G_TYPE_OBJECT);

static void
search_finished (GlJournalModel *model,
                 GParamSpec *pspec,
                 gpointer user_data)
{
    GlSearchProvider *search_provider = user_data;
    GVariantBuilder builder;
    GlJournalEntry *entry;
    gint i;

    g_return_if_fail (GL_IS_JOURNAL_MODEL (search_provider->model));

    search_provider->hits = gl_journal_model_get_hits (search_provider->model);

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

    for(i = 0; i < search_provider->hits->len; i++)
    {
        entry = g_ptr_array_index (search_provider->hits, i);
        g_variant_builder_add (&builder, "s", gl_journal_entry_get_message(entry));
    }

    g_dbus_method_invocation_return_value (search_provider->invocation, g_variant_new ("(as)", &builder));

    g_application_release (g_application_get_default ());

    g_clear_object (&search_provider->invocation);
}

static void
execute_search (GlSearchProvider *search_provider,
                GDBusMethodInvocation *invocation,
                gchar **terms)
{
    gchar *search_text;
    GlQuery *query;

    // Clear the earlier searches
    if (search_provider->model != NULL)
    {
        g_clear_object (&search_provider->model);
    }

    search_text = g_strjoinv (" ", terms);

    search_provider->model = gl_journal_model_new ();

    search_provider->invocation = g_object_ref (invocation);

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

    /* Set the created query on the journal model */
    gl_journal_model_take_query (search_provider->model, query);

    g_signal_connect (search_provider->model, "notify::loading",
                      G_CALLBACK (search_finished), search_provider);

    g_application_hold (g_application_get_default ());
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
    const gchar *message;
    const gchar *process_name;
    GHashTable *metas_cache;
    GVariantBuilder meta;
    GVariant *meta_variant;
    GVariantBuilder builder;
    GVariant *meta_data;
    GIcon *result_icon;
    gint idx;

    metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify) g_variant_unref);

    /* Load the icon */
    result_icon = g_themed_icon_new_with_default_fallbacks ("text-x-generic");

    for (idx = 0; idx < search_provider->hits->len; idx++)
    {
        GlJournalEntry *entry;
        gchar *id;

        entry = g_ptr_array_index (search_provider->hits, idx);

        g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

        id = g_strdup_printf ("%d", idx);

        message = gl_journal_entry_get_message (entry);
        process_name = gl_journal_entry_get_command_line (entry);


        g_variant_builder_add (&meta, "{sv}",
                               "id", g_variant_new_string (id));

        g_variant_builder_add (&meta, "{sv}",
                               "name", g_variant_new_string (message));

        g_variant_builder_add (&meta, "{sv}",
                               "description", g_variant_new_string (process_name));

        g_variant_builder_add (&meta, "{sv}",
                               "icon", g_icon_serialize (result_icon));

        meta_variant = g_variant_builder_end (&meta);

        g_hash_table_insert (metas_cache,
                             g_strdup (message), g_variant_ref_sink (meta_variant));

        g_free (id);
    }

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    for (idx = 0; results[idx] != NULL; idx++)
    {
        meta_data = g_hash_table_lookup (metas_cache, results[idx]);

        g_variant_builder_add_value (&builder, meta_data);
    }

    g_dbus_method_invocation_return_value (invocation,
                                           g_variant_new ("(aa{sv})", &builder));

    g_hash_table_destroy (metas_cache);

    return TRUE;
}

static void
search_provider_dispose (GObject *obj)
{
    GlSearchProvider *provider = GL_SEARCH_PROVIDER (obj);

    g_clear_object (&provider->skeleton);
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

    self->skeleton = logs_shell_search_provider2_skeleton_new ();

    g_signal_connect_swapped (self->skeleton,
                              "handle-get-initial-result-set",
                              G_CALLBACK (handle_get_initial_result_set),
                              self);
    g_signal_connect_swapped (self->skeleton,
                              "handle-get-subsearch-result-set",
                              G_CALLBACK (handle_get_subsearch_result_set),
                              self);
    g_signal_connect_swapped (self->skeleton,
                              "handle-get-result-metas",
                              G_CALLBACK (handle_get_result_metas),
                              self);
}

gboolean
gl_search_provider_register (GlSearchProvider *self,
                             GDBusConnection *connection,
                             GError **error)
{
    return g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                             connection,
                                             "/org/gnome/Logs/SearchProvider", error);
}

void
gl_search_provider_unregister (GlSearchProvider *self)
{
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));
}

GlSearchProvider *
gl_search_provider_new (void)
{
    return g_object_new (GL_TYPE_SEARCH_PROVIDER, NULL);
}
