/*
 *  GNOME Logs - View and search logs
 *  Copyright (C) 2013, 2014, 2015  Red Hat, Inc.
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

#include "gl-eventviewlist.h"

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <stdlib.h>

#include "gl-categorylist.h"
#include "gl-enums.h"
#include "gl-eventtoolbar.h"
#include "gl-eventview.h"
#include "gl-eventviewdetail.h"
#include "gl-eventviewrow.h"
#include "gl-journal-model.h"
#include "gl-util.h"

struct _GlEventViewList
{
    /*< private >*/
    GtkBox parent_instance;
};

typedef struct
{
    GlJournalModel *journal_model;
    GlJournalEntry *entry;
    GlUtilClockFormat clock_format;
    GtkListBox *entries_box;
    GtkSizeGroup *category_sizegroup;
    GtkSizeGroup *message_sizegroup;
    GtkSizeGroup *time_sizegroup;
    GtkWidget *categories;
    GtkWidget *event_search;
    GtkWidget *event_scrolled;
    GtkWidget *search_entry;
    GtkWidget *search_dropdown_button;

    /* Search popover elements */
    GtkWidget *parameter_stack;
    GtkWidget *parameter_button_label;
    GtkWidget *parameter_label_stack;
    GtkWidget *parameter_treeview;
    GtkListStore *parameter_liststore;
    GtkWidget *search_type_revealer;
    GtkWidget *range_stack;
    GtkWidget *range_label_stack;
    GtkWidget *range_treeview;
    GtkWidget *range_button_label;
    GtkWidget *clear_range_button;
    GtkWidget *range_button_drop_down_image;
    GtkListStore *range_liststore;
    GtkWidget *search_popover_menu;

    GtkWidget *start_time_spinbox_revealer;
    GtkWidget *start_time_stack;
    GtkWidget *start_time_button_drop_down_image;
    GtkWidget *start_time_clear_button;
    GtkWidget *start_time_hour_spin;
    GtkWidget *start_time_minute_spin;
    GtkWidget *start_time_second_spin;
    GtkWidget *start_time_am_pm_spin;
    GtkWidget *start_time_button_label;

    GtkWidget *start_date_stack;
    GtkWidget *start_date_calendar_revealer;
    GtkWidget *start_date_button_drop_down_image;
    GtkWidget *start_date_clear_button;
    GtkWidget *start_date_entry;
    GtkWidget *start_date_button_label;

    GtkWidget *end_time_spinbox_revealer;
    GtkWidget *end_time_stack;
    GtkWidget *end_time_button_drop_down_image;
    GtkWidget *end_time_clear_button;
    GtkWidget *end_time_hour_spin;
    GtkWidget *end_time_minute_spin;
    GtkWidget *end_time_second_spin;
    GtkWidget *end_time_am_pm_spin;
    GtkWidget *end_time_button_label;

    GtkWidget *end_date_stack;
    GtkWidget *end_date_calendar_revealer;
    GtkWidget *end_date_button_drop_down_image;
    GtkWidget *end_date_clear_button;
    GtkWidget *end_date_entry;
    GtkWidget *end_date_button_label;

    gchar *search_text;
    const gchar *boot_match;
    gsize parameter_group;
    GlQuerySearchType search_type;
    gsize range_group;
    guint64 custom_start_timestamp;
    guint64 custom_end_timestamp;
} GlEventViewListPrivate;

/* We define these two enum values as 2 and 3 to avoid the conflict with TRUE
 * and FALSE */
typedef enum
{
    LOGICAL_OR = 2,
    LOGICAL_AND = 3
} GlEventViewListLogic;

typedef enum
{
    ALL_AVAILABLE_FIELDS,
    PID = 2,
    UID,
    GID,
    MESSAGE,
    PROCESS_NAME,
    SYSTEMD_UNIT,
    KERNEL_DEVICE,
    AUDIT_SESSION,
    EXECUTABLE_PATH
} GlParameterGroups;

typedef enum
{
    CURRENT_BOOT,
    PREVIOUS_BOOT,
    TODAY = 3,
    YESTERDAY = 4,
    LAST_3_DAYS = 5,
    ENTIRE_JOURNAL = 7,
    SET_CUSTOM_RANGE = 9
} GlRangeGroups;

typedef enum
{
    AM,
    PM
} GlTimePeriod;

G_DEFINE_TYPE_WITH_PRIVATE (GlEventViewList, gl_event_view_list, GTK_TYPE_BOX)

static const gchar DESKTOP_SCHEMA[] = "org.gnome.desktop.interface";
static const gchar SETTINGS_SCHEMA[] = "org.gnome.Logs";
static const gchar CLOCK_FORMAT[] = "clock-format";
static const gchar SORT_ORDER[] = "sort-order";

gchar *
gl_event_view_list_get_output_logs (GlEventViewList *view)
{
    gchar *output_buf = NULL;
    gint index = 0;
    GOutputStream *stream;
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    stream = g_memory_output_stream_new_resizable ();

    while (gtk_list_box_get_row_at_index (GTK_LIST_BOX (priv->entries_box),
                                          index) != NULL)
    {
        const gchar *comm;
        const gchar *message;
        gchar *output_text;
        gchar *time;
        GDateTime *now;
        guint64 timestamp;
        GtkListBoxRow *row;

        row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (priv->entries_box),
                                             index);

        /* Only output search results.
         * Search results are entries that are visible and child visible */
        if (gtk_widget_get_mapped (GTK_WIDGET (row)) == FALSE
            || gtk_widget_get_visible (GTK_WIDGET (row)) == FALSE)
        {
            index++;
            continue;
        }

        comm = gl_event_view_row_get_command_line (GL_EVENT_VIEW_ROW (row));
        message = gl_event_view_row_get_message (GL_EVENT_VIEW_ROW (row));
        timestamp = gl_event_view_row_get_timestamp (GL_EVENT_VIEW_ROW (row));
        now = g_date_time_new_now_local ();
        time = gl_util_timestamp_to_display (timestamp, now,
                                             priv->clock_format, TRUE);

        output_text = g_strconcat (time, " ",
                                   comm ? comm : "kernel", ": ",
                                   message, "\n", NULL);
        index++;

        g_output_stream_write (stream, output_text, strlen (output_text),
                               NULL, NULL);

        g_date_time_unref (now);
        g_free (time);
        g_free (output_text);
    }

    output_buf = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (stream));

    g_output_stream_close (stream, NULL, NULL);

    return output_buf;
}



static void
listbox_update_header_func (GtkListBoxRow *row,
                            GtkListBoxRow *before,
                            gpointer user_data)
{
    GtkWidget *current;

    if (before == NULL)
    {
        gtk_list_box_row_set_header (row, NULL);
        return;
    }

    current = gtk_list_box_row_get_header (row);

    if (current == NULL)
    {
        current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_show (current);
        gtk_list_box_row_set_header (row, current);
    }
}



static void
on_listbox_row_activated (GtkListBox *listbox,
                          GtkListBoxRow *row,
                          GlEventViewList *view)
{
    GlEventViewListPrivate *priv;
    GtkWidget *toplevel;

    priv = gl_event_view_list_get_instance_private (view);
    priv->entry = gl_event_view_row_get_entry (GL_EVENT_VIEW_ROW (row));

    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

    if (gtk_widget_is_toplevel (toplevel))
    {
        GAction *mode;
        GEnumClass *eclass;
        GEnumValue *evalue;

        mode = g_action_map_lookup_action (G_ACTION_MAP (toplevel), "view-mode");
        eclass = g_type_class_ref (GL_TYPE_EVENT_VIEW_MODE);
        evalue = g_enum_get_value (eclass, GL_EVENT_VIEW_MODE_DETAIL);

        g_action_activate (mode, g_variant_new_string (evalue->value_nick));

        g_type_class_unref (eclass);
    }
    else
    {
        g_debug ("Widget not in toplevel window, not switching toolbar mode");
    }
}

GlJournalEntry *
gl_event_view_list_get_detail_entry (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    return priv->entry;
}

