
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

#ifndef GL_SHELL_SEARCH_PROVIDER_H
#define GL_SHELL_SEARCH_PROVIDER_H

#include <gio/gio.h>

#include "gl-journal-model.h"

#define GL_TYPE_SHELL_SEARCH_PROVIDER gl_shell_search_provider_get_type ()
G_DECLARE_FINAL_TYPE (GlShellSearchProvider, gl_shell_search_provider, GL, SHELL_SEARCH_PROVIDER, GObject)

GlShellSearchProvider *   gl_shell_search_provider_new                      (void);

gboolean                  gl_shell_search_provider_register                 (GlShellSearchProvider *self,
                                                                             GDBusConnection *connection,
                                                                             GError **error);

void                      gl_shell_search_provider_unregister               (GlShellSearchProvider *self);

#endif
