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

#ifndef GL_SEARCH_POPOVER_H_
#define GL_SEARCH_POPOVER_H_

#include <gtk/gtk.h>

#include "gl-application.h"
#include "gl-journal.h"

G_BEGIN_DECLS

#define GL_TYPE_SEARCH_POPOVER (gl_search_popover_get_type ())
G_DECLARE_FINAL_TYPE (GlSearchPopover, gl_search_popover, GL, SEARCH_POPOVER, GtkPopover)

GtkWidget * gl_search_popover_new (void);

G_END_DECLS

#endif /* GL_SEARCH_POPOVER_H_ */