gchar *
gl_event_view_list_get_current_boot_time (GlEventViewList *view,
                                          const gchar *boot_match)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    return gl_journal_model_get_current_boot_time (priv->journal_model,
                                                   boot_match);
}

GArray *
gl_event_view_list_get_boot_ids (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    return gl_journal_model_get_boot_ids (priv->journal_model);
}

gboolean
gl_event_view_list_handle_search_event (GlEventViewList *view,
                                        GAction *action,
                                        GdkEvent *event)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    if (g_action_get_enabled (action))
    {
        if (gtk_search_bar_handle_event (GTK_SEARCH_BAR (priv->event_search),
                                         event) == GDK_EVENT_STOP)
        {
            g_action_change_state (action, g_variant_new_boolean (TRUE));

            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

void
gl_event_view_list_set_search_mode (GlEventViewList *view,
                                    gboolean state)
{
    GlEventViewListPrivate *priv;

    g_return_if_fail (GL_EVENT_VIEW_LIST (view));

    priv = gl_event_view_list_get_instance_private (view);

    gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (priv->event_search), state);

    if (state)
    {
        gtk_widget_grab_focus (priv->search_entry);
        gtk_editable_set_position (GTK_EDITABLE (priv->search_entry), -1);
    }
    else
    {
        gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");
    }
}

static GtkWidget *
gl_event_view_create_empty (G_GNUC_UNUSED GlEventViewList *view)
{
    GtkWidget *box;
    GtkStyleContext *context;
    GtkWidget *image;
    GtkWidget *label;
    gchar *markup;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (box, TRUE);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand (box, TRUE);
    context = gtk_widget_get_style_context (box);
    gtk_style_context_add_class (context, "dim-label");

    image = gtk_image_new_from_icon_name ("action-unavailable-symbolic", 0);
    context = gtk_widget_get_style_context (image);
    gtk_style_context_add_class (context, "dim-label");
    gtk_image_set_pixel_size (GTK_IMAGE (image), 128);
    gtk_container_add (GTK_CONTAINER (box), image);

    label = gtk_label_new (NULL);
    /* Translators: Shown when there are no (zero) results in the current
     * view. */
    markup = g_markup_printf_escaped ("<big>%s</big>", _("No results"));
    gtk_label_set_markup (GTK_LABEL (label), markup);
    gtk_container_add (GTK_CONTAINER (box), label);
    g_free (markup);

    gtk_widget_show_all (box);

    return box;
}

static GtkWidget *
gl_event_list_view_create_row_widget (gpointer item,
                                      gpointer user_data)
{
    GtkWidget *rtn;
    GtkWidget *message_label;
    GtkWidget *time_label;
    GlCategoryList *list;
    GlCategoryListFilter filter;
    GlEventViewList *view = user_data;

    GlEventViewListPrivate *priv = gl_event_view_list_get_instance_private (view);

    list = GL_CATEGORY_LIST (priv->categories);
    filter = gl_category_list_get_category (list);

    if (filter == GL_CATEGORY_LIST_FILTER_IMPORTANT)
    {
        GtkWidget *category_label;

        rtn = gl_event_view_row_new (item,
                                     priv->clock_format,
                                     GL_EVENT_VIEW_ROW_CATEGORY_IMPORTANT);

        category_label = gl_event_view_row_get_category_label (GL_EVENT_VIEW_ROW (rtn));
        gtk_size_group_add_widget (GTK_SIZE_GROUP (priv->category_sizegroup),
                                   category_label);
    }
    else
    {
        rtn = gl_event_view_row_new (item,
                                     priv->clock_format,
                                     GL_EVENT_VIEW_ROW_CATEGORY_NONE);
    }

    message_label = gl_event_view_row_get_message_label (GL_EVENT_VIEW_ROW (rtn));
    time_label = gl_event_view_row_get_time_label (GL_EVENT_VIEW_ROW (rtn));

    gtk_size_group_add_widget (GTK_SIZE_GROUP (priv->message_sizegroup),
                               message_label);
    gtk_size_group_add_widget (GTK_SIZE_GROUP (priv->time_sizegroup),
                               time_label);

    return rtn;
}

static gchar *
get_uid_match_field_value (void)
{
    GCredentials *creds;
    uid_t uid;
    gchar *str = NULL;

    creds = g_credentials_new ();
    uid = g_credentials_get_unix_user (creds, NULL);

    if (uid != -1)
        str = g_strdup_printf ("%d", uid);

    g_object_unref (creds);
    return str;
}

/* Get Boot ID for current boot match */
static gchar *
get_current_boot_id (const gchar *boot_match)
{
    gchar *boot_value;

    boot_value = strchr (boot_match, '=') + 1;

    return g_strdup (boot_value);
}

static void
query_add_search_matches (GlQuery *query,
                          gsize parameter_group,
                          const gchar *search_text,
                          GlQuerySearchType search_type)
{
    switch (parameter_group)
    {
        case ALL_AVAILABLE_FIELDS:
        {
            gl_query_add_match (query, "_PID", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_UID", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_GID", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "MESSAGE", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_COMM", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_SYSTEMD_UNIT", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_KERNEL_DEVICE", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_AUDIT_SESSION", search_text, SEARCH_TYPE_SUBSTRING);
            gl_query_add_match (query, "_EXE", search_text, SEARCH_TYPE_SUBSTRING);
        }
        break;

        case PID:
        {
            gl_query_add_match (query, "_PID", search_text, search_type);
        }
        break;

        case UID:
        {
            gl_query_add_match (query, "_UID", search_text, search_type);
        }
        break;

        case GID:
        {
            gl_query_add_match (query, "_GID", search_text, search_type);
        }
        break;

        case MESSAGE:
        {
            gl_query_add_match (query, "MESSAGE", search_text, search_type);
        }
        break;

        case PROCESS_NAME:
        {
            gl_query_add_match (query, "_COMM", search_text, search_type);
        }
        break;

        case SYSTEMD_UNIT:
        {
            gl_query_add_match (query, "_SYSTEMD_UNIT", search_text, search_type);
        }
        break;

        case KERNEL_DEVICE:
        {
            gl_query_add_match (query, "_KERNEL_DEVICE", search_text, search_type);
        }
        break;

        case AUDIT_SESSION:
        {
            gl_query_add_match (query, "_AUDIT_SESSION", search_text, search_type);
        }
        break;

        case EXECUTABLE_PATH:
        {
            gl_query_add_match (query, "_EXE", search_text, search_type);
        }
        break;
    }
}

static void
query_set_day_timestamps (GlQuery *query,
                          gint start_day_offset,
                          gint end_day_offset)
{
    GDateTime *now;
    GDateTime *today_start;
    GDateTime *today_end;
    guint64 start_timestamp;
    guint64 end_timestamp;

    now = g_date_time_new_now_local();

    today_start = g_date_time_new_local (g_date_time_get_year (now),
                                         g_date_time_get_month (now),
                                         g_date_time_get_day_of_month (now) - start_day_offset,
                                         23,
                                         59,
                                         59.0);

    start_timestamp = g_date_time_to_unix (today_start) * G_USEC_PER_SEC;

    today_end = g_date_time_new_local (g_date_time_get_year (now),
                                       g_date_time_get_month (now),
                                       g_date_time_get_day_of_month (now) - end_day_offset,
                                       0,
                                       0,
                                       0.0);

    end_timestamp = g_date_time_to_unix (today_end) * G_USEC_PER_SEC;

    gl_query_set_journal_range (query, start_timestamp, end_timestamp);

    g_date_time_unref (now);
    g_date_time_unref (today_start);
    g_date_time_unref (today_end);
}

static void
query_add_journal_range_filter (GlQuery *query,
                                GlEventViewList *view)
{
    GArray *boot_ids;
    GlJournalBootID *boot_id;
    GlEventViewListPrivate *priv;

    boot_ids = gl_event_view_list_get_boot_ids (view);

    priv = gl_event_view_list_get_instance_private (view);

    /* Add Range filters */
    switch (priv->range_group)
    {
        case CURRENT_BOOT:
        {
            /* Get current boot id */
            gchar *boot_match;

            boot_match = get_current_boot_id (priv->boot_match);
            gl_query_add_match (query, "_BOOT_ID", boot_match, SEARCH_TYPE_EXACT);

            g_free (boot_match);
        }
        break;

        case PREVIOUS_BOOT:
        {
            boot_id = &g_array_index (boot_ids, GlJournalBootID, boot_ids->len - 2);

            gl_query_set_journal_range (query, boot_id->realtime_last, boot_id->realtime_first);
        }
        break;

        case TODAY:
        {
            query_set_day_timestamps (query, 0, 0);
        }
        break;

        case YESTERDAY:
        {
            query_set_day_timestamps (query, 1, 1);
        }
        break;

        case LAST_3_DAYS:
        {
            query_set_day_timestamps (query, 0, 2);
        }
        break;

        case ENTIRE_JOURNAL:
        {

        }
        break;

        case SET_CUSTOM_RANGE:
        {
            /* Set the values set in the custom range submenu */
            gl_query_set_journal_range (query, priv->custom_start_timestamp, priv->custom_end_timestamp);
        }
        break;
    }
}

/* Create Query Object according to GUI elements and set it on Journal Model */
static GlQuery *
create_query_object (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;
    GlQuery *query;
    GlCategoryList *list;
    GlCategoryListFilter filter;

    priv = gl_event_view_list_get_instance_private (view);
    list = GL_CATEGORY_LIST (priv->categories);

    /* Create new query object */
    query = gl_query_new ();

    /* Set Journal Range */
    query_add_journal_range_filter (query, view);

    /* Add Exact Matches according to selected category */
    filter = gl_category_list_get_category (list);

    switch (filter)
    {
        case GL_CATEGORY_LIST_FILTER_IMPORTANT:
            {
              /* Alert or emergency priority. */
              gl_query_add_match (query, "PRIORITY", "0", SEARCH_TYPE_EXACT);
              gl_query_add_match (query, "PRIORITY", "1", SEARCH_TYPE_EXACT);
              gl_query_add_match (query, "PRIORITY", "2", SEARCH_TYPE_EXACT);
              gl_query_add_match (query, "PRIORITY", "3", SEARCH_TYPE_EXACT);
            }
            break;

        case GL_CATEGORY_LIST_FILTER_ALL:
            {

            }
            break;

        case GL_CATEGORY_LIST_FILTER_APPLICATIONS:
            /* Allow all _TRANSPORT != kernel. Attempt to filter by only processes
             * owned by the same UID. */
            {
                gchar *uid_str;

                uid_str = get_uid_match_field_value ();

                gl_query_add_match (query, "_TRANSPORT", "journal", SEARCH_TYPE_EXACT);
                gl_query_add_match (query, "_TRANSPORT", "stdout", SEARCH_TYPE_EXACT);
                gl_query_add_match (query, "_TRANSPORT", "syslog", SEARCH_TYPE_EXACT);
                gl_query_add_match (query, "_UID", uid_str, SEARCH_TYPE_EXACT);

                g_free (uid_str);
            }
            break;

        case GL_CATEGORY_LIST_FILTER_SYSTEM:
            {
                gl_query_add_match (query, "_TRANSPORT", "kernel", SEARCH_TYPE_EXACT);
            }
            break;

        case GL_CATEGORY_LIST_FILTER_HARDWARE:
            {
                gl_query_add_match (query, "_TRANSPORT", "kernel", SEARCH_TYPE_EXACT);
                gl_query_add_match ( query, "_KERNEL_DEVICE", NULL, SEARCH_TYPE_EXACT);
            }
            break;

        case GL_CATEGORY_LIST_FILTER_SECURITY:
            {
                gl_query_add_match (query, "_AUDIT_SESSION", NULL, SEARCH_TYPE_EXACT);
            }
            break;

        default:
            g_assert_not_reached ();
    }

    /* Add Substring Matches */
    query_add_search_matches (query, priv->parameter_group, priv->search_text, priv->search_type);

    query->is_search_field_exact = (priv->search_type == SEARCH_TYPE_EXACT);

    return query;
}

static void
on_notify_category (GlCategoryList *list,
                    GParamSpec *pspec,
                    gpointer user_data)
{
    GlEventViewList *view;
    GlEventViewListPrivate *priv;
    GSettings *settings;
    gint sort_order;
    GlQuery *query;

    view = GL_EVENT_VIEW_LIST (user_data);
    priv = gl_event_view_list_get_instance_private (view);

    /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);

    settings = g_settings_new (SETTINGS_SCHEMA);
    sort_order = g_settings_get_enum (settings, SORT_ORDER);
    g_object_unref (settings);
    gl_event_view_list_set_sort_order (view, sort_order);
}

