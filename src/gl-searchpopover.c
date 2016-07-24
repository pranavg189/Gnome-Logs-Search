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

#include "gl-searchpopover.h"
#include "gl-enums.h"

#include <glib/gi18n.h>

struct _GlSearchPopover
{
    /*< private >*/
    GtkPopover parent_instance;
};

typedef struct
{
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

    GlSearchPopoverJournalFieldFilter journal_search_field;
    gulong journal_field_row;

    GlQuerySearchType search_type;

    GlSearchPopoverJournalTimestampRange journal_timestamp_range;
    gulong journal_range_row;
} GlSearchPopoverPrivate;

enum
{
    PROP_0,
    PROP_JOURNAL_SEARCH_FIELD,
    PROP_SEARCH_TYPE,
    PROP_JOURNAL_TIMESTAMP_RANGE,
    N_PROPERTIES
};

enum
{
    COLUMN_JOURNAL_FIELD_NAME,
    COLUMN_JOURNAL_FIELD_INDEX,
    COLUMN_SHOW_SEPARATOR,
    COLUMN_JOURNAL_FIELD_ENUM_NICK,
    N_COLUMNS
};

enum
{
    COLUMN_JOURNAL_TIMESTAMP_RANGE_NAME,
    COLUMN_JOURNAL_TIMESTAMP_RANGE_INDEX,
    COLUMN_JOURNAL_TIMESTAMP_RANGE_SHOW_SEPARATOR,
    COLUMN_JOURNAL_TIMESTAMP_RANGE_ENUM_NICK,
    JOURNAL_TIMESTAMP_RANGE_N_COLUMNS
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GlSearchPopover, gl_search_popover, GTK_TYPE_POPOVER)

/* Event handlers for search popover elements */
static void
search_popover_closed (GtkPopover *popover,
                       gpointer user_data)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");
}

static void
select_parameter_button_clicked (GtkButton *button,
                                 gpointer user_data)
{
    GlSearchPopoverPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    gchar *path_string;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "select-parameter-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");

    path_string = g_strdup_printf ("%lu:0", priv->journal_field_row);

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
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gboolean show_separator;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), iter,
                        COLUMN_SHOW_SEPARATOR, &show_separator,
                        -1);

    return show_separator;
}

static void
on_parameter_treeview_row_activated (GtkTreeView *tree_view,
                                     GtkTreePath *path,
                                     GtkTreeViewColumn *column,
                                     gpointer user_data)
{
    GtkTreeIter iter;
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gchar *parameter_label;
    gchar *search_field_enum_nick;
    GEnumClass *eclass;
    GEnumValue *evalue;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->parameter_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), &iter,
                        COLUMN_JOURNAL_FIELD_NAME, &parameter_label,
                        COLUMN_JOURNAL_FIELD_INDEX, &priv->journal_field_row,
                        COLUMN_JOURNAL_FIELD_ENUM_NICK, &search_field_enum_nick,
                        -1);

    gtk_label_set_label (GTK_LABEL (priv->parameter_button_label),
                         _(parameter_label));

    eclass = g_type_class_ref (GL_TYPE_SEARCH_POPOVER_JOURNAL_FIELD_FILTER);

    if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_ALL_AVAILABLE_FIELDS", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_ALL_AVAILABLE_FIELDS);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PID", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PID);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_UID", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_UID);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_GID", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_GID);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_MESSAGE", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_MESSAGE);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PROCESS_NAME", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PROCESS_NAME);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_SYSTEMD_UNIT", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_SYSTEMD_UNIT);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_KERNEL_DEVICE", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_KERNEL_DEVICE);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_AUDIT_SESSION", search_field_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_AUDIT_SESSION);
    }
    else
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_EXECUTABLE_PATH);
    }

    priv->journal_search_field = evalue->value;

    g_object_notify_by_pspec (G_OBJECT (popover),
                              obj_properties[PROP_JOURNAL_SEARCH_FIELD]);

    /* Do not Show "Search Type" option if all available fields group is selected */
    if (priv->journal_search_field == GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_ALL_AVAILABLE_FIELDS)
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->search_type_revealer), FALSE);
        gtk_widget_set_visible (priv->search_type_revealer, FALSE);
    }
    else
    {
        gtk_widget_set_visible (priv->search_type_revealer, TRUE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->search_type_revealer), TRUE);
    }

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    g_free (parameter_label);
    g_type_class_unref (eclass);
}

