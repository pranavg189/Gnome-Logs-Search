/*
 *  GNOME Logs - View and search logs
 *  Copyright (C) 2013  Red Hat, Inc.
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

#include "gl-eventviewdetail.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#include "gl-enums.h"

enum
{
    PROP_0,
    PROP_ENTRY,
    N_PROPERTIES
};

struct _GlEventViewDetail
{
    /*< private >*/
    GtkPopover parent_instance;
};

typedef struct
{
    GlJournalEntry *entry;
    GtkWidget *grid;
    GtkWidget *comm_label;
    GtkWidget *message_label;
    GtkWidget *audit_label;
    GtkWidget *priority_label;
    GtkWidget *comm_field_label;
    GtkWidget *audit_field_label;
    GtkWidget *priority_field_label;

} GlEventViewDetailPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GlEventViewDetail, gl_event_view_detail, GTK_TYPE_POPOVER)

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static gchar *
gl_event_view_detail_replace_newline (const gchar *message)
{
    GString *newmessage;
    const gchar *newline_index;
    const gchar *prevpos;

    newmessage = g_string_sized_new (strlen (message));
    prevpos = message;

    newline_index = strchr (message, '\n');

    while (newline_index != NULL)
    {
        g_string_append_len (newmessage, prevpos, newline_index - prevpos);
        g_string_append_unichar (newmessage, 0x2424);

        prevpos = newline_index + 1;
        newline_index = strchr (prevpos, '\n');
    }

    g_string_append (newmessage, prevpos);

    return g_string_free (newmessage, FALSE);
}

static void
gl_event_view_detail_create_detail (GlEventViewDetail *detail)
{
    GlEventViewDetailPrivate *priv;
    GlJournalEntry *entry;
    gchar *str;

    priv = gl_event_view_detail_get_instance_private (detail);

    entry = priv->entry;

    /* Force LTR direction also for RTL languages */
    gtk_widget_set_direction (priv->grid, GTK_TEXT_DIR_LTR);
    gtk_widget_set_direction (priv->comm_label, GTK_TEXT_DIR_LTR);
    gtk_widget_set_direction (priv->message_label, GTK_TEXT_DIR_LTR);
    gtk_widget_set_direction (priv->audit_label, GTK_TEXT_DIR_LTR);
    gtk_widget_set_direction (priv->priority_label, GTK_TEXT_DIR_LTR);

    if (gl_journal_entry_get_command_line (entry))
    {
        gtk_label_set_text (GTK_LABEL (priv->comm_label), gl_journal_entry_get_command_line (entry));
        gtk_widget_show (priv->comm_field_label);
        gtk_widget_show (priv->comm_label);
    }

    /* Handle messages containing newline such as coredumps or stacktrace */
    // const gchar *message;
    // gchar *newline_index;

    // message = gl_journal_entry_get_message (entry);

    // newline_index = strchr (message, '\n');

    // if (newline_index)
    // {
    //     gchar *message_label;

    //     message_label = gl_event_view_detail_replace_newline (message);

    //     gtk_label_set_text (GTK_LABEL (priv->message_label), message_label);

    //     g_free (message_label);
    // }
    // else
    // {
    //     gtk_label_set_text (GTK_LABEL (priv->message_label), message);
    // }

    /* Don't handle newlines */
    const gchar *message;
    message = gl_journal_entry_get_message (entry);
    gtk_label_set_text (GTK_LABEL (priv->message_label), message);


    //gtk_label_set_line_wrap (GTK_LABEL (priv->message_label), TRUE);
    //gtk_widget_set_size_request (priv->message_label, 10, -1);


    if (gl_journal_entry_get_audit_session (entry))
    {
        gtk_label_set_text (GTK_LABEL (priv->audit_label), gl_journal_entry_get_audit_session (entry));
        gtk_widget_show (priv->audit_field_label);
        gtk_widget_show (priv->audit_label);
    }

    /* TODO: Give a user-friendly representation of the priority. */
    str = g_strdup_printf ("%d", gl_journal_entry_get_priority (entry));
    gtk_label_set_text (GTK_LABEL (priv->priority_label), str);
    g_free (str);
}

static void
gl_event_view_detail_finalize (GObject *object)
{
    GlEventViewDetail *detail = GL_EVENT_VIEW_DETAIL (object);
    GlEventViewDetailPrivate *priv = gl_event_view_detail_get_instance_private (detail);

    g_clear_object (&priv->entry);
}

static void
gl_event_view_detail_get_property (GObject *object,
                                   guint prop_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
    GlEventViewDetail *detail = GL_EVENT_VIEW_DETAIL (object);
    GlEventViewDetailPrivate *priv = gl_event_view_detail_get_instance_private (detail);

    switch (prop_id)
    {
        case PROP_ENTRY:
            g_value_set_object (value, priv->entry);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gl_event_view_detail_set_property (GObject *object,
                                   guint prop_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
    GlEventViewDetail *detail = GL_EVENT_VIEW_DETAIL (object);
    GlEventViewDetailPrivate *priv = gl_event_view_detail_get_instance_private (detail);

    switch (prop_id)
    {
        case PROP_ENTRY:
            priv->entry = g_value_dup_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gl_event_view_detail_constructed (GObject *object)
{
    GlEventViewDetail *detail = GL_EVENT_VIEW_DETAIL (object);

    /* contruct-only properties have already been set. */
    gl_event_view_detail_create_detail (detail);

    G_OBJECT_CLASS (gl_event_view_detail_parent_class)->constructed (object);
}

static void
gl_event_view_detail_class_init (GlEventViewDetailClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->constructed = gl_event_view_detail_constructed;
    gobject_class->finalize = gl_event_view_detail_finalize;
    gobject_class->get_property = gl_event_view_detail_get_property;
    gobject_class->set_property = gl_event_view_detail_set_property;

    obj_properties[PROP_ENTRY] = g_param_spec_object ("entry", "Entry",
                                                      "Journal entry for this detailed view",
                                                      GL_TYPE_JOURNAL_ENTRY,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPERTIES,
                                       obj_properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Logs/gl-eventviewdetail.ui");
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  grid);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  comm_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  message_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  audit_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  priority_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  comm_field_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  audit_field_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlEventViewDetail,
                                                  priority_field_label);
}

static void
gl_event_view_detail_init (GlEventViewDetail *detail)
{
    gtk_widget_init_template (GTK_WIDGET (detail));
    //gtk_widget_set_size_request(GTK_WIDGET(detail), 10, 10);
}

GtkWidget *
gl_event_view_detail_new (GlJournalEntry *entry)
{
    return g_object_new (GL_TYPE_EVENT_VIEW_DETAIL, "entry", entry, NULL);
}