void
gl_event_view_list_view_boot (GlEventViewList *view, const gchar *match)
{
    GlEventViewListPrivate *priv;
    GlCategoryList *categories;

    g_return_if_fail (GL_EVENT_VIEW_LIST (view));

    priv = gl_event_view_list_get_instance_private (view);
    categories = GL_CATEGORY_LIST (priv->categories);
    priv->boot_match = match;

    /* Select "Current Boot" in When label */
    priv->range_group = CURRENT_BOOT;

    gtk_label_set_label (GTK_LABEL (priv->range_button_label), _("Current Boot"));

    gtk_widget_hide (priv->clear_range_button);
    gtk_widget_show (priv->range_button_drop_down_image);

    on_notify_category (categories, NULL, view);
}

static gint
gl_event_view_sort_by_ascending_time (GtkListBoxRow *row1,
                                      GtkListBoxRow *row2)
{
    GlJournalEntry *entry1;
    GlJournalEntry *entry2;
    guint64 time1;
    guint64 time2;

    entry1 = gl_event_view_row_get_entry (GL_EVENT_VIEW_ROW (row1));
    entry2 = gl_event_view_row_get_entry (GL_EVENT_VIEW_ROW (row2));
    time1 = gl_journal_entry_get_timestamp (entry1);
    time2 = gl_journal_entry_get_timestamp (entry2);

    if (time1 > time2)
    {
        return 1;
    }
    else if (time1 < time2)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

static gint
gl_event_view_sort_by_descending_time (GtkListBoxRow *row1,
                                       GtkListBoxRow *row2)
{
    GlJournalEntry *entry1;
    GlJournalEntry *entry2;
    guint64 time1;
    guint64 time2;

    entry1 = gl_event_view_row_get_entry (GL_EVENT_VIEW_ROW (row1));
    entry2 = gl_event_view_row_get_entry (GL_EVENT_VIEW_ROW (row2));
    time1 = gl_journal_entry_get_timestamp (entry1);
    time2 = gl_journal_entry_get_timestamp (entry2);

    if (time1 > time2)
    {
        return -1;
    }
    else if (time1 < time2)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void
gl_event_view_list_set_sort_order (GlEventViewList *view,
                                   GlSortOrder sort_order)
{
    GlEventViewListPrivate *priv;

    g_return_if_fail (GL_EVENT_VIEW_LIST (view));

    priv = gl_event_view_list_get_instance_private (view);

    switch (sort_order)
    {
        case GL_SORT_ORDER_ASCENDING_TIME:
            gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->entries_box),
                                        (GtkListBoxSortFunc) gl_event_view_sort_by_ascending_time,
                                        NULL, NULL);
            break;
        case GL_SORT_ORDER_DESCENDING_TIME:
            gtk_list_box_set_sort_func (GTK_LIST_BOX (priv->entries_box),
                                        (GtkListBoxSortFunc) gl_event_view_sort_by_descending_time,
                                        NULL, NULL);
            break;
        default:
            g_assert_not_reached ();
            break;
    }

}

static void
on_search_entry_changed (GtkSearchEntry *entry,
                         gpointer user_data)
{
    GlEventViewList *view;
    GlEventViewListPrivate *priv;
    GlQuery *query;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (GL_EVENT_VIEW_LIST (user_data));

    g_free (priv->search_text);

    priv->search_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->search_entry)));

    /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);
}