static void
search_type_changed (GtkToggleButton *togglebutton,
                     gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    GEnumClass *eclass;
    GEnumValue *evalue;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);
    eclass = g_type_class_ref (GL_TYPE_QUERY_SEARCH_TYPE);

    if (gtk_toggle_button_get_active (togglebutton))
    {
        evalue = g_enum_get_value (eclass, GL_QUERY_SEARCH_TYPE_EXACT);
    }
    else
    {
        evalue = g_enum_get_value (eclass, GL_QUERY_SEARCH_TYPE_SUBSTRING);
    }

    priv->search_type = evalue->value;

    /* Inform GlEventViewlist about search type property change */
    g_object_notify_by_pspec (G_OBJECT (popover),
                              obj_properties[PROP_SEARCH_TYPE]);

    g_type_class_unref (eclass);
}

GlSearchPopoverJournalFieldFilter
gl_search_popover_get_journal_search_field (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (popover);

    return priv->journal_search_field;
}

GlQuerySearchType
gl_search_popover_get_query_search_type (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (popover);

    return priv->search_type;
}

GlSearchPopoverJournalTimestampRange
gl_search_popover_get_journal_timestamp_range (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (popover);

    return priv->journal_timestamp_range;
}

static void
gl_search_popover_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (object);
    GlSearchPopoverPrivate *priv = gl_search_popover_get_instance_private (popover);

    switch (prop_id)
    {
        case PROP_JOURNAL_SEARCH_FIELD:
            g_value_set_enum (value, priv->journal_search_field);
            break;
        case PROP_SEARCH_TYPE:
            g_value_set_enum (value, priv->search_type);
            break;
        case PROP_JOURNAL_TIMESTAMP_RANGE:
            g_value_set_enum (value, priv->journal_timestamp_range);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gl_search_popover_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (object);
    GlSearchPopoverPrivate *priv = gl_search_popover_get_instance_private (popover);

    switch (prop_id)
    {
        case PROP_JOURNAL_SEARCH_FIELD:
            priv->journal_search_field = g_value_get_enum (value);
            break;
        case PROP_SEARCH_TYPE:
            priv->search_type = g_value_get_enum (value);
            break;
        case PROP_JOURNAL_TIMESTAMP_RANGE:
            priv->journal_timestamp_range = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
select_range_button_clicked (GtkButton *button,
                             gpointer user_data)
{
    GlSearchPopoverPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    gchar *path_string;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "show-log-from-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    /* Select the row in treeview which was shown in the range button label */
    path_string = g_strdup_printf ("%lu:0", priv->journal_range_row);

    path = gtk_tree_path_new_from_string (path_string);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->range_treeview));

    gtk_tree_selection_select_path (selection, path);

    gtk_tree_path_free (path);
    g_free (path_string);
}

static gboolean
range_treeview_row_seperator (GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gboolean show_seperator;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->range_liststore), iter,
                        COLUMN_JOURNAL_TIMESTAMP_RANGE_SHOW_SEPARATOR, &show_seperator,
                        -1);

    return show_seperator;
}

static void
on_range_treeview_row_activated (GtkTreeView *tree_view,
                                 GtkTreePath *path,
                                 GtkTreeViewColumn *column,
                                 gpointer user_data)
{
    GtkTreeIter iter;
    gchar *range_label;
    gchar *journal_range_enum_nick;
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    GEnumClass *eclass;
    GEnumValue *evalue;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->range_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->range_liststore), &iter,
                        COLUMN_JOURNAL_TIMESTAMP_RANGE_NAME, &range_label,
                        COLUMN_JOURNAL_TIMESTAMP_RANGE_ENUM_NICK, &journal_range_enum_nick,
                        COLUMN_JOURNAL_TIMESTAMP_RANGE_INDEX, &priv->journal_range_row,
                        -1);

    gtk_label_set_label (GTK_LABEL (priv->range_button_label),
                         _(range_label));

    eclass = g_type_class_ref (GL_TYPE_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE);

    if (strstr ("GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_CURRENT_BOOT", journal_range_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_CURRENT_BOOT);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_PREVIOUS_BOOT", journal_range_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_PREVIOUS_BOOT);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_TODAY", journal_range_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_TODAY);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_YESTERDAY", journal_range_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_YESTERDAY);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_LAST_3_DAYS", journal_range_enum_nick))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_LAST_3_DAYS);
    }
    else
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_ENTIRE_JOURNAL);
    }

    priv->journal_timestamp_range = evalue->value;

    g_object_notify_by_pspec (G_OBJECT (popover),
                              obj_properties[PROP_JOURNAL_TIMESTAMP_RANGE]);

    /* Show "Clear Range" Button if other than "Current Boot" is selected */
    if(priv->journal_timestamp_range == GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_CURRENT_BOOT)
    {
        gtk_widget_hide (priv->clear_range_button);
        gtk_widget_show (priv->range_button_drop_down_image);
    }
    else
    {
        gtk_widget_show (priv->clear_range_button);
        gtk_widget_hide (priv->range_button_drop_down_image);
    }

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");

    g_free (range_label);
}

