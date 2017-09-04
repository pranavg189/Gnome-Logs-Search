/*
 *  GNOME Logs - View and search logs
 *  Copyright (C) 2015 Ekaterina Gerasimova
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

#include "gl-util.h"
#include "gl-mock-journal.h"
#include "gl-journal-model.h"
#include "gl-journal.h"

static void
util_timestamp_to_display (void)
{
    gsize i;
    GDateTime *now;

    static const struct
    {
        guint64 microsecs;
        GlUtilClockFormat format;
        const gchar *time;
    } times[] =
    {
        /* Test three cases for each format (same day, same year, different
           year */
        { G_GUINT64_CONSTANT (1423486800000000), GL_UTIL_CLOCK_FORMAT_12HR,
          " 1:00 PM" },
        { G_GUINT64_CONSTANT (1423402200000000), GL_UTIL_CLOCK_FORMAT_12HR,
          "Feb  8  1:30 PM" },
        { G_GUINT64_CONSTANT (1391952600000000), GL_UTIL_CLOCK_FORMAT_12HR,
          "Feb  9 2014  1:30 PM" },
        { G_GUINT64_CONSTANT (1423486800000000), GL_UTIL_CLOCK_FORMAT_24HR,
          "13:00" },
        { G_GUINT64_CONSTANT (1423402200000000), GL_UTIL_CLOCK_FORMAT_24HR,
          "Feb  8 13:30" },
        { G_GUINT64_CONSTANT (1391952600000000), GL_UTIL_CLOCK_FORMAT_24HR,
          "Feb  9 2014 13:30" }
    };

    now = g_date_time_new_utc (2015, 2, 9, 13, 30, 42);

    for (i = 0; i < G_N_ELEMENTS (times); i++)
    {
        gchar *compare;

        compare = gl_util_timestamp_to_display (times[i].microsecs, now,
                                                times[i].format, FALSE);
        g_assert_cmpstr (compare, ==, times[i].time);
        g_free (compare);
    }

    g_date_time_unref (now);
}

static void
check_log_message (void)
{
   GlMockJournal *journal = gl_mock_journal_new();
   GlMockJournalEntry *entry = gl_mock_journal_previous(journal);
   const gchar *mystring = gl_mock_journal_entry_get_message(entry);
   g_assert_cmpstr(mystring, ==, "This is a test");
}

static void
model_check_message_similarity (void)
{
    GlJournalEntry *entry;
    GlJournalEntry *prev_entry;
    GlRowEntry *current_row_entry;
    GlRowEntry *prev_row_entry;
    gboolean result;

    /* Test for similar message */
    entry = gl_journal_entry_new ("Test message", "systemd");
    prev_entry = gl_journal_entry_new ("Test message", "systemd");

    current_row_entry = gl_row_entry_test_new (entry);
    prev_row_entry = gl_row_entry_test_new (prev_entry);

    result = gl_row_entry_check_message_similarity (current_row_entry, prev_row_entry);

    g_assert_true(result);

    /* Test for similar first word */
    entry = gl_journal_entry_new ("First word", "systemd");
    prev_entry = gl_journal_entry_new ("First message", "dbus");

    current_row_entry = gl_row_entry_test_new (entry);
    prev_row_entry = gl_row_entry_test_new (prev_entry);

    result = gl_row_entry_check_message_similarity (current_row_entry, prev_row_entry);

    g_assert_true (result);

    /* Test for similar senders */
    entry = gl_journal_entry_new ("ABC", "systemd");
    prev_entry = gl_journal_entry_new ("XYZ", "systemd");

    current_row_entry = gl_row_entry_test_new (entry);
    prev_row_entry = gl_row_entry_test_new (prev_entry);

    result = gl_row_entry_check_message_similarity (current_row_entry, prev_row_entry);

    g_assert_true (result);

    /* Test for dissmilar message and sender */
    entry = gl_journal_entry_new ("ABC", "systemd");
    prev_entry = gl_journal_entry_new ("XYZ", "dbus");

    current_row_entry = gl_row_entry_test_new (entry);
    prev_row_entry = gl_row_entry_test_new (prev_entry);

    result = gl_row_entry_check_message_similarity (current_row_entry, prev_row_entry);

    g_assert_false (result);

    /* Test for similar message and null senders */
    entry = gl_journal_entry_new ("ABC", NULL);
    prev_entry = gl_journal_entry_new ("ABC", NULL);

    current_row_entry = gl_row_entry_test_new (entry);
    prev_row_entry = gl_row_entry_test_new (prev_entry);

    result = gl_row_entry_check_message_similarity (current_row_entry, prev_row_entry);

    g_assert_true (result);
}

int
main (int argc, char** argv)
{
    gtk_init(&argc, &argv);

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/util/timestamp_to_display", util_timestamp_to_display);
    g_test_add_func ("/util/check_log_message", check_log_message);

    g_test_add_func ("/model/check_message_similarity", model_check_message_similarity);

    return g_test_run ();
}
