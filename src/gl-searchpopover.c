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

    GlSearchPopoverJournalFieldFilter journal_search_field;
    GlQuerySearchType search_type;
} GlSearchPopoverPrivate;

enum
{
    PROP_0,
    PROP_JOURNAL_SEARCH_FIELD,
    PROP_SEARCH_TYPE,
    N_PROPERTIES
};

enum
{
    COLUMN_JOURNAL_FIELD_LABEL,
    COLUMN_JOURNAL_FIELD_SHOW_SEPARATOR,
    COLUMN_JOURNAL_FIELD_ENUM_NAME,
    JOURNAL_FIELD_N_COLUMNS
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
}

static void
select_parameter_button_clicked (GtkButton *button,
                                 gpointer user_data)
{
    GlSearchPopoverPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GEnumClass *eclass;
    GEnumValue *evalue;
    gboolean valid;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "select-parameter-label");

    eclass = g_type_class_ref (GL_TYPE_SEARCH_POPOVER_JOURNAL_FIELD_FILTER);

    evalue = g_enum_get_value (eclass, priv->journal_search_field);

    model = GTK_TREE_MODEL (priv->parameter_liststore);

    valid = gtk_tree_model_get_iter_first (model, &iter);

    while (valid)
    {
        gchar *journal_field_enum_name;

        gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), &iter,
                            COLUMN_JOURNAL_FIELD_ENUM_NAME, &journal_field_enum_name,
                            -1);

        if (g_strcmp0 (evalue->value_name, journal_field_enum_name) == 0)
        {
            g_free (journal_field_enum_name);
            break;
        }

        valid = gtk_tree_model_iter_next (model, &iter);

        g_free (journal_field_enum_name);
    }

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->parameter_treeview));

    gtk_tree_selection_select_iter (selection, &iter);

    g_type_class_unref (eclass);
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
                        COLUMN_JOURNAL_FIELD_SHOW_SEPARATOR, &show_separator,
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
    gchar *journal_field_label;
    gchar *journal_field_enum_name;
    GEnumClass *eclass;
    GEnumValue *evalue;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->parameter_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), &iter,
                        COLUMN_JOURNAL_FIELD_LABEL, &journal_field_label,
                        COLUMN_JOURNAL_FIELD_ENUM_NAME, &journal_field_enum_name,
                        -1);

    gtk_label_set_label (GTK_LABEL (priv->parameter_button_label),
                         _(journal_field_label));

    eclass = g_type_class_ref (GL_TYPE_SEARCH_POPOVER_JOURNAL_FIELD_FILTER);

    if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_ALL_AVAILABLE_FIELDS", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_ALL_AVAILABLE_FIELDS);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PID", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PID);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_UID", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_UID);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_GID", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_GID);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_MESSAGE", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_MESSAGE);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PROCESS_NAME", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_PROCESS_NAME);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_SYSTEMD_UNIT", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_SYSTEMD_UNIT);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_KERNEL_DEVICE", journal_field_enum_name))
    {
        evalue = g_enum_get_value (eclass, GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_KERNEL_DEVICE);
    }
    else if (strstr ("GL_SEARCH_POPOVER_JOURNAL_FIELD_FILTER_AUDIT_SESSION", journal_field_enum_name))
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

    g_free (journal_field_label);
    g_free (journal_field_enum_name);
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
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
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


    gtk_widget_class_bind_template_callback (widget_class,
                                             search_popover_closed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             select_parameter_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_parameter_treeview_row_activated);
    gtk_widget_class_bind_template_callback (widget_class,
                                             search_type_changed);
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
}

GtkWidget *
gl_search_popover_new (void)
{
    return g_object_new (GL_TYPE_SEARCH_POPOVER, NULL);
}