static void
on_search_bar_notify_search_mode_enabled (GtkSearchBar *search_bar,
                                          GParamSpec *pspec,
                                          gpointer user_data)
{
    GAction *search;
    GtkWidget *toplevel;
    GActionMap *appwindow;

    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (user_data));

    if (gtk_widget_is_toplevel (toplevel))
    {
        appwindow = G_ACTION_MAP (toplevel);
        search = g_action_map_lookup_action (appwindow, "search");
    }
    else
    {
        /* TODO: Investigate whether this only happens during dispose. */
        g_debug ("%s",
                 "Search bar activated while not in a toplevel");
        return;
    }

    g_action_change_state (search,
                           g_variant_new_boolean (gtk_search_bar_get_search_mode (search_bar)));
}

static void
gl_event_list_view_edge_reached (GtkScrolledWindow *scrolled,
                                 GtkPositionType    pos,
                                 gpointer           user_data)
{
    GlEventViewList *view = user_data;
    GlEventViewListPrivate *priv = gl_event_view_list_get_instance_private (view);

    if (pos == GTK_POS_BOTTOM)
        gl_journal_model_fetch_more_entries (priv->journal_model, FALSE);
}

/* Event handlers for search popover elements */
static void
search_popover_closed (GtkPopover *popover,
                       gpointer user_data)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (GL_EVENT_VIEW_LIST (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");
}

static void
select_parameter_button_clicked (GtkButton *button,
                                 gpointer user_data)
{
    GlEventViewListPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    gchar *path_string;

    priv = gl_event_view_list_get_instance_private (GL_EVENT_VIEW_LIST (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "select-parameter-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");

    path_string = g_strdup_printf ("%" G_GSIZE_FORMAT ":0", priv->parameter_group);

    path = gtk_tree_path_new_from_string (path_string);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->parameter_treeview));

    gtk_tree_selection_select_path (selection, path);

    gtk_tree_path_free (path);
    g_free (path_string);
}

static gboolean
parameter_treeview_row_seperator (GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  gpointer user_data)
{
    GlEventViewList *view;
    GlEventViewListPrivate *priv;
    gboolean show_seperator;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), iter,
                        2, &show_seperator,
                        -1);

    return show_seperator;
}

static void
on_parameter_treeview_row_activated (GtkTreeView *tree_view,
                                     GtkTreePath *path,
                                     GtkTreeViewColumn *column,
                                     gpointer user_data)
{
    GlQuery *query;
    GtkTreeIter iter;
    GlEventViewList *view;
    GlEventViewListPrivate *priv;
    gchar *parameter_label;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->parameter_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), &iter,
                        0, &parameter_label,
                        1, &priv->parameter_group,
                        -1);

    gtk_label_set_label (GTK_LABEL (priv->parameter_button_label),
                         _(parameter_label));

    /* Do not Show "Search Type" option if all available fields group is selected */
    if (priv->parameter_group == ALL_AVAILABLE_FIELDS)
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->search_type_revealer), FALSE);
        gtk_widget_set_visible (priv->search_type_revealer, FALSE);
    }
    else
    {
        gtk_widget_set_visible (priv->search_type_revealer, TRUE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->search_type_revealer), TRUE);
    }

    /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    g_free (parameter_label);
}

static void
search_type_changed (GtkToggleButton *togglebutton,
                     gpointer user_data)
{
    GlQuery *query;
    GlEventViewList *view;
    GlEventViewListPrivate *priv;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    if (gtk_toggle_button_get_active (togglebutton))
    {
        priv->search_type = SEARCH_TYPE_EXACT;
    }
    else
    {
        priv->search_type = SEARCH_TYPE_SUBSTRING;
    }

     /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);
}

static void
select_range_button_clicked (GtkButton *button,
                             gpointer user_data)
{
    GlEventViewListPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    gchar *path_string;

    priv = gl_event_view_list_get_instance_private (GL_EVENT_VIEW_LIST (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "show-log-from-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    /* Select the row in treeview which was shown in the range button label */
    path_string = g_strdup_printf ("%" G_GSIZE_FORMAT ":0", priv->range_group);

    path = gtk_tree_path_new_from_string (path_string);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->range_treeview));

    gtk_tree_selection_select_path (selection, path);

    gtk_tree_path_free (path);
    g_free (path_string);
}

static void
show_start_date_widgets (GlEventViewList *view, gboolean visible)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->start_date_stack),
                                      visible ? "start-date-entry" : "start-date-button");

    gtk_widget_set_visible (priv->start_date_calendar_revealer, visible);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->start_date_calendar_revealer), visible);
}

static void
show_start_time_widgets (GlEventViewList *view, gboolean visible)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->start_time_stack),
                                      visible ? "start-time-set-button" : "start-time-select-button");

    gtk_widget_set_visible (priv->start_time_spinbox_revealer, visible);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->start_time_spinbox_revealer), visible);
}

static void
show_end_date_widgets (GlEventViewList *view, gboolean visible)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->end_date_stack),
                                      visible ? "end-date-entry" : "end-date-button");

    gtk_widget_set_visible (priv->end_date_calendar_revealer, visible);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->end_date_calendar_revealer), visible);
}

static void
show_end_time_widgets (GlEventViewList *view, gboolean visible)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->end_time_stack),
                                      visible ? "end-time-set-button" : "end-time-select-button");

    gtk_widget_set_visible (priv->end_time_spinbox_revealer, visible);
    gtk_revealer_set_reveal_child (GTK_REVEALER (priv->end_time_spinbox_revealer), visible);
}

static void
reset_custom_range_widgets (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    priv->custom_start_timestamp = 0;
    priv->custom_end_timestamp = 0;

    /* Close any previously opened widgets in the submenu */
    show_start_date_widgets (view, FALSE);
    show_start_time_widgets (view, FALSE);
    show_end_date_widgets (view, FALSE);
    show_end_time_widgets (view, FALSE);

    /* Reset start range elements */
    gtk_entry_set_text (GTK_ENTRY (priv->start_date_entry), "");
    gtk_label_set_label (GTK_LABEL (priv->start_date_button_label), "Select Start Date...");
    gtk_label_set_label (GTK_LABEL (priv->start_time_button_label), "Select Start Time...");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_time_hour_spin), 11);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_time_minute_spin), 59);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_time_second_spin), 59);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->start_time_am_pm_spin), PM);

    /*Reset end range elements */
    gtk_entry_set_text (GTK_ENTRY (priv->end_date_entry), "");
    gtk_label_set_label (GTK_LABEL (priv->end_date_button_label), "Select End Date...");
    gtk_label_set_label (GTK_LABEL (priv->end_time_button_label), "Select End Time...");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_time_hour_spin), 12);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_time_minute_spin), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_time_second_spin), 0);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->end_time_am_pm_spin), AM);
}

static gboolean
range_treeview_row_seperator (GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
    GlEventViewList *view;
    GlEventViewListPrivate *priv;
    gboolean show_seperator;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->range_liststore), iter,
                        2, &show_seperator,
                        -1);

    return show_seperator;
}

static void
on_range_treeview_row_activated (GtkTreeView *tree_view,
                                 GtkTreePath *path,
                                 GtkTreeViewColumn *column,
                                 gpointer user_data)
{
    GlQuery *query;
    GtkTreeIter iter;
    GlEventViewList *view;
    GlEventViewListPrivate *priv;
    gchar *range_label;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->range_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->range_liststore), &iter,
                        0, &range_label,
                        1, &priv->range_group,
                        -1);

    if (priv->range_group == SET_CUSTOM_RANGE)
    {
        gtk_popover_menu_open_submenu (GTK_POPOVER_MENU (priv->search_popover_menu), "custom-range-submenu");
    }
    else
    {
        /* Reset the Custom Range elements if set as only one filter can be applied at time */
        reset_custom_range_widgets (view);

        gtk_label_set_label (GTK_LABEL (priv->range_button_label), _(range_label));

        /* Show "Clear Range" Button if other than "Current Boot" is selected */
        if(priv->range_group == CURRENT_BOOT)
        {
            gtk_widget_hide (priv->clear_range_button);
            gtk_widget_show (priv->range_button_drop_down_image);
        }
        else
        {
            gtk_widget_show (priv->clear_range_button);
            gtk_widget_hide (priv->range_button_drop_down_image);
        }

        /* Create the query object */
        query = create_query_object (view);

        /* Set the created query on the journal model */
        gl_journal_model_take_query (priv->journal_model, query);

        gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
        gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");
    }

    g_free (range_label);
}

