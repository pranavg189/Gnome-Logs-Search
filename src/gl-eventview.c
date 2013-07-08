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

#include "gl-eventview.h"

#include <glib/gi18n.h>
#include <systemd/sd-journal.h>

G_DEFINE_TYPE (GlEventView, gl_event_view, GTK_TYPE_LIST_BOX)

static void
gl_event_view_class_init (GlEventViewClass *klass)
{
}

static void
gl_event_view_init (GlEventView *view)
{
    GtkWidget *listbox;
    sd_journal *journal;
    gint ret;
    gsize i;
    GtkWidget *scrolled;

    listbox = GTK_WIDGET (view);
    ret = sd_journal_open (&journal, 0);

    if (ret < 0)
    {
        g_warning ("Error opening systemd journal: %s", g_strerror (-ret));
    }

    ret = sd_journal_seek_tail (journal);

    if (ret < 0)
    {
        g_warning ("Error seeking to end of systemd journal: %s",
                   g_strerror (-ret));
    }

    ret = sd_journal_next (journal);

    if (ret < 0)
    {
        g_warning ("Error setting cursor to end of systemd journal: %s",
                   g_strerror (-ret));
    }

    for (i = 0; i < 10; i++)
    {
        const gchar *data;
        gsize length;
        GtkWidget *label;

        ret = sd_journal_get_data (journal, "MESSAGE", (const void **)&data,
                                   &length);

        if (ret < 0)
        {
            g_warning ("Error getting data from systemd journal: %s",
                       g_strerror (-ret));
            break;
        }

        label = gtk_label_new (strchr (data, '=') + 1);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_container_add (GTK_CONTAINER (listbox), label);

        ret = sd_journal_previous (journal);

        if (ret < 0)
        {
            g_warning ("Error setting cursor to previous systemd journal entry %s",
                       g_strerror (-ret));
            break;
        }
    }

    sd_journal_close (journal);
}

GtkWidget *
gl_event_view_new (void)
{
    return g_object_new (GL_TYPE_EVENT_VIEW, NULL);
}