static void
clear_range_button_clicked (GtkButton *button,
                            gpointer user_data)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (user_data);
    GlSearchPopoverPrivate *priv;
    GEnumClass *eclass;
    GEnumValue *evalue;

    priv = gl_search_popover_get_instance_private (popover);

    gtk_widget_hide (priv->clear_range_button);
    gtk_widget_show (priv->range_button_drop_down_image);

    gtk_label_set_label (GTK_LABEL (priv->range_button_label),
                         _("Current Boot"));

    eclass = g_type_class_ref (GL_TYPE_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE);

    /* Set "Current Boot" as default journal timestamp range */
    evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_CURRENT_BOOT);

    priv->journal_timestamp_range = evalue->value;

    g_object_notify_by_pspec (G_OBJECT (popover),
                              obj_properties[PROP_JOURNAL_TIMESTAMP_RANGE]);
}

static void
gl_search_popover_class_init (GlSearchPopoverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->get_property = gl_search_popover_get_property;
    gobject_class->set_property = gl_search_popover_set_property;

    obj_properties[PROP_JOURNAL_SEARCH_FIELD] = g_param_spec_enum ("journal-search-field", "Journal Search Field",
                                                                    "The Journal search field by which to filter the logs",
                                                                    GL_TYPE_SEARCH_POPOVER_JOURNAL_FIELD_FILTER,
                                                                    GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_ALL_AVAILABLE_FIELDS,
                                                                    G_PARAM_READWRITE |
                                                                    G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_SEARCH_TYPE] = g_param_spec_enum ("search-type", "Search Type",
                                                          "Do exact or substring search",
                                                          GL_TYPE_QUERY_SEARCH_TYPE,
                                                          GL_QUERY_SEARCH_TYPE_SUBSTRING,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS);

    obj_properties[PROP_JOURNAL_TIMESTAMP_RANGE] = g_param_spec_enum ("journal-timestamp-range", "Journal Timestamp Range",
                                                                      "The Timestamp range of the logs to be shown",
                                                                      GL_TYPE_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE,
                                                                      GL_SEARCH_POPOVER_JOURNAL_TIMESTAMP_RANGE_CURRENT_BOOT,
                                                                      G_PARAM_READWRITE |
                                                                      G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPERTIES,
                                       obj_properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Logs/gl-searchpopover.ui");
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_button_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_label_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_treeview);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_liststore);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  search_type_revealer);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_label_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_treeview);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_button_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  clear_range_button);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_button_drop_down_image);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_liststore);


    gtk_widget_class_bind_template_callback (widget_class,
                                             search_popover_closed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             select_parameter_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_parameter_treeview_row_activated);
    gtk_widget_class_bind_template_callback (widget_class,
                                             search_type_changed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             select_range_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_range_treeview_row_activated);
    gtk_widget_class_bind_template_callback (widget_class,
                                             clear_range_button_clicked);
}

static void
gl_search_popover_init (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    gtk_widget_init_template (GTK_WIDGET (popover));

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->parameter_treeview),
                                          (GtkTreeViewRowSeparatorFunc) parameter_treeview_row_seperator,
                                          popover,
                                          NULL);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->range_treeview),
                                          (GtkTreeViewRowSeparatorFunc) range_treeview_row_seperator,
                                          popover,
                                          NULL);
}

GtkWidget *
gl_search_popover_new (void)
{
    return g_object_new (GL_TYPE_SEARCH_POPOVER, NULL);
}