static void
clear_range_button_clicked (GtkButton *button,
                            gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);
    GlEventViewListPrivate *priv;
    GlQuery *query;

    priv = gl_event_view_list_get_instance_private (view);

    gtk_widget_hide (priv->clear_range_button);
    gtk_widget_show (priv->range_button_drop_down_image);

    gtk_label_set_label (GTK_LABEL (priv->range_button_label), _("Current Boot"));

    priv->range_group = CURRENT_BOOT;

    /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);
}

static void
start_date_button_clicked (GtkButton *button,
                           gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);

    show_start_date_widgets (view, TRUE);
    show_start_time_widgets (view, FALSE);
    show_end_date_widgets (view, FALSE);
    show_end_time_widgets (view, FALSE);
}

/* Utility function for converting hours from 12 hour format to 24 hour format */
static gint
convert_hour (gint hour, gint ampm)
{
    if (hour == 12 && ampm == AM)
    {
        return 0;
    }
    else if (hour != 12 && ampm == PM)
    {
        return hour + 12;
    }
    else
        return hour;
}

static GDateTime *
get_start_date_time (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;
    const gchar *entry_date;
    GDateTime *now;
    GDateTime *start_date_time;
    GDate *start_date;
    gint hour_12;
    gint hour_24;
    gint minute;
    gint second;
    gint ampm;

    priv = gl_event_view_list_get_instance_private (view);

    entry_date = gtk_entry_get_text (GTK_ENTRY (priv->start_date_entry));

    hour_12 = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->start_time_hour_spin));
    minute = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->start_time_minute_spin));
    second = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->start_time_second_spin));
    ampm = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->start_time_am_pm_spin));

    /* Convert to 24 Hour Format */
    hour_24 = convert_hour (hour_12, ampm);

    /* Parse the date entered into the Text Entry */
    start_date = g_date_new ();
    g_date_set_parse (start_date, entry_date);

    now = g_date_time_new_now_local ();

    /* If Invalid date, then take today's date as default */
    if (!g_date_valid (start_date))
    {
        start_date_time = g_date_time_new_local (g_date_time_get_year (now),
                                                 g_date_time_get_month (now),
                                                 g_date_time_get_day_of_month (now),
                                                 hour_24,
                                                 minute,
                                                 second);
    }
    else
    {
        start_date_time = g_date_time_new_local (g_date_get_year (start_date),
                                                 g_date_get_month (start_date),
                                                 g_date_get_day (start_date),
                                                 hour_24,
                                                 minute,
                                                 second);
    }

    g_date_time_unref (now);
    g_date_free (start_date);

    return start_date_time;
}

static void
update_range_button (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;
    gchar *range_button_label;
    gchar *display_time;
    GDateTime *now;

    priv = gl_event_view_list_get_instance_private (view);

    now = g_date_time_new_now_local ();

    /* Update range button label according to timestamps set in the custom range submenu */
    if (priv->custom_end_timestamp && priv->custom_start_timestamp)
    {
        range_button_label = gl_util_boot_time_to_display (priv->custom_start_timestamp,
                                                           priv->custom_end_timestamp);
    }
    else if (priv->custom_start_timestamp && !priv->custom_end_timestamp)
    {
        display_time = gl_util_timestamp_to_display (priv->custom_start_timestamp,
                                                     now, GL_UTIL_CLOCK_FORMAT_12HR, FALSE);

        range_button_label = g_strdup_printf (_("From %s"), display_time);

        g_free (display_time);
    }
    else
    {
        display_time = gl_util_timestamp_to_display (priv->custom_end_timestamp,
                                                     now, GL_UTIL_CLOCK_FORMAT_12HR, FALSE);

        range_button_label = g_strdup_printf (_("Until %s"), display_time);

        g_free (display_time);
    }

    gtk_label_set_label (GTK_LABEL (priv->range_button_label), range_button_label);

    gtk_widget_show (priv->clear_range_button);
    gtk_widget_hide (priv->range_button_drop_down_image);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");

    g_date_time_unref (now);
    g_free (range_button_label);
}

static void
start_date_calendar_day_selected (GtkCalendar *calendar,
                                  gpointer user_data)
{
    GDateTime *date;
    GDateTime *now;
    guint year, month, day;
    gchar *date_label;
    GlEventViewList *view;
    GlEventViewListPrivate *priv;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    gtk_calendar_get_date (calendar, &year, &month, &day);

    date = g_date_time_new_local (year, month + 1, day, 0, 0, 0);

    date_label = g_date_time_format (date, "%e %B %Y");

    now = g_date_time_new_now_local ();

    /* If a future date, fail silently */
    if (g_date_time_compare (date, now) != 1)
    {
        GDateTime *start_date_time;
        GlQuery *query;

        gtk_entry_set_text (GTK_ENTRY (priv->start_date_entry), date_label);

        gtk_label_set_label (GTK_LABEL (priv->start_date_button_label), date_label);

        start_date_time = get_start_date_time (view);

        priv->custom_start_timestamp = g_date_time_to_unix (start_date_time) * G_USEC_PER_SEC;

        update_range_button (view);

        /* Create the query object */
        query = create_query_object (view);

        /* Set the created query on the journal model */
        gl_journal_model_take_query (priv->journal_model, query);

        g_date_time_unref (start_date_time);
    }

    g_date_time_unref (date);
}

static void
start_date_entry_activate (GtkEntry *entry,
                           gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);

    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    if (gtk_entry_get_text_length (entry) > 0)
    {
        GDateTime *now;
        GDateTime *date_time;
        GDate *date;

        date = g_date_new ();
        g_date_set_parse (date, gtk_entry_get_text (entry));

        /* Invalid date silently does nothing */
        if (!g_date_valid (date))
        {
            g_date_free (date);
            return;
        }

        now = g_date_time_new_now_local ();
        date_time = g_date_time_new_local (g_date_get_year (date),
                                           g_date_get_month (date),
                                           g_date_get_day (date),
                                           0,
                                           0,
                                           0);

        /* Future dates silently fail */
        if (g_date_time_compare (date_time, now) != 1)
        {
            GDateTime *start_date_time;
            GlQuery *query;

            gtk_label_set_label (GTK_LABEL (priv->start_date_button_label), gtk_entry_get_text(entry));

            show_start_date_widgets (view, FALSE);

            start_date_time = get_start_date_time (view);

            priv->custom_start_timestamp = g_date_time_to_unix (start_date_time) * G_USEC_PER_SEC;

            update_range_button (view);

            /* Create the query object */
            query = create_query_object (view);

            /* Set the created query on the journal model */
            gl_journal_model_take_query (priv->journal_model, query);

            g_date_time_unref (start_date_time);
        }

        g_date_time_unref (now);
        g_date_time_unref (date_time);
        g_date_free (date);
    }
}

static void
start_time_button_clicked (GtkButton *button,
                           gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);

    show_start_time_widgets (view, TRUE);
    show_start_date_widgets (view, FALSE);
    show_end_time_widgets (view, FALSE);
    show_end_date_widgets (view, FALSE);
}

static void
start_time_set_button_clicked (GtkButton *button,
                               gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);
    GlEventViewListPrivate *priv;
    GDateTime *start_date_time;
    gchar *button_label;
    GlQuery *query;

    priv = gl_event_view_list_get_instance_private (view);

    start_date_time = get_start_date_time (view);

    button_label = g_date_time_format (start_date_time, "%I:%M:%S %p");

    gtk_label_set_label (GTK_LABEL (priv->start_time_button_label), button_label);

    priv->custom_start_timestamp = g_date_time_to_unix (start_date_time) * G_USEC_PER_SEC;

    update_range_button (view);

    /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);

    show_start_time_widgets (view, FALSE);

    g_date_time_unref (start_date_time);
}

