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

#ifndef GL_SEARCH_PROVIDER_H
#define GL_SEARCH_PROVIDER_H

#include <gio/gio.h>

#define GL_TYPE_SEARCH_PROVIDER gl_search_provider_get_type ()
G_DECLARE_FINAL_TYPE (GlSearchProvider, gl_search_provider, GL, SEARCH_PROVIDER, GObject)

GlSearchProvider *        gl_search_provider_new                            (void);

gboolean                  gl_search_provider_register                       (GlSearchProvider *self,
                                                                             GDBusConnection *connection,
                                                                             GError **error);

void                      gl_search_provider_unregister                     (GlSearchProvider *self);

#endif
