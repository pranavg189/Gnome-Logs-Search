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

#include "gl-search-popover.h"

#include <glib/gi18n.h>
#include <glib-unix.h>

#include <gl-journal-model.h>

struct _GlSearchPopover
{
    /*< private >*/
    GtkPopover parent_instance;
};

typedef struct
{
    GtkWidget *parameter_stack;
    GtkWidget *parameter_label_stack;
    GtkWidget *parameter_listbox;
    GtkWidget *parameter_button_label;
    GtkWidget *exact_radio_button;
} GlSearchPopoverPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GlSearchPopover, gl_search_popover, GTK_TYPE_POPOVER)

struct {
    gchar *name;
    gchar *value;
} parameter_groups[] = {
    {
        N_("All Available Fields"),
        (NULL)
    },
    {
        N_("PID"),
        ("_PID")
    },
    {
        N_("Message"),
        ("MESSAGE")
    },
    {
        N_("Process Name"),
        ("_COMM")
    },
    {
        N_("Systemd Unit"),
        ("_SYSTEMD_UNIT")
    }
};

enum {
  PARAMETER_CHANGED,
  SEARCH_TYPE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Function to get the entries to be filled in the parameter listbox */
static gint
parameter_get_number_of_groups (void)
{
    return G_N_ELEMENTS (parameter_groups);
}

static const gchar*
parameter_group_get_name (gint group_index)
{
    g_return_val_if_fail (group_index < G_N_ELEMENTS (parameter_groups), NULL);

    return gettext (parameter_groups[group_index].name);
}

/* Header function to draw seperator in listbox */
static void
parameter_listbox_header_func (GtkListBoxRow         *row,
                               GtkListBoxRow         *before,
                               gpointer              user_data)
{
    gboolean show_separator;

    show_separator = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "show-separator"));

    if (show_separator)
    {
        GtkWidget *separator;

        separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_show (separator);

        gtk_list_box_row_set_header (row, separator);
    }
}

/* Function to create a row in the popover listbox */
static GtkWidget*
create_row_for_label (const gchar *text,
                      gboolean     show_separator)
{
    GtkWidget *row;
    GtkWidget *label;

    row = gtk_list_box_row_new ();

    g_object_set_data (G_OBJECT (row), "show-separator", GINT_TO_POINTER (show_separator));

    label = g_object_new (GTK_TYPE_LABEL,
                          "label", text,
                          "hexpand", TRUE,
                          "xalign", 0.0,
                          "margin-start", 6,
                          NULL);

    gtk_container_add (GTK_CONTAINER (row), label);
    gtk_widget_show_all (row);

  return row;
}

/* Fill the entries in the parameter listbox */
static void
fill_parameter_listbox (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;
    GtkWidget *row;
    int i;
    gint n_groups;

    priv = gl_search_popover_get_instance_private (popover);

    n_groups = parameter_get_number_of_groups ();

    /* Parameters */
    for (i = 0; i < n_groups; i++)
    {
        row = create_row_for_label (parameter_group_get_name (i), i == 1);
        g_object_set_data (G_OBJECT (row), "parameter-group-index", GINT_TO_POINTER (i));

        gtk_container_add (GTK_CONTAINER (priv->parameter_listbox), row);
    }
}

/* Event Handlers */
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

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "select-parameter-label");
}

static void
parameter_listbox_row_activated (GtkListBox *box,
                                 GtkListBoxRow *row,
                                 gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gint parameter_group;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    parameter_group = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "parameter-group-index"));

    gtk_label_set_label (GTK_LABEL (priv->parameter_button_label),
                         parameter_group_get_name (parameter_group));

    g_signal_emit_by_name (popover, "parameter-changed", parameter_group);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");
}

static void
gl_search_popover_search_type_changed (GtkToggleButton *togglebutton,
                                       gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;

    popover = GL_SEARCH_POPOVER (user_data);
    priv = gl_search_popover_get_instance_private (popover);

    if ( gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(priv->exact_radio_button)))
    {
        g_signal_emit_by_name (popover, "search-type-changed", 0);
    }
    else
    {
        g_signal_emit_by_name (popover, "search-type-changed", 1);
    }
}

static void
gl_search_popover_finalize (GObject *object)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (object);
    GlSearchPopoverPrivate *priv = gl_search_popover_get_instance_private (popover);
}

static void
gl_search_popover_class_init (GlSearchPopoverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->finalize = gl_search_popover_finalize;

    signals[PARAMETER_CHANGED] = g_signal_new ("parameter-changed",
                                       GL_TYPE_SEARCH_POPOVER,
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL,
                                       NULL,
                                       g_cclosure_marshal_generic,
                                       G_TYPE_NONE,
                                       1,
                                       G_TYPE_INT);

    signals[SEARCH_TYPE_CHANGED] = g_signal_new ("search-type-changed",
                                                 GL_TYPE_SEARCH_POPOVER,
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL,
                                                 NULL,
                                                 g_cclosure_marshal_generic,
                                                 G_TYPE_NONE,
                                                 1,
                                                 G_TYPE_INT);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Logs/gl-search-popover.ui");
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_label_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_listbox);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_button_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  exact_radio_button);

    gtk_widget_class_bind_template_callback (widget_class,
                                             search_popover_closed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             select_parameter_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class,
                                             parameter_listbox_row_activated);
}

static void
gl_search_popover_init (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (popover);

    gtk_widget_init_template (GTK_WIDGET (popover));

    /* Set up header function for parameter listbox */
    gtk_list_box_set_header_func (GTK_LIST_BOX (priv->parameter_listbox),
                                  (GtkListBoxUpdateHeaderFunc) parameter_listbox_header_func,
                                  NULL,
                                  NULL);

    g_signal_connect (GTK_TOGGLE_BUTTON (priv->exact_radio_button), "toggled",
                      G_CALLBACK (gl_search_popover_search_type_changed), popover);

    /* Fill the listbox */
    fill_parameter_listbox (popover);
}

GtkWidget *
gl_search_popover_new (void)
{
    return g_object_new (GL_TYPE_SEARCH_POPOVER, NULL);
}