static void
format_time_to_two_digits (GtkSpinButton *spin_button)
{
    gchar *time_string;
    gint value;

    value = gtk_spin_button_get_value_as_int (spin_button);

    time_string = g_strdup_printf ("%02d", value);

    gtk_entry_set_text (GTK_ENTRY (spin_button), time_string);

    g_free (time_string);
}

static void
roundoff_invalid_time_value (GtkSpinButton *spin_button,
                             gdouble *new_val,
                             gint lower_limit,
                             gint upper_limit)
{
    const gchar *entry_value;
    gint time;

    entry_value = gtk_entry_get_text (GTK_ENTRY (spin_button));
    time = atoi (entry_value);

    /* Roundoff to the nearest limit if out of limits*/
    if (time < lower_limit)
    {
        *new_val = upper_limit;
    }
    else if (time > upper_limit)
    {
        *new_val = upper_limit;
    }
    else
    {
        *new_val = time;
    }
}

static void
spinbox_format_time_period_to_text (GtkSpinButton *spin_button)
{
    gchar *ampm_string;
    gint value;

    value = gtk_spin_button_get_value_as_int (spin_button);

    if (value == AM)
    {
        ampm_string = g_strdup_printf ("AM");
    }
    else
    {
        ampm_string = g_strdup_printf ("PM");
    }

    gtk_entry_set_text (GTK_ENTRY (spin_button), ampm_string);

    g_free (ampm_string);
}

static void
spinbox_format_time_period_to_int (GtkSpinButton *spin_button,
                                   gdouble *new_val)
{
    const gchar *entry_value;

    entry_value = gtk_entry_get_text (GTK_ENTRY (spin_button));

    if ( g_strcmp0 ("PM", entry_value) == 0)
    {
        *new_val = PM;
    }
    else
    {
        *new_val = AM;
    }
}

static gint
start_time_ampm_spin_input (GtkSpinButton *spin_button,
                            gdouble *new_val,
                            gpointer user_data)
{
    spinbox_format_time_period_to_int (spin_button, new_val);
    return TRUE;
}

static gboolean
start_time_ampm_spin_output (GtkSpinButton *spin_button,
                             gpointer data)
{
    spinbox_format_time_period_to_text (spin_button);
    return TRUE;
}

static gint
start_time_minute_spin_input (GtkSpinButton *spin_button,
                              gdouble *new_val,
                              gpointer user_data)
{
    roundoff_invalid_time_value (spin_button, new_val, 0, 59);
    return TRUE;
}

static gboolean
start_time_minute_spin_output (GtkSpinButton *spin_button,
                               gpointer data)
{
    format_time_to_two_digits (spin_button);
    return TRUE;
}

static gint
start_time_second_spin_input (GtkSpinButton *spin_button,
                              gdouble *new_val,
                              gpointer user_data)
{
    roundoff_invalid_time_value (spin_button, new_val, 0, 59);
    return TRUE;
}

static gboolean
start_time_second_spin_output (GtkSpinButton *spin_button,
                               gpointer data)
{
    format_time_to_two_digits (spin_button);
    return TRUE;
}

static gint
start_time_hour_spin_input (GtkSpinButton *spin_button,
                            gdouble *new_val,
                            gpointer user_data)
{
    roundoff_invalid_time_value (spin_button, new_val, 1, 12);
    return TRUE;
}

static gboolean
start_time_hour_spin_output (GtkSpinButton *spin_button,
                             gpointer data)
{
    format_time_to_two_digits (spin_button);
    return TRUE;
}

static void
end_date_button_clicked (GtkButton *button,
                         gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);

    show_end_date_widgets (view, TRUE);
    show_end_time_widgets (view, FALSE);
    show_start_date_widgets (view, FALSE);
    show_start_time_widgets (view, FALSE);
}

static GDateTime *
get_end_date_time (GlEventViewList *view)
{
    GlEventViewListPrivate *priv;
    const gchar *entry_date;
    GDateTime *now;
    GDateTime *end_date_time;
    GDate *end_date;
    gint hour_12;
    gint hour_24;
    gint minute;
    gint second;
    gint ampm;

    priv = gl_event_view_list_get_instance_private (view);

    entry_date = gtk_entry_get_text (GTK_ENTRY (priv->end_date_entry));

    hour_12 = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->end_time_hour_spin));
    minute = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->end_time_minute_spin));
    second = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->end_time_second_spin));
    ampm = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->end_time_am_pm_spin));

    /* Convert to 24 Hour Format */
    hour_24 = convert_hour (hour_12, ampm);

    /* Parse the date entered into the Text Entry */
    end_date = g_date_new ();
    g_date_set_parse (end_date, entry_date);

    now = g_date_time_new_now_local ();

    /* If Invalid date, then take today's date as default */
    if (!g_date_valid (end_date))
    {
        end_date_time = g_date_time_new_local (g_date_time_get_year (now),
                                               g_date_time_get_month (now),
                                               g_date_time_get_day_of_month (now),
                                               hour_24,
                                               minute,
                                               second);
    }
    else
    {
        end_date_time = g_date_time_new_local (g_date_get_year (end_date),
                                               g_date_get_month (end_date),
                                               g_date_get_day (end_date),
                                               hour_24,
                                               minute,
                                               second);
    }

    g_date_time_unref (now);
    g_date_free (end_date);

    return end_date_time;
}

static void
end_date_calendar_day_selected (GtkCalendar *calendar,
                                gpointer user_data)
{
    GDateTime *date;
    GDateTime *now;
    guint year, month, day;
    gchar *date_label;
    GlEventViewList *view;
    GlEventViewListPrivate *priv;

    view = GL_EVENT_VIEW_LIST (user_data);

    priv = gl_event_view_list_get_instance_private (view);

    gtk_calendar_get_date (calendar, &year, &month, &day);

    date = g_date_time_new_local (year, month + 1, day, 0, 0, 0);

    date_label = g_date_time_format (date, "%e %B %Y");

    now = g_date_time_new_now_local ();

    /* If a future date, fail silently */
    if (g_date_time_compare (date, now) != 1)
    {
        GDateTime *end_date_time;
        GlQuery *query;

        gtk_entry_set_text (GTK_ENTRY (priv->end_date_entry), date_label);

        gtk_label_set_label (GTK_LABEL (priv->end_date_button_label), date_label);

        end_date_time = get_end_date_time (view);

        priv->custom_end_timestamp = g_date_time_to_unix (end_date_time) * G_USEC_PER_SEC;

        update_range_button (view);

        /* Create the query object */
        query = create_query_object (view);

        /* Set the created query on the journal model */
        gl_journal_model_take_query (priv->journal_model, query);

        g_date_time_unref (end_date_time);
    }

    g_date_time_unref (date);
}

static void
end_date_entry_activate (GtkEntry *entry,
                         gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);

    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    if (gtk_entry_get_text_length (entry) > 0)
    {
        GDateTime *now;
        GDateTime *date_time;
        GDate *date;

        date = g_date_new ();
        g_date_set_parse (date, gtk_entry_get_text (entry));

        /* Invalid date silently does nothing */
        if (!g_date_valid (date))
        {
            g_date_free (date);
            return;
        }

        now = g_date_time_new_now_local ();
        date_time = g_date_time_new_local (g_date_get_year (date),
                                           g_date_get_month (date),
                                           g_date_get_day (date),
                                           0,
                                           0,
                                           0);

        /* Future dates silently fails */
        if (g_date_time_compare (date_time, now) != 1)
        {
            GDateTime *end_date_time;
            GlQuery *query;

            gtk_label_set_label (GTK_LABEL (priv->end_date_button_label), gtk_entry_get_text(entry));

            show_end_date_widgets (view, FALSE);

            end_date_time = get_end_date_time (view);

            priv->custom_end_timestamp = g_date_time_to_unix (end_date_time) * G_USEC_PER_SEC;

            update_range_button (view);

            /* Create the query object */
            query = create_query_object (view);

            /* Set the created query on the journal model */
            gl_journal_model_take_query (priv->journal_model, query);

            g_date_time_unref (end_date_time);
        }

        g_date_time_unref (now);
        g_date_time_unref (date_time);
        g_date_free (date);
    }
}

