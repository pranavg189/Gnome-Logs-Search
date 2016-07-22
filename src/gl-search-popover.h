#ifndef GL_SEARCH_POPOVER_H_
#define GL_SEARCH_POPOVER_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Rows in parameter treeview */
typedef enum
{
    GL_PARAMETER_GROUP_ALL_AVAILABLE_FIELDS,
    GL_PARAMETER_GROUP_PID = 2, /* Row 1 is a separator in parameter liststore */
    GL_PARAMETER_GROUP_UID,
    GL_PARAMETER_GROUP_GID,
    GL_PARAMETER_GROUP_MESSAGE,
    GL_PARAMETER_GROUP_PROCESS_NAME,
    GL_PARAMETER_GROUP_SYSTEMD_UNIT,
    GL_PARAMETER_GROUP_KERNEL_DEVICE,
    GL_PARAMETER_GROUP_AUDIT_SESSION,
    GL_PARAMETER_GROUP_EXECUTABLE_PATH
} GlParameterGroups;

#define GL_TYPE_SEARCH_POPOVER (gl_search_popover_get_type ())
G_DECLARE_FINAL_TYPE (GlSearchPopover, gl_search_popover, GL, SEARCH_POPOVER, GtkPopover)

GtkWidget * gl_search_popover_new (void);

G_END_DECLS

#endif /* GL_SEARCH_POPOVER_H_ */
