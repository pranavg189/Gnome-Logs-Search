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

typedef struct
{
    LogsShellSearchProvider2 *skeleton;
    GlJournalModel *model;
    GDBusMethodInvocation *invocation;

    GPtrArray *hits; /* remember to clear the hits array everytime a new search is started */

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
search_finished (GlJournalModel *model,
                 GParamSpec *pspec,
                 gpointer user_data)
{
    GlSearchProvider *search_provider = user_data;
    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (search_provider);

    if(priv->invocation == NULL)
    {
        g_print("invocation is null\n");
        //g_application_release (g_application_get_default ());
        return;
    }

    // If model has started loading the entries
    if(gl_journal_model_get_loading (priv->model))
    {
        g_print("model is currently loading\n");
        //g_application_release (g_application_get_default ());
        return;
    }
    // if model has finished loading the entries
    else
    {
        g_return_if_fail (GL_IS_JOURNAL_MODEL (priv->model));
        g_print("model has finished loading entries\n");
        g_print("hits len: %d\n", g_list_model_get_n_items (G_LIST_MODEL (priv->model)));

        /* check if model has fetched at least some results */
        if ((g_list_model_get_n_items (G_LIST_MODEL (priv->model))) > 0)
        {
            GVariantBuilder builder;
            gint i;

            g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

            i = 0;
            while (g_list_model_get_item (G_LIST_MODEL (priv->model), i))
            {
                GlJournalEntry *entry;
                entry = g_list_model_get_item (G_LIST_MODEL (priv->model), i);

                g_ptr_array_add (priv->hits, entry);

                g_variant_builder_add (&builder, "s", gl_journal_entry_get_message (entry));

                i++;
            }

            g_print("priv->hits->len: %d\n", priv->hits->len);

            g_dbus_method_invocation_return_value (priv->invocation, g_variant_new ("(as)", &builder));
        }

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
    if (priv->hits->len > 0)
    {
        g_ptr_array_free (priv->hits, TRUE);

        priv->hits = g_ptr_array_new_with_free_func (g_object_unref);
    }

    /* join terms seperated by space */
    search_text = g_strjoinv (" ", terms);

    /* Create the query to be given to journal model */
    query = gl_query_new ();

    /* Add all available journal fields */
    /* Does it make sense to search the terms by all the fields ? */
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
    const gchar *message;
    const gchar *process_name;
    GHashTable *metas_cache;
    GVariantBuilder meta;
    GVariant *meta_variant;
    GVariantBuilder builder;
    GVariant *meta_data;
    GIcon *result_icon;
    gint idx;

    GlSearchProviderPrivate *priv;

    priv = gl_search_provider_get_instance_private (search_provider);

    metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify) g_variant_unref);

    /* Load the icon */
    result_icon = g_themed_icon_new_with_default_fallbacks ("text-x-generic");

    g_print ("hits len result metas: %d\n", priv->hits->len);

    if (priv->hits->len > 0)
    {

        for (idx = 0; idx < priv->hits->len; idx++)
        {
            GlJournalEntry *entry;
            gchar *id;

            entry = g_ptr_array_index (priv->hits, idx);

            g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

            id = g_strdup_printf ("%d", idx);

            message = gl_journal_entry_get_message (entry);
            process_name = gl_journal_entry_get_command_line (entry);


            g_variant_builder_add (&meta, "{sv}",
                                   "id", g_variant_new_string (id));

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

            meta_variant = g_variant_builder_end (&meta);

            g_hash_table_insert (metas_cache,
                                 g_strdup (message), g_variant_ref_sink (meta_variant));

            g_free (id);
        }

        /* Look up the meta data in the hash table */
        g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

        for (idx = 0; results[idx] != NULL; idx++)
        {
            meta_data = g_hash_table_lookup (metas_cache, results[idx]);

            g_variant_builder_add_value (&builder, meta_data);
        }

        g_dbus_method_invocation_return_value (invocation,
                                               g_variant_new ("(aa{sv})", &builder));

    }

    g_hash_table_destroy (metas_cache);


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

    entry = g_ptr_array_index (priv->hits, atoi (result));

    gl_application_open_detail_entry (app, entry);

    logs_shell_search_provider2_complete_activate_result (skeleton, invocation);

    return TRUE;
}

// static gboolean
// handle_launch_search (LogsShellSearchProvider2 *skeleton,
//                       GDBusMethodInvocation *invocation,
//                       gchar **terms,
//                       guint32 timestamp,
//                       gpointer user_data)
// {
//   GApplication *app;
//   gchar *string;

//   string = g_strjoinv (" ", terms);

//   app = g_application_get_default ();

//   gl_application_search (app, string);

//   g_free (string);

//   logs_shell_search_provider2_complete_launch_search (skeleton, invocation);

//   return TRUE;
// }

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

    priv->hits = g_ptr_array_new_with_free_func (g_object_unref);

    /* We can't use it here because GlApplication class has not been initialized yet */
    //g_application_bind_busy_property (g_application_get_default (), priv->model, "loading");

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
    // g_signal_connect_swapped (self->skeleton,
    //                           "handle-launch-search",
    //                           G_CALLBACK (handle_launch_search),
    //                           self);

    g_signal_connect (priv->model, "notify::loading",
                      G_CALLBACK (search_finished), self);
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