static void
end_time_button_clicked (GtkButton *button,
                         gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);

    show_end_time_widgets (view, TRUE);
    show_end_date_widgets (view, FALSE);
    show_start_time_widgets (view, FALSE);
    show_start_date_widgets (view, FALSE);
}

static void
end_time_set_button_clicked (GtkButton *button,
                             gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);
    GlEventViewListPrivate *priv;
    GDateTime *end_date_time;
    gchar *button_label;
    GlQuery *query;

    priv = gl_event_view_list_get_instance_private (view);

    end_date_time = get_end_date_time (view);

    button_label = g_date_time_format (end_date_time, "%I:%M:%S %p");

    gtk_label_set_label (GTK_LABEL (priv->end_time_button_label), button_label);

    priv->custom_end_timestamp = g_date_time_to_unix (end_date_time) * G_USEC_PER_SEC;

    update_range_button (view);

    /* Create the query object */
    query = create_query_object (view);

    /* Set the created query on the journal model */
    gl_journal_model_take_query (priv->journal_model, query);

    show_end_time_widgets (view, FALSE);

    g_date_time_unref (end_date_time);
}

static gint
end_time_ampm_spin_input (GtkSpinButton *spin_button,
                          gdouble *new_val,
                          gpointer user_data)
{
    spinbox_format_time_period_to_int (spin_button, new_val);
    return TRUE;
}

static gboolean
end_time_ampm_spin_output (GtkSpinButton *spin_button,
                           gpointer data)
{
    spinbox_format_time_period_to_text (spin_button);
    return TRUE;
}

static gint
end_time_minute_spin_input (GtkSpinButton *spin_button,
                            gdouble *new_val,
                            gpointer user_data)
{
    roundoff_invalid_time_value (spin_button, new_val, 0, 59);
    return TRUE;
}

static gboolean
end_time_minute_spin_output (GtkSpinButton *spin_button,
                             gpointer data)
{
    format_time_to_two_digits (spin_button);
    return TRUE;
}

static gint
end_time_second_spin_input (GtkSpinButton *spin_button,
                            gdouble *new_val,
                            gpointer user_data)
{
    roundoff_invalid_time_value (spin_button, new_val, 0, 59);
    return TRUE;
}

static gboolean
end_time_second_spin_output (GtkSpinButton *spin_button,
                             gpointer data)
{
    format_time_to_two_digits (spin_button);
    return TRUE;
}

static gint
end_time_hour_spin_input (GtkSpinButton *spin_button,
                          gdouble *new_val,
                          gpointer user_data)
{
    roundoff_invalid_time_value (spin_button, new_val, 1, 12);
    return TRUE;
}

static gboolean
end_time_hour_spin_output (GtkSpinButton *spin_button,
                           gpointer data)
{
    format_time_to_two_digits (spin_button);
    return TRUE;
}

static void
custom_range_submenu_back_button_clicked (GtkButton *button,
                                          gpointer user_data)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (user_data);
    GlEventViewListPrivate *priv;

    priv = gl_event_view_list_get_instance_private (view);

    /* Default to Current boot if none of the timestamp was set */
    if (!priv->custom_start_timestamp && !priv->custom_end_timestamp)
    {
        gchar *range_button_label;
        GlQuery *query;

        range_button_label = g_strdup_printf (_("Current Boot"));

        gtk_label_set_label (GTK_LABEL (priv->range_button_label), range_button_label);

        gtk_widget_hide (priv->clear_range_button);
        gtk_widget_show (priv->range_button_drop_down_image);

        priv->range_group = CURRENT_BOOT;

        /* Create the query object */
        query = create_query_object (view);

        /* Set the created query on the journal model */
        gl_journal_model_take_query (priv->journal_model, query);

        g_free (range_button_label);
    }

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");
}

