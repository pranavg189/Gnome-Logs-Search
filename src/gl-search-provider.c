#include <stdlib.h>

#include "gl-application.h"
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

    gint64 start_time;
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

    g_print("loading: %s\n", gl_journal_model_get_loading(model) ? "TRUE" : "FALSE");

    g_return_if_fail (GL_IS_JOURNAL_MODEL (search_provider->model));

    search_provider->hits = gl_journal_model_get_hits (search_provider->model);

    GVariantBuilder builder;
    GlJournalEntry *entry;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

    g_print("hits len: %d\n", search_provider->hits->len);

    for(int i = 0; i < search_provider->hits->len; i++)
    {
        entry = g_ptr_array_index (search_provider->hits, i);
        g_variant_builder_add (&builder, "s", gl_journal_entry_get_message(entry));
        g_print("hit added: %s\n", gl_journal_entry_get_message(entry));
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

    /* don't attempt searches for a single character */
    /*if (g_strv_length (terms) == 1 &&
        g_utf8_strlen (terms[0], -1) == 1) {
        g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
        return;
    }*/

    // Clear the earlier searches
    if (search_provider->model != NULL)
    {
        g_print("search provider is not null\n");
        g_clear_object (&search_provider->model);
    }

    /* would have to view what is stored in this */
    search_text = g_strjoinv (" ", terms);

    g_print("search_text: %s\n", *terms);

    /* Create a new object for journal-model */
    search_provider->model = gl_journal_model_new();

    search_provider->invocation = g_object_ref(invocation);

    /* create a new object for query */
    query = gl_query_new ();

    /* Add all available fields */
    gl_query_add_match (query, "_PID", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_UID", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_GID", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "MESSAGE", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_COMM", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_SYSTEMD_UNIT", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_KERNEL_DEVICE", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_AUDIT_SESSION", search_text, SEARCH_TYPE_SUBSTRING);
    gl_query_add_match (query, "_EXE", search_text, SEARCH_TYPE_SUBSTRING);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (search_provider->model, query);

    g_signal_connect (search_provider->model, "notify::loading",
                      G_CALLBACK (search_finished), search_provider);

    g_application_hold (g_application_get_default ());

    /* start searching */
    g_print ("*** Search engine search started\n");
}

static gboolean
handle_get_initial_result_set (LogsShellSearchProvider2 *skeleton,
                                           GDBusMethodInvocation *invocation,
                                           gchar **terms,
                                           gpointer user_data)
{
  GlSearchProvider *search_provider = user_data;

  g_print ("****** GetInitialResultSet\n");
  execute_search (search_provider, invocation, terms);
  return TRUE;
}

static gboolean
handle_get_subsearch_result_set (LogsShellSearchProvider2  *skeleton,
                                             GDBusMethodInvocation         *invocation,
                                             gchar                        **previous_results,
                                             gchar                        **terms,
                                             gpointer                       user_data)
{
  GlSearchProvider *search_provider = user_data;

  g_print ("****** GetSubSearchResultSet\n");
  execute_search (search_provider, invocation, terms);
  return TRUE;
}

static gboolean
handle_get_result_metas (LogsShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation         *invocation,
                         gchar                        **results,
                         gpointer                       user_data)
{
    g_print("****** GetResultMetas\n");
    GlSearchProvider *search_provider = user_data;
    const gchar *message;
    const gchar *process_name;

    GHashTable *metas_cache;

    metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, (GDestroyNotify) g_variant_unref);

    // Build the result metas
    GVariantBuilder meta;
    GVariant *meta_variant;
    gint idx;
    for(int i = 0; i < search_provider->hits->len; i++)
    {
        GlJournalEntry *entry = g_ptr_array_index (search_provider->hits, i);
        gchar *id;

        g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

        id = g_strdup_printf ("%d", i);

        message = gl_journal_entry_get_message (entry);
        process_name = gl_journal_entry_get_command_line (entry);


        g_variant_builder_add (&meta, "{sv}",
                               "id", g_variant_new_string (id));

        g_variant_builder_add (&meta, "{sv}",
                           "name", g_variant_new_string (message));

        g_variant_builder_add (&meta, "{sv}",
                           "description", g_variant_new_string (process_name));

        meta_variant = g_variant_builder_end (&meta);

        g_hash_table_insert (metas_cache,
                             g_strdup(message), g_variant_ref_sink (meta_variant));

        g_free (id);
    }

    // Return the array of result metas to DBus
    GVariantBuilder builder;
    GVariant *meta_data;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

    for (idx = 0; results[idx] != NULL; idx++) {
        meta_data = g_hash_table_lookup (metas_cache,
                                    results[idx]);
        g_variant_builder_add_value (&builder, meta_data);
        g_print("result %d: %s\n", idx, results[idx]);
    }

    g_dbus_method_invocation_return_value (invocation,
                                         g_variant_new ("(aa{sv})", &builder));

    g_hash_table_destroy (metas_cache);

    g_print("**********************\n");

    return TRUE;
}

static gboolean
handle_activate_result (LogsShellSearchProvider2 *skeleton,
                        GDBusMethodInvocation        *invocation,
                        gchar                        *result,
                        gchar                        **terms,
                        guint32                       timestamp,
                        gpointer                      user_data)
{
    GlSearchProvider *search_provider = user_data;
    GApplication *app = g_application_get_default();
    GlJournalEntry *entry;

  // Get the GlJournalEntry object for the selected result
    g_print("selected result from search provider is: %s\n", result);

    entry = g_ptr_array_index (search_provider->hits, atoi(result));

    g_print("journal entry message: %s", gl_journal_entry_get_message(entry));

    for(int i=0; terms[i] != NULL;i++)
        g_print("term %d: %s\n", i, terms[i]);

    gl_application_open_detail_entry (app, entry);

    logs_shell_search_provider2_complete_activate_result (skeleton, invocation);
    return TRUE;
}

static gboolean
handle_launch_search (LogsShellSearchProvider2 *skeleton,
                      GDBusMethodInvocation        *invocation,
                      gchar                       **terms,
                      guint32                       timestamp,
                      gpointer                      user_data)
{
  GApplication *app = g_application_get_default ();
  gchar *string = g_strjoinv (" ", terms);

  gl_application_search(app, string);
  g_free (string);

  logs_shell_search_provider2_complete_launch_search (skeleton, invocation);
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

    g_print("Search provider started\n");


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
    g_signal_connect_swapped (self->skeleton,
                              "handle-activate-result",
                              G_CALLBACK (handle_activate_result),
                              self);
    g_signal_connect_swapped (self->skeleton,
                              "handle-launch-search",
                              G_CALLBACK (handle_launch_search),
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