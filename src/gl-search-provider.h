#ifndef GL_SEARCH_PROVIDER_H
#define GL_SEARCH_PROVIDER_H

#include <gio/gio.h>

#define GL_TYPE_SEARCH_PROVIDER gl_search_provider_get_type()
G_DECLARE_FINAL_TYPE (GlSearchProvider, gl_search_provider, GL, SEARCH_PROVIDER, GObject)

GlSearchProvider *        gl_search_provider_new                            (void);

gboolean                  gl_search_provider_register                       (GlSearchProvider *self,
                                                                             GDBusConnection *connection,
                                                                             GError **error);

void                      gl_search_provider_unregister                     (GlSearchProvider *self);

#endif