/* Get the view elements from ui file and link it with the drop down button */
static void
setup_search_popover (GlEventViewList *view)
{

    GlEventViewListPrivate *priv;
    GtkBuilder *builder;

    priv = gl_event_view_list_get_instance_private (view);

    builder = gtk_builder_new_from_resource ("/org/gnome/Logs/gl-searchpopover.ui");

    /* Get elements from the view ui file */
    priv->search_popover_menu = GTK_WIDGET (gtk_builder_get_object (builder, "search_popover_menu"));

    /* elements related to "what" parameter filter label */
    priv->parameter_treeview = GTK_WIDGET (gtk_builder_get_object (builder, "parameter_treeview"));
    priv->parameter_stack = GTK_WIDGET (gtk_builder_get_object (builder, "parameter_stack"));
    priv->parameter_label_stack = GTK_WIDGET (gtk_builder_get_object (builder, "parameter_label_stack"));
    priv->parameter_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "parameter_button_label"));

    priv->parameter_liststore = GTK_LIST_STORE (gtk_builder_get_object (builder, "parameter_liststore"));

    /* elements related to "search type" label */
    priv->search_type_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "search_type_revealer"));

    /* elements related to "when" range filter label */
    priv->range_stack = GTK_WIDGET (gtk_builder_get_object (builder, "range_stack"));
    priv->range_label_stack = GTK_WIDGET (gtk_builder_get_object (builder, "range_label_stack"));
    priv->range_treeview = GTK_WIDGET (gtk_builder_get_object (builder, "range_treeview"));
    priv->range_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "range_button_label"));
    priv->clear_range_button = GTK_WIDGET (gtk_builder_get_object (builder, "clear_range_button"));
    priv->range_button_drop_down_image = GTK_WIDGET (gtk_builder_get_object (builder, "range_button_drop_down_image"));

    priv->range_liststore = GTK_LIST_STORE (gtk_builder_get_object (builder, "range_liststore"));

    /* elements related to "Set Custom Range" submenu */
    priv->start_date_stack = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_stack"));
    priv->start_date_calendar_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_calendar_revealer"));
    priv->start_date_button_drop_down_image = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_button_drop_down_image"));
    priv->start_date_entry = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_entry"));
    priv->start_date_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "start_date_button_label"));

    priv->start_time_spinbox_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_spinbox_revealer"));
    priv->start_time_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_button_label"));
    priv->start_time_stack = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_stack"));
    priv->start_time_button_drop_down_image = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_button_drop_down_image"));
    priv->start_time_hour_spin = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_hour_spin"));
    priv->start_time_minute_spin = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_minute_spin"));
    priv->start_time_second_spin = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_second_spin"));
    priv->start_time_am_pm_spin = GTK_WIDGET (gtk_builder_get_object (builder, "start_time_am_pm_spin"));

    priv->end_date_stack = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_stack"));
    priv->end_date_calendar_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_calendar_revealer"));
    priv->end_date_button_drop_down_image = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_button_drop_down_image"));
    priv->end_date_entry = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_entry"));
    priv->end_date_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "end_date_button_label"));

    priv->end_time_spinbox_revealer = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_spinbox_revealer"));
    priv->end_time_button_label = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_button_label"));
    priv->end_time_stack = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_stack"));
    priv->end_time_button_drop_down_image = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_button_drop_down_image"));
    priv->end_time_hour_spin = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_hour_spin"));
    priv->end_time_minute_spin = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_minute_spin"));
    priv->end_time_second_spin = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_second_spin"));
    priv->end_time_am_pm_spin = GTK_WIDGET (gtk_builder_get_object (builder, "end_time_am_pm_spin"));

    /* Connect signals */
    gtk_builder_add_callback_symbols (builder,
                                      "select_parameter_button_clicked",
                                      G_CALLBACK (select_parameter_button_clicked),
                                      "on_parameter_treeview_row_activated",
                                      G_CALLBACK (on_parameter_treeview_row_activated),
                                      "search_popover_closed",
                                      G_CALLBACK (search_popover_closed),
                                      "search_type_changed",
                                      G_CALLBACK (search_type_changed),
                                      "select_range_button_clicked",
                                      G_CALLBACK (select_range_button_clicked),
                                      "on_range_treeview_row_activated",
                                      G_CALLBACK (on_range_treeview_row_activated),
                                      "clear_range_button_clicked",
                                      G_CALLBACK (clear_range_button_clicked),
                                      "start_date_button_clicked",
                                      G_CALLBACK (start_date_button_clicked),
                                      "start_date_calendar_day_selected",
                                      G_CALLBACK (start_date_calendar_day_selected),
                                      "start_date_entry_activate",
                                      G_CALLBACK (start_date_entry_activate),
                                      "start_time_button_clicked",
                                      G_CALLBACK (start_time_button_clicked),
                                      "start_time_set_button_clicked",
                                      G_CALLBACK (start_time_set_button_clicked),
                                      "start_time_ampm_spin_input",
                                      G_CALLBACK (start_time_ampm_spin_input),
                                      "start_time_ampm_spin_output",
                                      G_CALLBACK (start_time_ampm_spin_output),
                                      "start_time_minute_spin_input",
                                      G_CALLBACK (start_time_minute_spin_input),
                                      "start_time_minute_spin_output",
                                      G_CALLBACK (start_time_minute_spin_output),
                                      "start_time_second_spin_input",
                                      G_CALLBACK (start_time_second_spin_input),
                                      "start_time_second_spin_output",
                                      G_CALLBACK (start_time_second_spin_output),
                                      "start_time_hour_spin_input",
                                      G_CALLBACK (start_time_hour_spin_input),
                                      "start_time_hour_spin_output",
                                      G_CALLBACK (start_time_hour_spin_output),
                                      "end_date_button_clicked",
                                      G_CALLBACK (end_date_button_clicked),
                                      "end_date_calendar_day_selected",
                                      G_CALLBACK (end_date_calendar_day_selected),
                                      "end_date_entry_activate",
                                      G_CALLBACK (end_date_entry_activate),
                                      "end_time_button_clicked",
                                      G_CALLBACK (end_time_button_clicked),
                                      "end_time_set_button_clicked",
                                      G_CALLBACK (end_time_set_button_clicked),
                                      "end_time_ampm_spin_input",
                                      G_CALLBACK (end_time_ampm_spin_input),
                                      "end_time_ampm_spin_output",
                                      G_CALLBACK (end_time_ampm_spin_output),
                                      "end_time_minute_spin_input",
                                      G_CALLBACK (end_time_minute_spin_input),
                                      "end_time_minute_spin_output",
                                      G_CALLBACK (end_time_minute_spin_output),
                                      "end_time_second_spin_input",
                                      G_CALLBACK (end_time_second_spin_input),
                                      "end_time_second_spin_output",
                                      G_CALLBACK (end_time_second_spin_output),
                                      "end_time_hour_spin_input",
                                      G_CALLBACK (end_time_hour_spin_input),
                                      "end_time_hour_spin_output",
                                      G_CALLBACK (end_time_hour_spin_output),
                                      "custom_range_submenu_back_button_clicked",
                                      G_CALLBACK (custom_range_submenu_back_button_clicked),
                                      NULL);

    /* pass "GlEventviewlist *view" as user_data to signals as callback data*/
    gtk_builder_connect_signals (builder, view);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->parameter_treeview),
                                          (GtkTreeViewRowSeparatorFunc) parameter_treeview_row_seperator,
                                          view,
                                          NULL);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->range_treeview),
                                          (GtkTreeViewRowSeparatorFunc) range_treeview_row_seperator,
                                          view,
                                          NULL);

    /* Grab/Remove keyboard focus from popover menu when it is opened or closed */
    g_signal_connect (priv->search_popover_menu, "show", (GCallback) gtk_widget_grab_focus, NULL);
    g_signal_connect_swapped (priv->search_popover_menu, "closed", (GCallback) gtk_widget_grab_focus, view);

    /* Link the drop down button with search popover */
    gtk_menu_button_set_popover (GTK_MENU_BUTTON (priv->search_dropdown_button),
                                 priv->search_popover_menu);

    /* Set "All Available Fields" as default option in the select parameter button */
    priv->parameter_group = ALL_AVAILABLE_FIELDS;

    /* Set Substring search as the default search type */
    priv->search_type = SEARCH_TYPE_SUBSTRING;

    /* Set "Current Boot" as the default journal range */
    priv->range_group = CURRENT_BOOT;

    /* Set 0 as default for Custom timestamps */
    priv->custom_start_timestamp = 0;
    priv->custom_end_timestamp = 0;

    g_object_unref (builder);
}

static void
gl_event_view_list_finalize (GObject *object)
{
    GlEventViewList *view = GL_EVENT_VIEW_LIST (object);
    GlEventViewListPrivate *priv = gl_event_view_list_get_instance_private (view);

    g_clear_object (&priv->journal_model);
    g_clear_pointer (&priv->search_text, g_free);
    g_object_unref (priv->category_sizegroup);
    g_object_unref (priv->message_sizegroup);
    g_object_unref (priv->time_sizegroup);
}

static void
gl_event_view_list_class_init (GlEventViewListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = gl_event_view_list_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Logs/gl-eventviewlist.ui");
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewList,
                                                  entries_box);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewList,
                                                  categories);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewList,
                                                  event_search);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewList,
                                                  event_scrolled);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewList,
                                                  search_entry);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewList,
                                                  search_dropdown_button);

    gtk_widget_class_bind_template_callback (widget_class,
                                             on_search_entry_changed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_search_bar_notify_search_mode_enabled);
}

static void
gl_event_view_list_init (GlEventViewList *view)
{
    GlCategoryList *categories;
    GlEventViewListPrivate *priv;
    GSettings *settings;

    gtk_widget_init_template (GTK_WIDGET (view));

    priv = gl_event_view_list_get_instance_private (view);

    priv->search_text = NULL;
    priv->boot_match = NULL;
    priv->category_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->message_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    priv->time_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    categories = GL_CATEGORY_LIST (priv->categories);

    priv->journal_model = gl_journal_model_new ();
    g_application_bind_busy_property (g_application_get_default (), priv->journal_model, "loading");

    setup_search_popover (view);

    g_signal_connect (priv->event_scrolled, "edge-reached",
                      G_CALLBACK (gl_event_list_view_edge_reached), view);

    gtk_list_box_bind_model (GTK_LIST_BOX (priv->entries_box),
                             G_LIST_MODEL (priv->journal_model),
                             gl_event_list_view_create_row_widget,
                             view, NULL);

    gtk_list_box_set_header_func (GTK_LIST_BOX (priv->entries_box),
                                  (GtkListBoxUpdateHeaderFunc) listbox_update_header_func,
                                  NULL, NULL);
    gtk_list_box_set_placeholder (GTK_LIST_BOX (priv->entries_box),
                                  gl_event_view_create_empty (view));
    g_signal_connect (priv->entries_box, "row-activated",
                      G_CALLBACK (on_listbox_row_activated), GTK_BOX (view));

    /* TODO: Monitor and propagate any GSettings changes. */
    settings = g_settings_new (DESKTOP_SCHEMA);
    priv->clock_format = g_settings_get_enum (settings, CLOCK_FORMAT);
    g_object_unref (settings);

    g_signal_connect (categories, "notify::category", G_CALLBACK (on_notify_category),
                      view);
}

void
gl_event_view_list_search (GlEventViewList *view,
                           const gchar *needle)
{
    GlEventViewListPrivate *priv;

    g_return_if_fail (GL_EVENT_VIEW_LIST (view));

    priv = gl_event_view_list_get_instance_private (view);

    g_free (priv->search_text);
    priv->search_text = g_strdup (needle);

    /* for search, we need all entries - tell the model to fetch them */
    gl_journal_model_fetch_more_entries (priv->journal_model, TRUE);

    gtk_list_box_invalidate_filter (priv->entries_box);
}

GtkWidget *
gl_event_view_list_new (void)
{
    return g_object_new (GL_TYPE_EVENT_VIEW_LIST, NULL);
}
